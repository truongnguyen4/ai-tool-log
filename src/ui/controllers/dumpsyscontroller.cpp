#include "dumpsyscontroller.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "colorscheme.h"
#include "widgetstyling.h"
#include "components/components.h"

#include <QFileDialog>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QStatusBar>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QCompleter>
#include <QComboBox>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

DumpsysController::DumpsysController(Ui::MainWindow *ui,
                                     QStatusBar *statusBar,
                                     DeviceIdProvider deviceIdProvider,
                                     QObject *parent)
    : QObject(parent)
    , m_ui(ui)
    , m_statusBar(statusBar)
    , m_deviceIdProvider(std::move(deviceIdProvider))
{}

void DumpsysController::setup()
{
    QCompleter *completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setModel(new QStringListModel(QStringList(), completer));
    WidgetStyling::styleCompleterPopup(completer->popup());
    m_ui->txtDumpsysService->setCompleter(completer);

    connect(completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &text) {
        m_ui->txtDumpsysService->setText(text);
        onRunDumpsysClicked();
    });

    connect(m_ui->txtDumpsysService,    &QLineEdit::returnPressed,  this, &DumpsysController::onRunDumpsysClicked);
    connect(m_ui->txtDumpsysService,    &QLineEdit::textChanged,    this, &DumpsysController::updateDumpsysCommandText);
    connect(m_ui->btnDumpsysRefresh,    &QPushButton::clicked,      this, &DumpsysController::onRunDumpsysClicked);
    connect(m_ui->txtDumpsysCommand,    &QLineEdit::returnPressed,  this, &DumpsysController::onRunDumpsysCmdClicked);
    connect(m_ui->txtPackageFilter,     &QLineEdit::textChanged,    this, &DumpsysController::updateDumpsysCommandText);
    connect(m_ui->txtDumpsysSearch,     &QLineEdit::textChanged,    this, &DumpsysController::onDumpsysSearchChanged);
    connect(m_ui->txtDumpsysSearch,     &QLineEdit::returnPressed,  this, &DumpsysController::onDumpsysSearchNext);
    connect(m_ui->btnDumpsysSearchPrev, &QPushButton::clicked,      this, &DumpsysController::onDumpsysSearchPrev);
    connect(m_ui->btnDumpsysSearchNext, &QPushButton::clicked,      this, &DumpsysController::onDumpsysSearchNext);

    connect(&AdbManager::instance(), &AdbManager::dumpsysListFetched,    this, &DumpsysController::onDumpsysListFetched);
    connect(&AdbManager::instance(), &AdbManager::dumpsysFetched,        this, &DumpsysController::onDumpsysFetched);
    connect(&AdbManager::instance(), &AdbManager::rawAdbCommandFinished, this, &DumpsysController::onRawAdbCommandFinished);

    m_ui->splitterDumpsysOutput->setSizes({750, 250});

    // ── Inline match counter beside the search field ─────────────────────────
    // The search box lives inside `horizontalLayout_dumpsysService`. We locate
    // that layout via the UI accessor directly rather than walking parents
    // (search box's parentWidget() returns dumpsysControlsContainer whose
    // own layout is a QVBoxLayout — the wrong one).
    if (auto *svcRow = m_ui->horizontalLayout_dumpsysService) {
        m_matchLabel = new QLabel(m_ui->dumpsysControlsContainer);
        m_matchLabel->setObjectName(QStringLiteral("lblDumpsysMatchCount"));
        m_matchLabel->setProperty("role", QStringLiteral("caption"));
        m_matchLabel->setMinimumWidth(72);
        m_matchLabel->setText(QString());
        const int idx = svcRow->indexOf(m_ui->txtDumpsysSearch);
        if (idx >= 0)
            svcRow->insertWidget(idx + 1, m_matchLabel);
        else
            svcRow->addWidget(m_matchLabel);
    }

    // ── Toolbar (Save / Snapshot / Diff / Monitor) in header row ──
    if (auto *headerLay = m_ui->horizontalLayout_dumpsysHeader) {
        using namespace UiComponents;
        QWidget *parent = m_ui->dumpsysControlsContainer;
        m_btnSave     = Button::make(tr("Save"),     ButtonVariant::Ghost,     parent, ButtonSize::Small);
        m_btnSnapshot = Button::make(tr("Snapshot"), ButtonVariant::Secondary, parent, ButtonSize::Small);
        m_btnDiff     = Button::make(tr("Diff"),     ButtonVariant::Secondary, parent, ButtonSize::Small);
        m_btnMonitor  = Button::make(tr("Monitor"),  ButtonVariant::Secondary, parent, ButtonSize::Small);
        m_btnDiff   ->setCheckable(true);
        m_btnDiff   ->setEnabled(false);
        m_btnMonitor->setCheckable(true);
        m_btnMonitor->setProperty("role", QStringLiteral("monitor"));
        m_btnMonitor->style()->unpolish(m_btnMonitor);
        m_btnMonitor->style()->polish(m_btnMonitor);
        m_btnSave    ->setToolTip(tr("Save output to file"));
        m_btnSnapshot->setToolTip(tr("Snapshot current output (per service)"));
        m_btnDiff    ->setToolTip(tr("Diff current output against last snapshot"));
        m_btnMonitor ->setToolTip(tr("Re-fetch this dumpsys service every 500 ms"));
        for (QPushButton *b : { m_btnSave, m_btnSnapshot, m_btnDiff, m_btnMonitor })
            headerLay->addWidget(b);

        // Tick-rate selector right after the Monitor button.
        m_monitorIntervalCombo = new QComboBox(parent);
        m_monitorIntervalCombo->setToolTip(tr("Monitor tick interval"));
        m_monitorIntervalCombo->addItem(tr("250 ms"),  250);
        m_monitorIntervalCombo->addItem(tr("500 ms"),  500);
        m_monitorIntervalCombo->addItem(tr("1000 ms"), 1000);
        m_monitorIntervalCombo->addItem(tr("2000 ms"), 2000);
        m_monitorIntervalCombo->setCurrentIndex(1); // default 500 ms
        headerLay->addWidget(m_monitorIntervalCombo);

        connect(m_btnSave,     &QPushButton::clicked, this, &DumpsysController::onSaveOutputClicked);
        connect(m_btnSnapshot, &QPushButton::clicked, this, &DumpsysController::onSnapshotClicked);
        connect(m_btnDiff,     &QPushButton::toggled, this, &DumpsysController::onDiffToggled);
        connect(m_btnMonitor,  &QPushButton::toggled, this, &DumpsysController::onMonitorToggled);
        connect(m_monitorIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
            if (!m_monitorTimer) return;
            const int ms = m_monitorIntervalCombo->currentData().toInt();
            m_monitorTimer->setInterval(ms);
            if (m_monitoring) { m_monitorTimer->stop(); m_monitorTimer->start(); }
        });
    }

    // 500 ms re-fetch timer for monitor mode — actual interval driven by combo.
    m_monitorTimer = new QTimer(this);
    m_monitorTimer->setInterval(500);
    connect(m_monitorTimer, &QTimer::timeout, this, [this]() {
        // AdbManager coalesces overlapping dumpsys requests, so monitor ticks
        // keep the freshest requested service without stacking adb processes.
        onRunDumpsysClicked();
    });

    // ── Preset chips row (common dumpsys services) ───────────────────────────
    if (auto *outerLay = m_ui->verticalLayout_dumpsysControls) {
        auto *chipsRow = new QHBoxLayout;
        chipsRow->setSpacing(6);
        chipsRow->setContentsMargins(0, 2, 0, 2);
        auto *chipsLbl = new QLabel(tr("Presets:"), m_ui->dumpsysControlsContainer);
        chipsLbl->setProperty("role", QStringLiteral("caption"));
        chipsRow->addWidget(chipsLbl);
        const QStringList presets = {
            QStringLiteral("meminfo"), QStringLiteral("battery"),
            QStringLiteral("activity"), QStringLiteral("cpuinfo"),
            QStringLiteral("gfxinfo"), QStringLiteral("package"),
            QStringLiteral("wifi"), QStringLiteral("power"),
            QStringLiteral("display"), QStringLiteral("procstats"),
            QStringLiteral("window"), QStringLiteral("input"),
        };
        for (const QString &svc : presets) {
            auto *chip = UiComponents::Button::make(
                svc, UiComponents::ButtonVariant::Ghost,
                m_ui->dumpsysControlsContainer, UiComponents::ButtonSize::Small);
            chip->setProperty("role", QStringLiteral("chip"));
            connect(chip, &QPushButton::clicked, this, [this, svc]() { onPresetClicked(svc); });
            chipsRow->addWidget(chip);
        }
        chipsRow->addStretch();
        outerLay->addLayout(chipsRow);
    }
}

void DumpsysController::restoreLastService(const QString &deviceId)
{
    if (deviceId.isEmpty()) return;
    QSettings s;
    const QString svc = s.value(QStringLiteral("dumpsys/lastService/") + deviceId).toString();
    if (!svc.isEmpty()) {
        m_ui->txtDumpsysService->setText(svc);
    }
}

void DumpsysController::updateDumpsysCommandText()
{
    const QString deviceId = m_deviceIdProvider();
    const QString service  = m_ui->txtDumpsysService->text().trimmed();
    const QString pkg      = m_ui->txtPackageFilter->text().trimmed();

    if (deviceId.isEmpty() && service.isEmpty() && pkg.isEmpty()) {
        m_ui->txtDumpsysCommand->clear();
        return;
    }

    const QString device = deviceId.isEmpty() ? QStringLiteral("<device-id>") : deviceId;

    QString args = currentDumpsysArgs();
    if (args.isEmpty())
        args = QStringLiteral("<package-name>");

    m_ui->txtDumpsysCommand->setText(
        QStringLiteral("adb -s ") + device + QStringLiteral(" shell dumpsys ") + args);
}

void DumpsysController::onRunDumpsysClicked()
{
    // Refresh always returns to the raw output view — exit diff mode so the
    // user sees the freshly-fetched dumpsys, not an old diff overlay.
    // Exception: while monitoring, leave the diff toggle alone so the user
    // can keep watching changes against their snapshot.
    if (!m_monitoring && m_btnDiff && m_btnDiff->isChecked())
        m_btnDiff->setChecked(false);

    const QString deviceId = m_deviceIdProvider();
    if (deviceId.isEmpty()) {
        m_statusBar->showMessage("No device selected", 3000);
        return;
    }
    const QString args = currentDumpsysArgs();
    if (args.isEmpty()) {
        m_statusBar->showMessage("No service specified", 3000);
        return;
    }
    m_ui->txtDumpsysCmdResult->setPlainText("…");
    AdbManager::instance().fetchDumpsys(deviceId, args);
}

void DumpsysController::onDumpsysFetched(const QString &output)
{
    m_currentOutput  = output;
    m_currentService = m_ui->txtDumpsysService->text().trimmed();
    renderOutput();

    // Persist last-used service per device for next session.
    const QString deviceId = m_deviceIdProvider();
    if (!deviceId.isEmpty() && !m_currentService.isEmpty()) {
        QSettings s;
        s.setValue(QStringLiteral("dumpsys/lastService/") + deviceId, m_currentService);
    }

    m_statusBar->showMessage(
        QString("Dumpsys: %1 lines").arg(output.count('\n')), 3000);
}

void DumpsysController::applyDumpsysHighlights(const QString &needle)
{
    m_lastSearchNeedle = needle;
    int count = 0;
    if (!needle.isEmpty()) {
        QTextDocument *doc = m_ui->txtDumpsysCmdResult->document();
        QTextCursor cur(doc);
        while (!(cur = doc->find(needle, cur)).isNull())
            ++count;
    }
    if (m_matchLabel) {
        if (needle.isEmpty())
            m_matchLabel->setText(QString());
        else
            m_matchLabel->setText(count == 0 ? tr("0 matches")
                                             : tr("%1 matches").arg(count));
    }
    applyAllExtraSelections();
}

// Combines diff line-highlights (added=green, removed=red) with search
// match highlights so neither stomps the other.
void DumpsysController::applyAllExtraSelections()
{
    QList<QTextEdit::ExtraSelection> extras;
    QTextDocument *doc = m_ui->txtDumpsysCmdResult->document();

    // Diff line highlights (full-line backgrounds).
    auto pushLineSel = [&](int lineNumber, const QColor &bg) {
        QTextBlock blk = doc->findBlockByNumber(lineNumber);
        if (!blk.isValid()) return;
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = QTextCursor(blk);
        extras.append(sel);
    };
    const QColor addedBg = QColor(46, 160, 67, 60);    // green @ ~24% alpha
    const QColor removedBg = QColor(248, 81, 73, 70);  // red   @ ~27% alpha
    for (int ln : m_diffAddedLines)   pushLineSel(ln, addedBg);
    for (int ln : m_diffRemovedLines) pushLineSel(ln, removedBg);

    // Search match highlights on top.
    if (!m_lastSearchNeedle.isEmpty()) {
        QTextCharFormat fmt;
        fmt.setBackground(ColorScheme::instance().highlightBackground());
        fmt.setForeground(ColorScheme::instance().highlightForeground());
        QTextCursor cur(doc);
        while (!(cur = doc->find(m_lastSearchNeedle, cur)).isNull()) {
            QTextEdit::ExtraSelection sel;
            sel.format = fmt;
            sel.cursor = cur;
            extras.append(sel);
        }
    }
    m_ui->txtDumpsysCmdResult->setExtraSelections(extras);
}

void DumpsysController::onDumpsysSearchChanged()
{
    const QString needle = m_ui->txtDumpsysSearch->text();
    applyDumpsysHighlights(needle);
    if (needle.isEmpty()) return;
    QTextCursor c = m_ui->txtDumpsysCmdResult->textCursor();
    c.movePosition(QTextCursor::Start);
    m_ui->txtDumpsysCmdResult->setTextCursor(c);
    m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindFlags());
}

void DumpsysController::onDumpsysSearchNext()
{
    const QString needle = m_ui->txtDumpsysSearch->text();
    if (needle.isEmpty()) return;
    if (!m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindFlags())) {
        QTextCursor c = m_ui->txtDumpsysCmdResult->textCursor();
        c.movePosition(QTextCursor::Start);
        m_ui->txtDumpsysCmdResult->setTextCursor(c);
        m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindFlags());
    }
}

void DumpsysController::onDumpsysSearchPrev()
{
    const QString needle = m_ui->txtDumpsysSearch->text();
    if (needle.isEmpty()) return;
    if (!m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindBackward)) {
        QTextCursor c = m_ui->txtDumpsysCmdResult->textCursor();
        c.movePosition(QTextCursor::End);
        m_ui->txtDumpsysCmdResult->setTextCursor(c);
        m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindBackward);
    }
}

void DumpsysController::onRunDumpsysCmdClicked()
{
    const QString cmdText = m_ui->txtDumpsysCommand->text().trimmed();
    if (cmdText.isEmpty()) {
        m_statusBar->showMessage("No command specified", 3000);
        return;
    }
    m_ui->txtDumpsysResult->setPlainText("…");
    AdbManager::instance().runRawAdbCommand(cmdText);
}

void DumpsysController::onRawAdbCommandFinished(const QString &output)
{
    m_ui->txtDumpsysResult->setPlainText(output);
    m_statusBar->showMessage(
        QString("Command: %1 lines").arg(output.count('\n')), 3000);
}

void DumpsysController::onDumpsysListFetched(const QStringList &services)
{
    m_dumpsysServices = services;

    QCompleter *completer = m_ui->txtDumpsysService->completer();
    if (completer)
        qobject_cast<QStringListModel *>(completer->model())->setStringList(services);

    m_ui->txtDumpsysService->setPlaceholderText(tr("Service name..."));
    m_statusBar->showMessage(
        QString("Dumpsys: %1 services available").arg(services.size()), 3000);

    if (m_ui->txtDumpsysService->text().trimmed().isEmpty() && !services.isEmpty())
        m_ui->txtDumpsysService->setText(services.first());

    onRunDumpsysClicked();
}

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar actions
// ─────────────────────────────────────────────────────────────────────────────

void DumpsysController::onSaveOutputClicked()
{
    const QString defaultName = (m_currentService.isEmpty() ? QStringLiteral("dumpsys")
                                                            : QStringLiteral("dumpsys-") + m_currentService)
                                + QStringLiteral(".txt");
    const QString path = QFileDialog::getSaveFileName(
        m_ui->txtDumpsysCmdResult->window(),
        tr("Save dumpsys output"), defaultName,
        tr("Text (*.txt);;All files (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_statusBar->showMessage(tr("Save failed: %1").arg(f.errorString()), 4000);
        return;
    }
    f.write(m_ui->txtDumpsysCmdResult->toPlainText().toUtf8());
    f.close();
    m_statusBar->showMessage(tr("Saved %1").arg(path), 3000);
}


// ─────────────────────────────────────────────────────────────────────────────
// Snapshot + diff mode
// ─────────────────────────────────────────────────────────────────────────────

void DumpsysController::onSnapshotClicked()
{
    if (m_currentService.isEmpty() || m_currentOutput.isEmpty()) {
        m_statusBar->showMessage(tr("Nothing to snapshot — run a service first"), 3000);
        return;
    }
    m_snapshots.insert(m_currentService, m_currentOutput);
    m_statusBar->showMessage(
        tr("Snapshot taken for '%1' (%2 lines)")
            .arg(m_currentService).arg(m_currentOutput.count('\n')), 3000);
    if (m_btnDiff) m_btnDiff->setEnabled(true);
}

void DumpsysController::onDiffToggled(bool on)
{
    m_diffMode = on;
    renderOutput();
}

void DumpsysController::onPresetClicked(const QString &service)
{
    m_ui->txtDumpsysService->setText(service);
    onRunDumpsysClicked();
}

void DumpsysController::stopMonitor()
{
    // Toggle the button off; this funnels through onMonitorToggled(false)
    // which stops the timer and updates the label.
    if (m_btnMonitor && m_btnMonitor->isChecked())
        m_btnMonitor->setChecked(false);
}

void DumpsysController::onMonitorToggled(bool on)
{
    m_monitoring = on;
    if (m_btnMonitor)
        m_btnMonitor->setText(on ? tr("\u25CF REC") : tr("Monitor"));
    if (on) {
        const int ms = m_monitorIntervalCombo ? m_monitorIntervalCombo->currentData().toInt() : 500;
        m_monitorTimer->setInterval(ms);
        // Fire one immediate fetch then start the periodic timer.
        onRunDumpsysClicked();
        m_monitorTimer->start();
        m_statusBar->showMessage(tr("Monitoring dumpsys every %1 ms").arg(ms), 2000);
    } else {
        m_monitorTimer->stop();
        m_statusBar->showMessage(tr("Monitor stopped"), 2000);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void DumpsysController::updateMatchCounter()
{
    applyDumpsysHighlights(m_ui->txtDumpsysSearch->text());
}

void DumpsysController::renderOutput()
{
    m_diffAddedLines.clear();
    m_diffRemovedLines.clear();

    QString text = m_currentOutput;
    if (m_diffMode && !m_currentService.isEmpty()
        && m_snapshots.contains(m_currentService))
    {
        text = computeDiff(m_snapshots.value(m_currentService), m_currentOutput);
    }
    m_ui->txtDumpsysCmdResult->setPlainText(text);
    const QString needle = m_ui->txtDumpsysSearch->text();
    applyDumpsysHighlights(needle);
    if (!needle.isEmpty()) {
        QTextCursor c = m_ui->txtDumpsysCmdResult->textCursor();
        c.movePosition(QTextCursor::Start);
        m_ui->txtDumpsysCmdResult->setTextCursor(c);
        m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindFlags());
    }
}

QString DumpsysController::currentDumpsysArgs() const
{
    const QString service = m_ui->txtDumpsysService->text().trimmed();
    const QString package = m_ui->txtPackageFilter->text().trimmed();
    if (package.isEmpty())
        return service;
    return service.isEmpty() ? package : service + QLatin1Char(' ') + package;
}

// Lightweight set-difference diff: emits the new content unchanged (so it
// reads naturally) and records line indices that are NEW vs the snapshot for
// green highlighting. Lines that disappeared since the snapshot are listed
// in a separate trailing section and recorded for red highlighting.
QString DumpsysController::computeDiff(const QString &a, const QString &b)
{
    const QStringList oldLines = a.split('\n');
    const QStringList newLines = b.split('\n');
    const QSet<QString> oldSet(oldLines.begin(), oldLines.end());
    const QSet<QString> newSet(newLines.begin(), newLines.end());

    int added = 0, removed = 0, same = 0;
    for (const QString &line : newLines)
        (oldSet.contains(line) ? same : added)++;
    for (const QString &line : oldLines)
        if (!newSet.contains(line)) ++removed;

    const QString header =
        tr("# diff vs snapshot:  +%1 added (green),  -%2 removed (red),  =%3 unchanged")
            .arg(added).arg(removed).arg(same);

    QString out;
    out.reserve(a.size() + b.size() + 256);
    out += header;
    out += QStringLiteral("\n\n");
    int line = 2; // header + blank

    for (const QString &l : newLines) {
        if (!oldSet.contains(l))
            m_diffAddedLines.append(line);
        out += l;
        out += '\n';
        ++line;
    }

    if (removed > 0) {
        out += '\n';
        ++line;
        const QString sep = tr("# Removed since snapshot:");
        out += sep;
        out += '\n';
        ++line;
        for (const QString &l : oldLines) {
            if (!newSet.contains(l)) {
                m_diffRemovedLines.append(line);
                out += l;
                out += '\n';
                ++line;
            }
        }
    }
    return out;
}
