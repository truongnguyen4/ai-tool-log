#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "colorscheme.h"
#include "configurationcontroller.h"
#include "cradlecontroller.h"
#include "devicesmanager.h"
#include "devicestabcontroller.h"
#include "dumpsyscontroller.h"
#include "highlightdelegate.h"
#include "logsplitcontroller.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "settingsmodel.h"
#include "tableconfig.h"
#include "themesheets.h"
#include "threadtimelogconverter.h"
#include "tooltips.h"
#include "components/components.h"
#include "crashdetector.h"
#include "packageresolver.h"
#include "widgets/searchablelistpopup.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTableView>
#include <QTimer>

namespace {

// ── Status-bar indicators ────────────────────────────────────────────────────
constexpr int kDeviceFadeDurationMs = 280;
constexpr qreal kDeviceFadeFromOpacity = 0.35;
constexpr int kMonitorPulseIntervalMs = 700;
constexpr int kStatusWidgetHeight = 22;

/** Filled / hollow dot used by the status-bar indicators. */
constexpr auto kFilledDot = "●";
constexpr auto kHollowDot = "○";

// ── Log view toggles ─────────────────────────────────────────────────────────
constexpr auto kFileRowVisibleKey = "Logcat/fileRowVisible";
/** Height the marked panel opens at when it has none of its own. */
constexpr int kMarkedPanelHeight = 160;
/** How much of a crash message the status bar shows. */
constexpr int kCrashMessageChars = 90;

// ── Toolbar dividers ─────────────────────────────────────────────────────────
constexpr int kDividerWidth = 2;
constexpr int kDividerMinHeight = 28;
constexpr int kDividerMaxHeight = 34;

/** Coloured dot + label markup for a status-bar indicator. */
QString statusDot(const QColor &color, const char *glyph)
{
    return QStringLiteral("<span style='color:%1;'>%2</span>")
        .arg(ColorScheme::toHex(color), QString::fromUtf8(glyph));
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / initialize
// ─────────────────────────────────────────────────────────────────────────────

UiManager::UiManager(Ui::MainWindow *ui, MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_ui(ui)
    , m_mainWindow(mainWindow)
    , m_logConverter(new ThreadtimeLogConverter())
    , m_viewFont(savedViewFont())
{}

QTableView* UiManager::activeTableLog()
{
    return useB() ? m_logSplitController->paneB()->table : m_ui->tableLog;
}

void UiManager::snapshotInputsTo(PaneInputs &out) const
{
    out.query     = m_ui->txtLogQuery->text();
    out.startTime = m_ui->txtStartTime->text();
    out.endTime   = m_ui->txtEndTime->text();
    out.highlight = m_ui->txtHighlight->text();
    if      (m_ui->radioVerbosePlus->isChecked()) out.levelRadio = 0;
    else if (m_ui->radioV->isChecked())           out.levelRadio = 1;
    else if (m_ui->radioD->isChecked())           out.levelRadio = 2;
    else if (m_ui->radioI->isChecked())           out.levelRadio = 3;
    else if (m_ui->radioW->isChecked())           out.levelRadio = 4;
    else if (m_ui->radioE->isChecked())           out.levelRadio = 5;
    else if (m_ui->radioA->isChecked())           out.levelRadio = 6;
    else                                          out.levelRadio = -1;
}

void UiManager::loadInputsFrom(const PaneInputs &in)
{
    // Block signals so we don't trigger applyFilters() per-edit; we apply
    // once at the end after all widgets are set.
    const QSignalBlocker b1 (m_ui->txtLogQuery);
    const QSignalBlocker b2 (m_ui->txtStartTime);
    const QSignalBlocker b3 (m_ui->txtEndTime);
    const QSignalBlocker b4 (m_ui->txtHighlight);
    const QSignalBlocker b5 (m_ui->radioVerbosePlus);
    const QSignalBlocker b6 (m_ui->radioV);
    const QSignalBlocker b7 (m_ui->radioD);
    const QSignalBlocker b8 (m_ui->radioI);
    const QSignalBlocker b9 (m_ui->radioW);
    const QSignalBlocker b10(m_ui->radioE);
    const QSignalBlocker b11(m_ui->radioA);

    m_ui->txtLogQuery ->setText(in.query);
    m_ui->txtStartTime->setText(in.startTime);
    m_ui->txtEndTime  ->setText(in.endTime);
    m_ui->txtHighlight->setText(in.highlight);

    m_ui->radioVerbosePlus->setChecked(in.levelRadio == 0);
    m_ui->radioV          ->setChecked(in.levelRadio == 1);
    m_ui->radioD          ->setChecked(in.levelRadio == 2);
    m_ui->radioI          ->setChecked(in.levelRadio == 3);
    m_ui->radioW          ->setChecked(in.levelRadio == 4);
    m_ui->radioE          ->setChecked(in.levelRadio == 5);
    m_ui->radioA          ->setChecked(in.levelRadio == 6);

    m_highlightRow = -1;
    applyFilters();   // also refreshes the keyword highlighting
}


void UiManager::initialize()
{
    // ── Theme bootstrap ───────────────────────────────────────────────────────
    // Both light and dark stylesheets now live in ThemeSheets. The legacy dark
    // sheet baked into mainwindow.ui is discarded so qApp->setStyleSheet() can
    // drive theme swaps cleanly.
    m_darkStylesheet = ThemeSheets::darkStylesheet();
    m_mainWindow->setStyleSheet(QString());
    applyCurrentTheme();
    setupMainNavigationTabs();
    connect(&ColorScheme::instance(), &ColorScheme::modeChanged,
            this, &UiManager::applyCurrentTheme);

    // ── Status-bar quick theme toggle (one-click Light/Dark flip) ────────────
    if (m_ui->statusbar) {
        auto *themeToggleBtn = new QPushButton(m_mainWindow);
        themeToggleBtn->setObjectName(QStringLiteral("statusThemeToggle"));
        themeToggleBtn->setFlat(true);
        themeToggleBtn->setCursor(Qt::PointingHandCursor);
        themeToggleBtn->setToolTip(tr("Toggle light/dark theme"));
        themeToggleBtn->setFixedHeight(kStatusWidgetHeight);

        auto refreshLabel = [themeToggleBtn]() {
            const auto m = ColorScheme::instance().resolvedMode();
            themeToggleBtn->setText(m == ColorScheme::Mode::Light
                                        ? tr("Light \u25cf")
                                        : tr("\u25cb Dark"));
        };
        refreshLabel();

        connect(themeToggleBtn, &QPushButton::clicked, this, []() {
            auto &cs = ColorScheme::instance();
            const auto next = (cs.resolvedMode() == ColorScheme::Mode::Light)
                                  ? ColorScheme::Mode::Dark
                                  : ColorScheme::Mode::Light;
            cs.setMode(next);
        });
        connect(&ColorScheme::instance(), &ColorScheme::modeChanged,
                themeToggleBtn, refreshLabel);

        m_ui->statusbar->addPermanentWidget(themeToggleBtn);
    }

    // ── Create models ─────────────────────────────────────────────────────────
    m_logModel                = new LogModel(this);
    m_markLogModel            = new MarkLogModel(this);
    m_settingsModel           = new SettingsModel(this);
    m_propertiesModel         = new PropertiesModel(this);
    m_propertyDefinitionModel = new PropertyDefinitionModel(this);
    m_historyManager          = new HistoryManager(this);
    m_packageResolver         = new PackageResolver(this);
    m_logFilterController     = new LogFilterController(m_ui, this);

    // Batch-flush timer: coalesces incoming capture lines into one model
    // insert per tick. Created before any signal wiring so a line arriving
    // during start-up can never reach a null timer.
    m_batchFlushTimer = new QTimer(this);
    m_batchFlushTimer->setInterval(UiTiming::kBatchFlushIntervalMs);
    connect(m_batchFlushTimer, &QTimer::timeout, this, &UiManager::flushPendingLines);

    // ── Setup UI sections (order matters: models before tables) ───────────────
    setupLogTable();
    setupConfigurationTables();
    m_configurationController->setupSDKTab();

    m_logSplitController = new LogSplitController(m_ui, this);
    m_logSplitController->setup();
    connect(m_logSplitController, &LogSplitController::paneBBuilt,
            this, &UiManager::onPaneBBuilt);

    // Per-pane runtime UI state: snapshot the leaving pane and restore the
    // arriving pane on every active-pane change. Skipped while sync is on
    // (both panes share the same widget values then).
    connect(m_logSplitController, &LogSplitController::activePaneChanged, this,
            [this](bool isB) {
                if (m_syncPanes) { m_lastActiveIsB = isB; return; }
                if (m_lastActiveIsB) snapshotInputsTo(m_paneBInputs);
                else                 snapshotInputsTo(m_paneAInputs);
                if (isB) loadInputsFrom(m_paneBInputs);
                else     loadInputsFrom(m_paneAInputs);
                m_lastActiveIsB = isB;
            });
    connect(m_logSplitController, &LogSplitController::splitChanged, this,
            [this](bool active) {
                if (m_ui->btnSyncPanes) m_ui->btnSyncPanes->setVisible(active);
                if (active) {
                    snapshotInputsTo(m_paneAInputs);
                    m_paneBInputs   = m_paneAInputs;
                    m_lastActiveIsB = false;
                } else {
                    snapshotInputsTo(m_paneAInputs);
                    m_paneBInputs   = PaneInputs();
                    m_lastActiveIsB = false;
                    if (m_ui->btnSyncPanes && m_ui->btnSyncPanes->isChecked())
                        m_ui->btnSyncPanes->setChecked(false);
                    m_syncPanes = false;
                    if (m_logSplitController) m_logSplitController->setSyncHighlight(false);
                }
            });

    // Sync toggle: when ON, applyFilters() also drives the inactive pane.
    if (m_ui->btnSyncPanes) {
        connect(m_ui->btnSyncPanes, &QPushButton::toggled, this,
                [this](bool on) {
                    m_syncPanes = on;
                    if (m_logSplitController) m_logSplitController->setSyncHighlight(on);
                    if (on) {
                        snapshotInputsTo(m_paneAInputs);
                        m_paneBInputs = m_paneAInputs;
                        applyFilters();
                    }
                });
    }

    m_dumpsysController = new DumpsysController(
        m_ui, m_ui->statusbar,
        [this]() { return m_currentDeviceId; }, this);
    m_dumpsysController->setup();

    m_cradleController = new CradleController(
        m_ui, m_ui->statusbar,
        [this]() { return m_currentDeviceId; }, this);
    m_cradleController->setup();

    setupTabAutoFetch();
    setupTooltips();
    setupToolbarDividers();
    loadKnownDevices();
    setupCaptureControls();
    setupLogViewToggles();
    setupStatusBarIndicators();
    setupSplittersAndMisc();
    m_devicesTabController = new DevicesTabController(this, this);
    m_devicesTabController->setup();

    // ── Wire up all signal/slot connections ───────────────────────────────────
    connectAdbManagerSignals();
    connectFilterSignals();
    connectButtonSignals();
    connectTableSignals();
    setupFilterCompleters();

    // ── Initial state ─────────────────────────────────────────────────────────
    m_ui->tabWidget->setCurrentIndex(0);

    // Enable Ctrl+C copy for config/SDK tables (done after models are assigned)
    for (QTableView *tv : {m_ui->tableSettings, m_ui->tableProperties,
                           m_ui->tablePropertyDefinitions}) {
        enableTableCopyAction(tv);
    }

    applyFilters();
    updateStatusBar();
}

void UiManager::onPaneBBuilt(QTableView *logTable, QTableView *markTable)
{
    connectLogTableSignals(logTable, markTable);
    if (!logTable)
        return;

    // Pane B gets its own highlight delegates so that, with sync off, filter
    // keywords only light up the pane they were typed for.
    m_pidHighlightDelegateB     = new HighlightDelegate(this);
    m_packageHighlightDelegateB = new HighlightDelegate(this);
    m_tagHighlightDelegateB     = new HighlightDelegate(this);
    m_messageHighlightDelegateB = new HighlightDelegate(this);
    installLogHighlightDelegates(logTable, /*paneB=*/true);
    fitLogRowHeight(logTable);
    fitLogRowHeight(markTable);
    wireRowResizeTriggers(logTable, [this]() {
        auto *paneB = m_logSplitController ? m_logSplitController->paneB() : nullptr;
        return paneB && paneB->model && paneB->model->rowCount() > 0;
    });

    // Size any rows the pane was seeded with.
    m_rowResizeTimer->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Devices Tab
// ─────────────────────────────────────────────────────────────────────────────

// refreshDevicesTab/onDevicesOrGroupsChanged/selectDeviceRow/refreshCheckedDevicesList/updateDeviceDetails/onDeviceDetailsFetched are defined in uimanager_devicestab.cpp
// setupDevicesTab() moved to uimanager_devicestab.cpp
void UiManager::setupToolbarDividers()
{
    // Visually group the Logcat toolbar buttons:
    //   [Start | Kernel]  ·  [AutoScroll · Columns · CellContent]  ·  [Split]  ·  [Clear · Save]
    auto *layout = qobject_cast<QHBoxLayout *>(m_ui->btnAutoScroll->parentWidget()->layout());
    if (!layout) return;

    auto makeDivider = [this]() {
        auto *line = new QFrame(m_ui->btnAutoScroll->parentWidget());
        line->setObjectName(QStringLiteral("toolbarDivider"));
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Plain);
        line->setFixedWidth(kDividerWidth);
        line->setMinimumHeight(kDividerMinHeight);
        line->setMaximumHeight(kDividerMaxHeight);
        // Colour comes from the theme sheet's QFrame#toolbarDivider rule.
        return line;
    };

    auto insertBefore = [&](QWidget *anchor) {
        const int idx = layout->indexOf(anchor);
        if (idx >= 0) layout->insertWidget(idx, makeDivider());
    };

    insertBefore(m_ui->btnAutoScroll);
    insertBefore(m_ui->btnSplitLog);
    insertBefore(m_ui->btnClear);
}

void UiManager::setupLogViewToggles()
{
    using namespace UiComponents;
    QHBoxLayout *toolbar = m_ui->horizontalLayout_3;
    if (!toolbar)
        return;
    QWidget *parent = m_ui->btnStart->parentWidget();
    QSettings settings;

    // The file row is dead space for anyone who only watches a live device.
    m_btnFileRow = Button::make(tr("▤ File"), ButtonVariant::Secondary, parent, ButtonSize::Medium);
    m_btnFileRow->setObjectName(QStringLiteral("btnFileRow"));
    m_btnFileRow->setCheckable(true);
    m_btnFileRow->setToolTip(tr("Show the row that opens and saves a log file"));
    connect(m_btnFileRow, &QPushButton::toggled, this, [this](bool on) {
        setFileRowVisible(on);
        QSettings().setValue(QLatin1String(kFileRowVisibleKey), on);
    });

    // The marked list starts out of the way and appears when it has something.
    m_btnMarkedPanel = Button::make(tr("⚑ Marked"), ButtonVariant::Secondary, parent, ButtonSize::Medium);
    m_btnMarkedPanel->setObjectName(QStringLiteral("btnMarkedPanel"));
    m_btnMarkedPanel->setCheckable(true);
    m_btnMarkedPanel->setToolTip(tr("Show the marked lines. Double-click a log row to mark it."));
    connect(m_btnMarkedPanel, &QPushButton::toggled, this, &UiManager::setMarkedPanelVisible);

    // Crashes and ANRs mark themselves; this walks them.
    m_btnCrashes = Button::make(tr("⚠ Crashes"), ButtonVariant::Secondary, parent, ButtonSize::Medium);
    m_btnCrashes->setObjectName(QStringLiteral("btnCrashes"));
    m_btnCrashes->setToolTip(tr("Jump to the next crash or ANR in this log"));
    m_btnCrashes->setEnabled(false);
    connect(m_btnCrashes, &QPushButton::clicked, this, &UiManager::onNextCrashClicked);

    const int index = toolbar->indexOf(m_ui->btnFitRows);
    toolbar->insertWidget(index + 1, m_btnFileRow);
    toolbar->insertWidget(index + 2, m_btnMarkedPanel);
    toolbar->insertWidget(index + 3, m_btnCrashes);

    // Saving the marked lines is what turns a session into a bug report.
    if (QHBoxLayout *markHeader = m_ui->horizontalLayout_markLogHeader) {
        m_btnSaveMarked = Button::icon(QIcon(QStringLiteral(":/icons/document-save.svg")),
                                       tr("Save the marked lines to a file"),
                                       m_ui->markLogHeader, ButtonSize::Small);
        m_btnSaveMarked->setObjectName(QStringLiteral("btnSaveMarked"));
        connect(m_btnSaveMarked, &QPushButton::clicked, this, &UiManager::onSaveMarkedClicked);
        markHeader->insertWidget(qMax(0, markHeader->indexOf(m_ui->btnClearAllMarked)), m_btnSaveMarked);
    }

    // Which app wrote a line only becomes visible once pids are resolved.
    m_btnAppFilter = Button::make(tr("▾ App"), ButtonVariant::Ghost, m_ui->groupBoxQuery,
                                  ButtonSize::Small);
    m_btnAppFilter->setObjectName(QStringLiteral("btnAppFilter"));
    m_btnAppFilter->setToolTip(tr("Filter by an app running on the device.\n"
                                  "The list opens with a search box; type to narrow it."));
    connect(m_btnAppFilter, &QPushButton::clicked, this, &UiManager::showAppFilterPopup);
    if (m_ui->verticalLayout_query)
        m_ui->verticalLayout_query->addWidget(m_btnAppFilter);

    // The buttons follow the model, so every way of marking a row updates them.
    connect(m_markLogModel, &QAbstractItemModel::rowsInserted, this, [this]() { updateMarkedPanel(); });
    connect(m_markLogModel, &QAbstractItemModel::rowsRemoved,  this, [this]() { updateMarkedPanel(); });
    connect(m_markLogModel, &QAbstractItemModel::modelReset,   this, [this]() { updateMarkedPanel(); });

    // Lines that arrived before their process was known get their app filled in.
    connect(m_packageResolver, &PackageResolver::mappingChanged,
            this, &UiManager::fillMissingPackages);

    m_btnFileRow->setChecked(settings.value(QLatin1String(kFileRowVisibleKey), true).toBool());
    setFileRowVisible(m_btnFileRow->isChecked());
    setMarkedPanelVisible(false);
    updateMarkedPanel();
}

void UiManager::setFileRowVisible(bool visible)
{
    for (QWidget *widget : {static_cast<QWidget *>(m_ui->lblFilePath),
                            static_cast<QWidget *>(m_ui->txtFilePath),
                            static_cast<QWidget *>(m_ui->btnOpen)}) {
        if (widget)
            widget->setVisible(visible);
    }
}

void UiManager::setMarkedPanelVisible(bool visible)
{
    if (!m_ui->markLogContainer)
        return;
    m_ui->markLogContainer->setVisible(visible);
    if (!visible)
        return;

    // A panel hidden when the splitter last laid itself out comes back with no
    // height of its own.
    QSplitter *splitter = m_ui->splitterLogTables;
    QList<int> sizes = splitter ? splitter->sizes() : QList<int>();
    if (sizes.size() == 2 && sizes.at(1) < kMarkedPanelHeight) {
        const int total = qMax(sizes.at(0) + sizes.at(1), 3 * kMarkedPanelHeight);
        splitter->setSizes({total - kMarkedPanelHeight, kMarkedPanelHeight});
    }
}

void UiManager::updateMarkedPanel()
{
    const int count = m_markLogModel->getMarkedCount();
    m_btnMarkedPanel->setText(count > 0 ? tr("⚑ Marked (%1)").arg(count) : tr("⚑ Marked"));

    // Earning its space: the panel opens on the first mark, not on every one,
    // so hiding it again is respected.
    if (count > 0 && m_lastMarkedCount == 0 && !m_btnMarkedPanel->isChecked())
        m_btnMarkedPanel->setChecked(true);
    m_lastMarkedCount = count;

    int crashes = 0;
    for (const LogEntry &entry : m_markLogModel->markedEntries())
        crashes += CrashDetector::classify(entry) != CrashDetector::Kind::None ? 1 : 0;
    m_btnCrashes->setText(crashes > 0 ? tr("⚠ Crashes (%1)").arg(crashes) : tr("⚠ Crashes"));
    m_btnCrashes->setEnabled(crashes > 0);
}

void UiManager::onNextCrashClicked()
{
    MarkLogModel *markModel = activeMarkLogModel();
    const QVector<LogEntry> entries = markModel->markedEntries();
    if (entries.isEmpty())
        return;

    // Walk the marked crashes in order, carrying on past the one shown last.
    for (int step = 1; step <= entries.size(); ++step) {
        const int row = (m_lastCrashRow + step) % entries.size();
        const CrashDetector::Kind kind = CrashDetector::classify(entries.at(row));
        if (kind == CrashDetector::Kind::None)
            continue;
        m_lastCrashRow = row;

        const int filteredRow = findLogInFilteredLogs(markModel->getOriginalIndex(row));
        if (filteredRow < 0) {
            flashStatus(tr("The next %1 is hidden by the filter").arg(CrashDetector::describe(kind)));
            return;
        }
        QTableView *table = activeTableLog();
        table->selectRow(filteredRow);
        table->scrollTo(activeLogModel()->index(filteredRow, TableConfig::LogColumns::MESSAGE),
                        QAbstractItemView::PositionAtCenter);
        flashStatus(tr("%1: %2").arg(CrashDetector::describe(kind),
                                     entries.at(row).message.left(kCrashMessageChars)));
        return;
    }
    flashStatus(tr("No crash or ANR in this log"));
}

void UiManager::showAppFilterPopup()
{
    const QStringList packages = m_packageResolver->packages();
    if (packages.isEmpty()) {
        m_packageResolver->refresh();
        flashStatus(m_currentDeviceId.isEmpty() ? tr("Connect a device to list its apps")
                                                : tr("Reading the app list…"));
        return;
    }

    // What already wrote to this log comes first: that is what the user is after.
    QStringList seen;
    for (const QString &pid : std::as_const(m_pidsSeen)) {
        const QString package = m_packageResolver->packageFor(pid);
        if (!package.isEmpty() && !seen.contains(package))
            seen.append(package);
    }
    seen.sort(Qt::CaseInsensitive);

    QVector<SearchableListPopup::Item> items;
    items.reserve(packages.size() + 2);
    if (!seen.isEmpty()) {
        items.append({tr("In this log"), true});
        for (const QString &name : std::as_const(seen))
            items.append({name, false});
        items.append({tr("Running on the device"), true});
    }
    for (const QString &name : packages) {
        if (!seen.contains(name))
            items.append({name, false});
    }

    if (!m_appFilterPopup) {
        m_appFilterPopup = new SearchableListPopup(m_mainWindow);
        m_appFilterPopup->setPlaceholderText(tr("Type to search apps…"));
        m_appFilterPopup->setItemLabel(tr("apps"));
        connect(m_appFilterPopup, &SearchableListPopup::itemChosen, this, [this](const QString &name) {
            addQueryTerm(LogQuery::Field::Package, name, /*exclude=*/false);
        });
    }
    m_appFilterPopup->showItems(m_btnAppFilter, items);
}

void UiManager::fillMissingPackages()
{
    const auto lookup = [this](const QString &pid) { return m_packageResolver->packageFor(pid); };
    const auto fillVector = [&lookup](QVector<LogEntry> &entries) {
        for (LogEntry &entry : entries) {
            if (entry.package.isEmpty() && !entry.pid.isEmpty())
                entry.package = lookup(entry.pid);
        }
    };

    fillVector(allLogs);
    fillVector(filteredLogs);
    m_logModel->fillMissingPackages(lookup);
    if (m_logSplitController) {
        if (auto *paneB = m_logSplitController->paneB()) {
            fillVector(paneB->allLogs);
            fillVector(paneB->filteredLogs);
            if (paneB->model)
                paneB->model->fillMissingPackages(lookup);
        }
    }
}

void UiManager::setupStatusBarIndicators()
{
    if (!m_ui->statusbar) return;

    m_lblStatusDevices = new QLabel(m_ui->statusbar);
    m_lblStatusMonitor = new QLabel(m_ui->statusbar);
    m_lblStatusDevices->setObjectName(QStringLiteral("statusDevicesLabel"));
    m_lblStatusMonitor->setObjectName(QStringLiteral("statusMonitorLabel"));
    m_lblStatusDevices->setContentsMargins(8, 0, 8, 0);
    m_lblStatusMonitor->setContentsMargins(8, 0, 8, 0);

    // Fade the device indicator when the count changes, to draw the eye.
    auto *deviceOpacity = new QGraphicsOpacityEffect(m_lblStatusDevices);
    deviceOpacity->setOpacity(1.0);
    m_lblStatusDevices->setGraphicsEffect(deviceOpacity);
    auto *deviceFade = new QPropertyAnimation(deviceOpacity, "opacity", this);
    deviceFade->setDuration(kDeviceFadeDurationMs);
    deviceFade->setEasingCurve(QEasingCurve::InOutCubic);
    deviceFade->setStartValue(kDeviceFadeFromOpacity);
    deviceFade->setEndValue(1.0);

    auto refreshDevices = [this, deviceOpacity, deviceFade]() {
        const ColorScheme &colors = ColorScheme::instance();
        const int count = int(m_onlineSerials.size());
        const bool connected = count > 0;

        m_lblStatusDevices->setText(
            statusDot(connected ? colors.success() : colors.mutedText(),
                      connected ? kFilledDot : kHollowDot)
            + QLatin1Char(' ') + tr("%n device(s)", nullptr, count));

        deviceFade->stop();
        deviceOpacity->setOpacity(kDeviceFadeFromOpacity);
        deviceFade->start();
    };
    refreshDevices();

    if (m_ui->cmbDevice) {
        connect(m_ui->cmbDevice, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [refreshDevices](int) { refreshDevices(); });
        // The combo's own model signals catch add / remove / reset.
        QAbstractItemModel *deviceModel = m_ui->cmbDevice->model();
        connect(deviceModel, &QAbstractItemModel::rowsInserted,
                this, [refreshDevices](const QModelIndex &, int, int) { refreshDevices(); });
        connect(deviceModel, &QAbstractItemModel::rowsRemoved,
                this, [refreshDevices](const QModelIndex &, int, int) { refreshDevices(); });
        connect(deviceModel, &QAbstractItemModel::modelReset,
                this, [refreshDevices]() { refreshDevices(); });
    }

    auto renderMonitor = [this]() {
        const ColorScheme &colors = ColorScheme::instance();
        if (m_monitorActiveCount <= 0) {
            m_lblStatusMonitor->setText(statusDot(colors.mutedText(), kHollowDot)
                                        + QLatin1Char(' ') + tr("Idle"));
            return;
        }
        // Alternate between the accent and a muted tone for a "live" pulse.
        const QColor pulse = m_monitorPulseBright ? colors.accent() : colors.mutedText();
        m_lblStatusMonitor->setText(statusDot(pulse, kFilledDot) + QLatin1Char(' ')
                                    + tr("Monitoring (%1)").arg(m_monitorActiveCount));
    };

    if (!m_monitorPulseTimer) {
        m_monitorPulseTimer = new QTimer(this);
        m_monitorPulseTimer->setInterval(kMonitorPulseIntervalMs);
        connect(m_monitorPulseTimer, &QTimer::timeout, this, [this, renderMonitor]() {
            m_monitorPulseBright = !m_monitorPulseBright;
            renderMonitor();
        });
    }

    auto setMonitorCount = [this, renderMonitor](int active) {
        m_monitorActiveCount = active;
        m_monitorPulseBright = true;
        renderMonitor();
        if (active > 0) {
            if (!m_monitorPulseTimer->isActive())
                m_monitorPulseTimer->start();
        } else {
            m_monitorPulseTimer->stop();
        }
    };
    setMonitorCount(0);

    // Repaint both indicators when the theme changes.
    connect(&ColorScheme::instance(), &ColorScheme::modeChanged, this,
            [refreshDevices, renderMonitor]() { refreshDevices(); renderMonitor(); });

    if (m_configurationController) {
        connect(m_configurationController, &ConfigurationController::monitorStateChanged,
                this, setMonitorCount);

        // Accent the config tables that are actively polling, reusing the same
        // themed "active pane" marker as the split log view.
        connect(m_configurationController, &ConfigurationController::monitorTablesChanged,
                this, [this](bool settings, bool properties, bool propertyDefs) {
            setTableMonitoring(m_ui->tableSettings, settings);
            setTableMonitoring(m_ui->tableProperties, properties);
            setTableMonitoring(m_ui->tablePropertyDefinitions, propertyDefs);
        });
    }

    m_ui->statusbar->addPermanentWidget(m_lblStatusDevices);
    m_ui->statusbar->addPermanentWidget(m_lblStatusMonitor);
}

void UiManager::setTableMonitoring(QTableView *view, bool monitoring)
{
    if (!view)
        return;
    view->setProperty("pane", monitoring ? QStringLiteral("active")
                                         : QStringLiteral("inactive"));
    view->style()->unpolish(view);
    view->style()->polish(view);
}

void UiManager::setupTooltips()
{
    m_ui->btnAppSettings->setToolTip(tr(Tooltips::btnAppSettings));
    m_ui->btnStart->setToolTip(tr(Tooltips::btnStart));
    m_ui->btnKernel->setToolTip(tr(Tooltips::btnKernel));
    m_ui->btnAutoScroll->setToolTip(tr(Tooltips::btnAutoScroll));
    m_ui->btnColumns->setToolTip(tr(Tooltips::btnColumns));
    m_ui->btnClear->setToolTip(tr(Tooltips::btnClear));
    m_ui->btnClearAllMarked->setToolTip(tr(Tooltips::btnClearAllMarked));
    m_ui->btnSave->setToolTip(tr(Tooltips::btnSave));
    m_ui->btnOpen->setToolTip(tr(Tooltips::btnOpen));
    m_ui->btnFetchPropertyDefs->setToolTip(tr(Tooltips::btnFetchPropertyDefs));

    // U7: filter input tooltips with syntax help.
    m_ui->txtLogQuery->setToolTip(tr(Tooltips::txtLogQuery));
    m_ui->txtFilterSettings->setToolTip(tr(Tooltips::txtFilterSettings));
    m_ui->txtFilterProperties->setToolTip(tr(Tooltips::txtFilterProperties));
    m_ui->txtStartTime->setToolTip(tr(Tooltips::txtStartTime));
    m_ui->txtEndTime->setToolTip(tr(Tooltips::txtEndTime));

    // U8: accessibility names so screen readers announce custom widgets.
    m_ui->btnAppSettings->setAccessibleName(tr("Application settings"));
    m_ui->btnAutoScroll->setAccessibleName(tr("Auto-scroll to newest log"));
    m_ui->btnAutoScroll->setAccessibleDescription(tr("When enabled, the table follows the newest log line."));
    m_ui->btnColumns->setAccessibleName(tr("Choose visible columns"));
    m_ui->btnClear->setAccessibleName(tr("Clear log buffer"));
    m_ui->btnSave->setAccessibleName(tr("Save logs to file"));
    m_ui->btnOpen->setAccessibleName(tr("Open log file"));
    m_ui->cmbDevice->setAccessibleName(tr("ADB device"));
    m_ui->txtLogQuery->setAccessibleName(tr("Log filter"));
}

void UiManager::persistFilterHistory()
{
    m_historyManager->flush();
    saveLayoutPreferences();
}


// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Dumpsys Tab — moved to src/ui/dumpsyscontroller.cpp
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Cradle Manager Tab — moved to src/ui/cradlecontroller.cpp
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: App Settings & Column Visibility
// ─────────────────────────────────────────────────────────────────────────────
