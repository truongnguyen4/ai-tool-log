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

namespace {

/** Wait for a typing pause before re-highlighting the whole document. */
constexpr int kSearchDebounceMs = 180;
/**
 * Upper bound on painted search hits. A one-character search over a large
 * dump can match hundreds of thousands of times; past a few thousand the
 * highlights convey nothing and only cost memory and paint time. The match
 * counter still reports the true total.
 */
constexpr int kMaxSearchHighlights = 4000;

constexpr int kDefaultMonitorIntervalMs   = 500;
constexpr int kMonitorIntervalsMs[]       = {250, 500, 1000, 2000};
constexpr int kDefaultMonitorIntervalIndex = 1;

constexpr int kPackageInputWidth = 160;
constexpr int kMatchLabelWidth   = 84;
constexpr int kStatusTimeoutMs   = 3000;

/** Services offered as one-click chips. */
constexpr const char *kPresetServices[] = {
    "meminfo", "battery", "activity", "cpuinfo", "gfxinfo", "package",
    "wifi", "power", "display", "procstats", "window", "input",
};

/** Diff line tints; deliberately translucent so text stays readable. */
const QColor kDiffAddedBackground(46, 160, 67, 60);
const QColor kDiffRemovedBackground(248, 81, 73, 70);

/** Number of newline-separated lines in a command's output. */
qsizetype countLines(const QString &text)
{
    return text.count(QLatin1Char('\n'));
}

} // namespace

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

    connect(m_ui->txtDumpsysService,    &QLineEdit::returnPressed, this, &DumpsysController::onRunDumpsysClicked);
    connect(m_ui->txtDumpsysService,    &QLineEdit::textChanged,   this, &DumpsysController::updateDumpsysCommandText);
    connect(m_ui->btnDumpsysRefresh,    &QPushButton::clicked,     this, &DumpsysController::onRunDumpsysClicked);
    connect(m_ui->txtDumpsysCommand,    &QLineEdit::returnPressed, this, &DumpsysController::onRunDumpsysCmdClicked);
    connect(m_ui->txtDumpsysSearch,     &QLineEdit::textChanged,   this, &DumpsysController::onDumpsysSearchChanged);
    connect(m_ui->txtDumpsysSearch,     &QLineEdit::returnPressed, this, &DumpsysController::onDumpsysSearchNext);
    connect(m_ui->btnDumpsysSearchPrev, &QPushButton::clicked,     this, &DumpsysController::onDumpsysSearchPrev);
    connect(m_ui->btnDumpsysSearchNext, &QPushButton::clicked,     this, &DumpsysController::onDumpsysSearchNext);

    AdbManager &adb = AdbManager::instance();
    connect(&adb, &AdbManager::dumpsysListFetched,    this, &DumpsysController::onDumpsysListFetched);
    connect(&adb, &AdbManager::dumpsysFetched,        this, &DumpsysController::onDumpsysFetched);
    connect(&adb, &AdbManager::rawAdbCommandFinished, this, &DumpsysController::onRawAdbCommandFinished);

    m_ui->splitterDumpsysOutput->setSizes({750, 250});

    // Highlighting a search over a multi-megabyte dump is not something to do
    // on every keystroke; wait for the user to pause first.
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(kSearchDebounceMs);
    connect(m_searchDebounce, &QTimer::timeout, this, [this]() {
        refreshExtraSelections();
        if (!m_ui->txtDumpsysSearch->text().isEmpty())
            findInOutput(/*backwards=*/false, /*fromStart=*/true);
    });

    buildToolbar();
    buildPresetChips();

    m_monitorTimer = new QTimer(this);
    m_monitorTimer->setInterval(kDefaultMonitorIntervalMs);
    connect(m_monitorTimer, &QTimer::timeout, this, [this]() {
        // AdbManager coalesces overlapping dumpsys requests, so monitor ticks
        // keep the freshest requested service without stacking adb processes.
        onRunDumpsysClicked();
    });
}

void DumpsysController::buildToolbar()
{
    using namespace UiComponents;
    QWidget *parent = m_ui->dumpsysControlsContainer;

    // ── Package argument + match counter, beside the search field ────────────
    // The search box lives in `horizontalLayout_dumpsysService`; reach it via
    // the UI accessor rather than walking parents (the box's parentWidget()
    // is dumpsysControlsContainer, whose own layout is the wrong one).
    if (auto *serviceRow = m_ui->horizontalLayout_dumpsysService) {
        // A dumpsys-specific package box. This used to reuse the Logcat tab's
        // package *filter*, so typing a log filter silently rewrote the
        // dumpsys command on another tab.
        m_packageInput = Input::make(tr("Package (optional)"), parent);
        m_packageInput->setObjectName(QStringLiteral("txtDumpsysPackage"));
        m_packageInput->setClearButtonEnabled(true);
        m_packageInput->setMaximumWidth(kPackageInputWidth);
        m_packageInput->setToolTip(tr("Extra argument passed after the service name, "
                                      "e.g. a package for 'dumpsys package'."));
        connect(m_packageInput, &QLineEdit::textChanged,
                this, &DumpsysController::updateDumpsysCommandText);
        connect(m_packageInput, &QLineEdit::returnPressed,
                this, &DumpsysController::onRunDumpsysClicked);

        m_matchLabel = new QLabel(parent);
        m_matchLabel->setObjectName(QStringLiteral("lblDumpsysMatchCount"));
        m_matchLabel->setProperty("role", QStringLiteral("caption"));
        m_matchLabel->setMinimumWidth(kMatchLabelWidth);

        const int serviceIdx = serviceRow->indexOf(m_ui->txtDumpsysService);
        if (serviceIdx >= 0)
            serviceRow->insertWidget(serviceIdx + 1, m_packageInput);
        else
            serviceRow->addWidget(m_packageInput);

        const int searchIdx = serviceRow->indexOf(m_ui->txtDumpsysSearch);
        if (searchIdx >= 0)
            serviceRow->insertWidget(searchIdx + 1, m_matchLabel);
        else
            serviceRow->addWidget(m_matchLabel);
    }

    // ── Save / Snapshot / Diff / Monitor in the header row ───────────────────
    auto *headerLayout = m_ui->horizontalLayout_dumpsysHeader;
    if (!headerLayout)
        return;

    m_btnSave     = Button::make(tr("Save"),     ButtonVariant::Ghost,     parent, ButtonSize::Small);
    m_btnSnapshot = Button::make(tr("Snapshot"), ButtonVariant::Secondary, parent, ButtonSize::Small);
    m_btnDiff     = Button::make(tr("Diff"),     ButtonVariant::Secondary, parent, ButtonSize::Small);
    m_btnMonitor  = Button::make(tr("Monitor"),  ButtonVariant::Secondary, parent, ButtonSize::Small);

    m_btnDiff->setCheckable(true);
    m_btnDiff->setEnabled(false);
    m_btnMonitor->setCheckable(true);
    m_btnMonitor->setProperty("role", QStringLiteral("monitor"));
    m_btnMonitor->style()->unpolish(m_btnMonitor);
    m_btnMonitor->style()->polish(m_btnMonitor);

    m_btnSave    ->setToolTip(tr("Save output to file"));
    m_btnSnapshot->setToolTip(tr("Snapshot current output (per service)"));
    m_btnDiff    ->setToolTip(tr("Diff current output against last snapshot"));
    m_btnMonitor ->setToolTip(tr("Re-fetch this dumpsys service on the selected interval"));

    for (QPushButton *button : {m_btnSave, m_btnSnapshot, m_btnDiff, m_btnMonitor})
        headerLayout->addWidget(button);

    m_monitorIntervalCombo = new QComboBox(parent);
    m_monitorIntervalCombo->setToolTip(tr("Monitor tick interval"));
    for (int ms : kMonitorIntervalsMs)
        m_monitorIntervalCombo->addItem(tr("%1 ms").arg(ms), ms);
    m_monitorIntervalCombo->setCurrentIndex(kDefaultMonitorIntervalIndex);
    headerLayout->addWidget(m_monitorIntervalCombo);

    connect(m_btnSave,     &QPushButton::clicked, this, &DumpsysController::onSaveOutputClicked);
    connect(m_btnSnapshot, &QPushButton::clicked, this, &DumpsysController::onSnapshotClicked);
    connect(m_btnDiff,     &QPushButton::toggled, this, &DumpsysController::onDiffToggled);
    connect(m_btnMonitor,  &QPushButton::toggled, this, &DumpsysController::onMonitorToggled);
    connect(m_monitorIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (!m_monitorTimer)
            return;
        m_monitorTimer->setInterval(m_monitorIntervalCombo->currentData().toInt());
        if (m_monitoring) {
            m_monitorTimer->stop();
            m_monitorTimer->start();
        }
    });
}

void DumpsysController::buildPresetChips()
{
    auto *outerLayout = m_ui->verticalLayout_dumpsysControls;
    if (!outerLayout)
        return;

    auto *chipsRow = new QHBoxLayout;
    chipsRow->setSpacing(6);
    chipsRow->setContentsMargins(0, 2, 0, 2);

    auto *label = new QLabel(tr("Presets:"), m_ui->dumpsysControlsContainer);
    label->setProperty("role", QStringLiteral("caption"));
    chipsRow->addWidget(label);

    for (const char *service : kPresetServices) {
        const QString name = QString::fromLatin1(service);
        auto *chip = UiComponents::Button::make(
            name, UiComponents::ButtonVariant::Ghost,
            m_ui->dumpsysControlsContainer, UiComponents::ButtonSize::Small);
        chip->setProperty("role", QStringLiteral("chip"));
        chip->style()->unpolish(chip);
        chip->style()->polish(chip);
        connect(chip, &QPushButton::clicked, this, [this, name]() { onPresetClicked(name); });
        chipsRow->addWidget(chip);
    }

    chipsRow->addStretch();
    outerLayout->addLayout(chipsRow);
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
    QString args = currentDumpsysArgs();

    if (deviceId.isEmpty() && args.isEmpty()) {
        m_ui->txtDumpsysCommand->clear();
        return;
    }

    const QString device = deviceId.isEmpty() ? QStringLiteral("<device-id>") : deviceId;
    if (args.isEmpty())
        args = QStringLiteral("<service>");

    m_ui->txtDumpsysCommand->setText(
        QStringLiteral("adb -s %1 shell dumpsys %2").arg(device, args));
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
        m_statusBar->showMessage(tr("No device selected"), kStatusTimeoutMs);
        return;
    }
    const QString args = currentDumpsysArgs();
    if (args.isEmpty()) {
        m_statusBar->showMessage(tr("No service specified"), kStatusTimeoutMs);
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
        tr("Dumpsys: %1 lines").arg(countLines(output)), kStatusTimeoutMs);
}

void DumpsysController::refreshExtraSelections()
{
    QPlainTextEdit *output = m_ui->txtDumpsysCmdResult;
    QTextDocument  *document = output->document();

    m_lastSearchNeedle = m_ui->txtDumpsysSearch->text();

    QList<QTextEdit::ExtraSelection> extras;
    extras.reserve(m_diffAddedLines.size() + m_diffRemovedLines.size());

    // Diff line tints first, so search hits paint on top of them.
    const auto addLineSelection = [&](int lineNumber, const QColor &background) {
        const QTextBlock block = document->findBlockByNumber(lineNumber);
        if (!block.isValid())
            return;
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(background);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = QTextCursor(block);
        extras.append(selection);
    };
    for (int line : std::as_const(m_diffAddedLines))
        addLineSelection(line, kDiffAddedBackground);
    for (int line : std::as_const(m_diffRemovedLines))
        addLineSelection(line, kDiffRemovedBackground);

    // Search hits: count and collect in a single walk of the document.
    int matches = 0;
    if (!m_lastSearchNeedle.isEmpty()) {
        QTextCharFormat format;
        format.setBackground(ColorScheme::instance().highlightBackground());
        format.setForeground(ColorScheme::instance().highlightForeground());

        QTextCursor cursor(document);
        while (!(cursor = document->find(m_lastSearchNeedle, cursor)).isNull()) {
            ++matches;
            if (matches > kMaxSearchHighlights)
                continue;   // keep counting, stop painting
            QTextEdit::ExtraSelection selection;
            selection.format = format;
            selection.cursor = cursor;
            extras.append(selection);
        }
    }

    output->setExtraSelections(extras);

    if (!m_matchLabel)
        return;
    if (m_lastSearchNeedle.isEmpty())
        m_matchLabel->setText(QString());
    else
        m_matchLabel->setText(tr("%n match(es)", nullptr, matches));
}

void DumpsysController::findInOutput(bool backwards, bool fromStart)
{
    const QString needle = m_ui->txtDumpsysSearch->text();
    if (needle.isEmpty())
        return;

    QPlainTextEdit *output = m_ui->txtDumpsysCmdResult;
    const QTextDocument::FindFlags flags =
        backwards ? QTextDocument::FindBackward : QTextDocument::FindFlags();

    if (fromStart) {
        QTextCursor cursor = output->textCursor();
        cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
        output->setTextCursor(cursor);
    }
    if (output->find(needle, flags))
        return;

    // Wrap around to the far end and try once more.
    QTextCursor cursor = output->textCursor();
    cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
    output->setTextCursor(cursor);
    output->find(needle, flags);
}

void DumpsysController::onDumpsysSearchChanged()
{
    // Debounced: the highlight pass walks the whole document.
    m_searchDebounce->start();
}

void DumpsysController::onDumpsysSearchNext()
{
    findInOutput(/*backwards=*/false);
}

void DumpsysController::onDumpsysSearchPrev()
{
    findInOutput(/*backwards=*/true);
}

void DumpsysController::onRunDumpsysCmdClicked()
{
    const QString cmdText = m_ui->txtDumpsysCommand->text().trimmed();
    if (cmdText.isEmpty()) {
        m_statusBar->showMessage(tr("No command specified"), kStatusTimeoutMs);
        return;
    }
    m_ui->txtDumpsysResult->setPlainText("…");
    AdbManager::instance().runRawAdbCommand(cmdText);
}

void DumpsysController::onRawAdbCommandFinished(const QString &output)
{
    m_ui->txtDumpsysResult->setPlainText(output);
    m_statusBar->showMessage(
        tr("Command: %1 lines").arg(countLines(output)), kStatusTimeoutMs);
}

void DumpsysController::onDumpsysListFetched(const QStringList &services)
{
    m_dumpsysServices = services;

    QCompleter *completer = m_ui->txtDumpsysService->completer();
    if (completer)
        qobject_cast<QStringListModel *>(completer->model())->setStringList(services);

    m_ui->txtDumpsysService->setPlaceholderText(tr("Service name..."));
    m_statusBar->showMessage(
        tr("Dumpsys: %1 services available").arg(services.size()), kStatusTimeoutMs);

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
        m_statusBar->showMessage(tr("Save failed: %1").arg(f.errorString()), kStatusTimeoutMs);
        return;
    }
    f.write(m_ui->txtDumpsysCmdResult->toPlainText().toUtf8());
    f.close();
    m_statusBar->showMessage(tr("Saved %1").arg(path), kStatusTimeoutMs);
}


// ─────────────────────────────────────────────────────────────────────────────
// Snapshot + diff mode
// ─────────────────────────────────────────────────────────────────────────────

void DumpsysController::onSnapshotClicked()
{
    if (m_currentService.isEmpty() || m_currentOutput.isEmpty()) {
        m_statusBar->showMessage(tr("Nothing to snapshot — run a service first"), kStatusTimeoutMs);
        return;
    }
    m_snapshots.insert(m_currentService, m_currentOutput);
    m_statusBar->showMessage(
        tr("Snapshot taken for '%1' (%2 lines)")
            .arg(m_currentService).arg(countLines(m_currentOutput)), kStatusTimeoutMs);
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

    if (!on) {
        m_monitorTimer->stop();
        m_statusBar->showMessage(tr("Monitor stopped"), 2000);
        return;
    }

    const int intervalMs = m_monitorIntervalCombo
                               ? m_monitorIntervalCombo->currentData().toInt()
                               : kDefaultMonitorIntervalMs;
    m_monitorTimer->setInterval(intervalMs);
    onRunDumpsysClicked();   // one immediate fetch, then tick
    m_monitorTimer->start();
    m_statusBar->showMessage(tr("Monitoring dumpsys every %1 ms").arg(intervalMs), 2000);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void DumpsysController::renderOutput()
{
    m_diffAddedLines.clear();
    m_diffRemovedLines.clear();

    QString text = m_currentOutput;
    if (m_diffMode && !m_currentService.isEmpty()
        && m_snapshots.contains(m_currentService)) {
        text = computeDiff(m_snapshots.value(m_currentService), m_currentOutput);
    }

    m_ui->txtDumpsysCmdResult->setPlainText(text);
    refreshExtraSelections();
    findInOutput(/*backwards=*/false, /*fromStart=*/true);
}

QString DumpsysController::currentDumpsysArgs() const
{
    const QString service = m_ui->txtDumpsysService->text().trimmed();
    const QString package = m_packageInput ? m_packageInput->text().trimmed() : QString();
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
