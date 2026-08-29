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

#include <QAbstractItemModel>
#include <QComboBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
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
{}

QTableView* UiManager::activeTableLog()
{
    return useB() ? m_logSplitController->paneB()->table : m_ui->tableLog;
}

void UiManager::snapshotInputsTo(PaneInputs &out) const
{
    out.message   = m_ui->txtFindMessage->text();
    out.tag       = m_ui->txtTagFilter->text();
    out.package   = m_ui->txtPackageFilter->text();
    out.pid       = m_ui->txtPidFilter->text();
    out.startTime = m_ui->txtStartTime->text();
    out.endTime   = m_ui->txtEndTime->text();
    out.keyword   = m_ui->txtKeyword->text();
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
    const QSignalBlocker b1 (m_ui->txtFindMessage);
    const QSignalBlocker b2 (m_ui->txtTagFilter);
    const QSignalBlocker b3 (m_ui->txtPackageFilter);
    const QSignalBlocker b4 (m_ui->txtPidFilter);
    const QSignalBlocker b5 (m_ui->txtStartTime);
    const QSignalBlocker b6 (m_ui->txtEndTime);
    const QSignalBlocker b7 (m_ui->txtKeyword);
    const QSignalBlocker b8 (m_ui->txtHighlight);
    const QSignalBlocker b9 (m_ui->radioVerbosePlus);
    const QSignalBlocker b10(m_ui->radioV);
    const QSignalBlocker b11(m_ui->radioD);
    const QSignalBlocker b12(m_ui->radioI);
    const QSignalBlocker b13(m_ui->radioW);
    const QSignalBlocker b14(m_ui->radioE);
    const QSignalBlocker b15(m_ui->radioA);

    m_ui->txtFindMessage  ->setText(in.message);
    m_ui->txtTagFilter    ->setText(in.tag);
    m_ui->txtPackageFilter->setText(in.package);
    m_ui->txtPidFilter    ->setText(in.pid);
    m_ui->txtStartTime    ->setText(in.startTime);
    m_ui->txtEndTime      ->setText(in.endTime);
    m_ui->txtKeyword      ->setText(in.keyword);
    m_ui->txtHighlight    ->setText(in.highlight);

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
        const int count = m_ui->cmbDevice ? m_ui->cmbDevice->count() : 0;
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
    m_ui->btnClearAllProperties->setToolTip(tr(Tooltips::btnClearAllProps));
    m_ui->btnFetchPropertyDefs->setToolTip(tr(Tooltips::btnFetchPropertyDefs));

    // U7: filter input tooltips with syntax help.
    m_ui->txtKeyword->setToolTip(tr(Tooltips::txtKeyword));
    m_ui->txtTagFilter->setToolTip(tr(Tooltips::txtTagFilter));
    m_ui->txtPidFilter->setToolTip(tr(Tooltips::txtPidFilter));
    m_ui->txtPackageFilter->setToolTip(tr(Tooltips::txtPackageFilter));
    m_ui->txtFindMessage->setToolTip(tr(Tooltips::txtFindMessage));
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
    m_ui->cmbDevice->setAccessibleName(tr("Connected ADB device"));
    m_ui->txtKeyword->setAccessibleName(tr("Keyword filter"));
    m_ui->txtTagFilter->setAccessibleName(tr("Tag filter"));
    m_ui->txtPidFilter->setAccessibleName(tr("PID filter"));
    m_ui->txtPackageFilter->setAccessibleName(tr("Package filter"));
    m_ui->txtFindMessage->setAccessibleName(tr("Message filter"));
}

void UiManager::persistFilterHistory()
{
    m_historyManager->flush();
    saveLayoutPreferences();
}

void UiManager::onDeviceChanged(int index)
{
    const QString oldDeviceId = m_currentDeviceId;
    const QString deviceName  = m_ui->cmbDevice->itemText(index);
    const QString deviceId    = m_ui->cmbDevice->itemData(index).toString();

    // Stop any active monitor — its 500ms re-fetch loop is bound to the
    // previous device and would race against the swap.
    if (oldDeviceId != deviceId) {
        if (m_dumpsysController)       m_dumpsysController->stopMonitor();
        if (m_configurationController) m_configurationController->stopAllMonitors();
    }

    m_currentDeviceId = deviceId;
    AdbManager::instance().setCurrentDeviceId(deviceId);

    if (!deviceId.isEmpty()) {
        m_ui->statusbar->showMessage(QString("Selected device: %1").arg(deviceName), 2000);
        if (m_dumpsysController) m_dumpsysController->clearServices();
        const QString prevService = m_ui->txtDumpsysService->text().trimmed();
        m_ui->txtDumpsysService->setPlaceholderText(tr("Loading services..."));
        AdbManager::instance().fetchDumpsysList(deviceId);

        // Refresh configuration tables for the newly-focused device so the
        // user sees data that belongs to *this* device, not the previous one.
        if (oldDeviceId != deviceId) {
            AdbManager::instance().fetchSettings(deviceId);
            AdbManager::instance().fetchProperties(deviceId);
            AdbManager::instance().fetchPropertyDefinitions(deviceId);
        }

        // If the service field is empty, restore the last-used service for
        // this specific device from QSettings (per-device persistence).
        if (prevService.isEmpty() && m_dumpsysController)
            m_dumpsysController->restoreLastService(deviceId);

        // Re-fetch the same dumpsys service on the new device
        const QString svcAfterRestore = m_ui->txtDumpsysService->text().trimmed();
        if (!svcAfterRestore.isEmpty()) {
            m_ui->txtDumpsysCmdResult->setPlainText(QStringLiteral("..."));
            AdbManager::instance().fetchDumpsys(deviceId, svcAfterRestore);
        }
    }
    if (m_dumpsysController) m_dumpsysController->refreshCommandText();
}

void UiManager::onDevicesChanged(const QList<AdbDevice> &devices)
{
    const QString previousDeviceId = m_ui->cmbDevice->currentData().toString();
    m_ui->cmbDevice->clear();
    setDeviceStatusConnected(!devices.isEmpty());

    if (devices.isEmpty()) {
        m_ui->cmbDevice->addItem(tr("No devices found"), QString());
        m_currentDeviceId.clear();
        AdbManager::instance().setCurrentDeviceId(QString());

        // Stop any active monitor — there is nothing left to query.
        if (m_dumpsysController)       m_dumpsysController->stopMonitor();
        if (m_configurationController) m_configurationController->stopAllMonitors();

        // Drop device-specific data so no stale values are shown as current.
        m_settingsModel->setSettings({});
        m_propertiesModel->setProperties({});
        m_availablePropertyDefinitions.clear();
        m_configurationController->updatePropertyNamesCompleter();
        m_ui->txtDumpsysCmdResult->clear();
        if (m_dumpsysController) m_dumpsysController->clearServices();
        m_ui->txtDumpsysService->clear();
        m_ui->txtDumpsysService->setPlaceholderText(tr("Service name (e.g. activity)"));
        return;
    }

    for (const AdbDevice &device : devices)
        m_ui->cmbDevice->addItem(device.name, device.id);

    // Keep the previous selection when that device is still attached.
    const int index = m_ui->cmbDevice->findData(previousDeviceId);
    m_ui->cmbDevice->setCurrentIndex(index >= 0 ? index : 0);

    m_currentDeviceId = m_ui->cmbDevice->currentData().toString();
    AdbManager::instance().setCurrentDeviceId(m_currentDeviceId);

    // Auto-load the configuration tables when the first device appears.
    if (!m_currentDeviceId.isEmpty() && previousDeviceId.isEmpty()) {
        AdbManager &adb = AdbManager::instance();
        adb.fetchSettings(m_currentDeviceId);
        adb.fetchProperties(m_currentDeviceId);
        adb.fetchPropertyDefinitions(m_currentDeviceId);
    }
}

void UiManager::setDeviceStatusConnected(bool connected)
{
    QLabel *label = m_ui->lblDeviceStatus;
    if (!label)
        return;
    // Styled from the theme sheet via QLabel#lblDeviceStatus[state=...].
    label->setProperty("state", connected ? QStringLiteral("connected")
                                          : QStringLiteral("disconnected"));
    label->style()->unpolish(label);
    label->style()->polish(label);
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
