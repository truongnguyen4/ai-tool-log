#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "devicesmanager.h"
#include "adbcommand.h"
#include "threadtimelogconverter.h"
#include "brieflogconverter.h"
#include "propertydefinitionconverter.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "highlightdelegate.h"
#include "tableconfig.h"
#include "settingsdialog.h"
#include "tooltips.h"
#include "cradlecontroller.h"
#include "dumpsyscontroller.h"
#include "devicestabcontroller.h"
#include "logsplitcontroller.h"
#include "configurationcontroller.h"
#include "colorscheme.h"
#include "themesheets.h"
#include "shortcutsdialog.h"
#include <adbcommand.h>

#include <QHeaderView>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCompleter>
#include <QSettings>
#include <QFrame>
#include <QLabel>
#include <QComboBox>
#include <QAbstractItemModel>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTableView>
#include <algorithm>
#include "toggleswitch.h"
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QRadioButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QStringListModel>
#include <QWheelEvent>
#include <QTextStream>
#include <QSplitter>
#include <QShortcut>
#include <QProcess>
#include <QTextEdit>
#include <QInputDialog>
#include <QListWidget>
#include <QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollArea>
#include <QProgressBar>
#include <QGroupBox>
#include <QGridLayout>
#include <QDrag>
#include <QMimeData>

// Row-resize threshold: below this count every row is sized to content on each
// resize pass; above it only the visible viewport is measured for performance.
static constexpr int ROW_RESIZE_THRESHOLD = 50'000;

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
QTableView* UiManager::activeTableMarkLog()
{
    return useB() ? m_logSplitController->paneB()->markTable : m_ui->tableMarkLog;
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
    applyFilters();
    updateFilterHighlighting();
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
        themeToggleBtn->setFixedHeight(22);

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

    // ── Setup UI sections (order matters: models before tables) ───────────────
    setupLogTable();
    setupConfigurationTables();
    m_configurationController->setupSDKTab();

    m_logSplitController = new LogSplitController(m_ui, this);
    m_logSplitController->setup();
    connect(m_logSplitController, &LogSplitController::paneBBuilt, this,
            [this](QTableView *logTable, QTableView *markTable) {
                if (logTable) {
                    using namespace TableConfig::LogColumns;
                    logTable->setContextMenuPolicy(Qt::CustomContextMenu);
                    connect(logTable, &QTableView::customContextMenuRequested,
                            this, &UiManager::onTableContextMenu);
                    connect(logTable, &QTableView::doubleClicked,
                            this, &UiManager::onLogTableDoubleClicked);
                    connect(logTable, &QTableView::clicked,
                            this, &UiManager::onLogTableClicked);
                    enableTableCopyAction(logTable);
                    logTable->viewport()->installEventFilter(m_mainWindow);
                    // Pane B owns its own highlight delegates so the
                    // message/tag/package/PID filter highlights only follow
                    // the active pane (when sync is off). word-wrap on
                    // message column matches pane A.
                    m_pidHighlightDelegateB     = new HighlightDelegate(this);
                    m_packageHighlightDelegateB = new HighlightDelegate(this);
                    m_tagHighlightDelegateB     = new HighlightDelegate(this);
                    m_messageHighlightDelegateB = new HighlightDelegate(this);
                    m_messageHighlightDelegateB->setWordWrap(true);
                    logTable->setItemDelegateForColumn(PID,     m_pidHighlightDelegateB);
                    logTable->setItemDelegateForColumn(PACKAGE, m_packageHighlightDelegateB);
                    logTable->setItemDelegateForColumn(TAG,     m_tagHighlightDelegateB);
                    logTable->setItemDelegateForColumn(MESSAGE, m_messageHighlightDelegateB);
                    // Pane-A also installs a plain HighlightDelegate at the
                    // view level so DATE/TIME/TID/LEVEL honour Qt::BackgroundRole
                    // for marked rows. Mirror it on pane B.
                    logTable->setItemDelegate(new HighlightDelegate(this));
                    // Word-wrap support: trigger row-height recompute on scroll
                    // and on column resize (TAG/MESSAGE only) — same wiring as
                    // pane A in setupLogTable().
                    connect(logTable->horizontalHeader(), &QHeaderView::sectionResized,
                            this, [this](int section, int, int) {
                        if (section == TableConfig::LogColumns::TAG ||
                            section == TableConfig::LogColumns::MESSAGE)
                            m_rowResizeTimer->start();
                    });
                    connect(logTable->verticalScrollBar(), &QScrollBar::valueChanged,
                            this, [this](int) {
                        if (m_logSplitController && m_logSplitController->paneB() &&
                            m_logSplitController->paneB()->model &&
                            m_logSplitController->paneB()->model->rowCount() > 0)
                            m_rowResizeTimer->start();
                    });
                    // Kick an initial row-resize so any pre-seeded rows wrap.
                    m_rowResizeTimer->start();
                }
                if (markTable) {
                    markTable->setContextMenuPolicy(Qt::CustomContextMenu);
                    connect(markTable, &QTableView::clicked,
                            this, &UiManager::onMarkLogTableClicked);
                    connect(markTable, &QTableView::customContextMenuRequested,
                            this, &UiManager::onMarkLogContextMenu);
                    enableTableCopyAction(markTable);
                    markTable->viewport()->installEventFilter(m_mainWindow);
                }
            });

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
                        updateFilterHighlighting();
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

    // Batch-flush timer: coalesces incoming logcat lines into one model insert per 100 ms
    m_batchFlushTimer = new QTimer(this);
    m_batchFlushTimer->setInterval(100);
    connect(m_batchFlushTimer, &QTimer::timeout, this, &UiManager::flushPendingLines);

    applyFilters();
    updateStatusBar();
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
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Plain);
        line->setFixedWidth(2);
        line->setMinimumHeight(28);
        line->setMaximumHeight(34);
        const QString c = QStringLiteral("#5a5a5a");
        line->setStyleSheet(QStringLiteral("QFrame { color: %1; background: %1; border: none; margin: 2px 6px; }").arg(c));
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

    // Opacity effect for device dot fade in/out on connect/disconnect transitions.
    auto *devOpacity = new QGraphicsOpacityEffect(m_lblStatusDevices);
    devOpacity->setOpacity(1.0);
    m_lblStatusDevices->setGraphicsEffect(devOpacity);
    auto *devAnim = new QPropertyAnimation(devOpacity, "opacity", this);
    devAnim->setDuration(280);
    devAnim->setEasingCurve(QEasingCurve::InOutCubic);

    auto refreshDevices = [this, devOpacity, devAnim]() {
        const int n = m_ui->cmbDevice ? m_ui->cmbDevice->count() : 0;
        const QString dot = (n > 0) ? QStringLiteral("\u25cf") : QStringLiteral("\u25cb");
        const QString color = (n > 0) ? QStringLiteral("#10b981") : QStringLiteral("#6b7280");
        m_lblStatusDevices->setText(
            QStringLiteral("<span style='color:%1;'>%2</span> %3 device%4")
                .arg(color, dot).arg(n).arg(n == 1 ? "" : "s"));
        // Fade out → in to draw the eye when count changes.
        devAnim->stop();
        devAnim->setKeyValueAt(0.0, 0.35);
        devAnim->setKeyValueAt(0.5, 1.0);
        devAnim->setKeyValueAt(1.0, 1.0);
        devOpacity->setOpacity(0.35);
        devAnim->setStartValue(0.35);
        devAnim->setEndValue(1.0);
        devAnim->start();
    };
    refreshDevices();
    if (m_ui->cmbDevice) {
        connect(m_ui->cmbDevice, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [refreshDevices](int) { refreshDevices(); });
        // Also catch add/remove of items.
        connect(m_ui->cmbDevice->model(), &QAbstractItemModel::rowsInserted,
                this, [refreshDevices](const QModelIndex &, int, int) { refreshDevices(); });
        connect(m_ui->cmbDevice->model(), &QAbstractItemModel::rowsRemoved,
                this, [refreshDevices](const QModelIndex &, int, int) { refreshDevices(); });
        connect(m_ui->cmbDevice->model(), &QAbstractItemModel::modelReset,
                this, [refreshDevices]() { refreshDevices(); });
    }

    auto renderMonitor = [this]() {
        if (m_monitorActiveCount <= 0) {
            m_lblStatusMonitor->setText(
                QStringLiteral("<span style='color:#6b7280;'>\u25cb</span> Idle"));
        } else {
            // Pulse between indigo-400 and indigo-300 for a subtle "live" feel.
            const QString color = m_monitorPulseBright
                ? QStringLiteral("#a5b4fc")  // indigo-300 (bright)
                : QStringLiteral("#6366f1"); // indigo-500 (dim)
            m_lblStatusMonitor->setText(
                QStringLiteral("<span style='color:%1;'>\u25cf</span> Monitoring (%2)")
                    .arg(color).arg(m_monitorActiveCount));
        }
    };

    if (!m_monitorPulseTimer) {
        m_monitorPulseTimer = new QTimer(this);
        m_monitorPulseTimer->setInterval(700);
        connect(m_monitorPulseTimer, &QTimer::timeout, this, [this, renderMonitor]() {
            m_monitorPulseBright = !m_monitorPulseBright;
            renderMonitor();
        });
    }

    auto setMonitor = [this, renderMonitor](int active) {
        m_monitorActiveCount = active;
        m_monitorPulseBright = true;
        renderMonitor();
        if (active > 0) {
            if (!m_monitorPulseTimer->isActive()) m_monitorPulseTimer->start();
        } else {
            m_monitorPulseTimer->stop();
        }
    };
    setMonitor(0);
    if (m_configurationController) {
        connect(m_configurationController, &ConfigurationController::monitorStateChanged,
                this, setMonitor);

        // Indigo accent border around config tables that are actively monitoring,
        // matching the active-pane border in the split log view.
        auto applyTableBorder = [](QTableView *tv, bool on) {
            if (!tv) return;
            tv->setStyleSheet(on
                ? QStringLiteral("QTableView { border: 2px solid #818cf8; border-radius: 4px; }")
                : QStringLiteral("QTableView { border: 2px solid transparent; border-radius: 4px; }"));
        };
        connect(m_configurationController, &ConfigurationController::monitorTablesChanged,
                this, [this, applyTableBorder](bool s, bool p, bool d) {
            applyTableBorder(m_ui->tableSettings, s);
            applyTableBorder(m_ui->tableProperties, p);
            applyTableBorder(m_ui->tablePropertyDefinitions, d);
        });
    }

    m_ui->statusbar->addPermanentWidget(m_lblStatusDevices);
    m_ui->statusbar->addPermanentWidget(m_lblStatusMonitor);

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
    const QString currentDeviceId = m_ui->cmbDevice->currentData().toString();
    m_ui->cmbDevice->clear();

    if (devices.isEmpty()) {
        m_ui->cmbDevice->addItem("No devices found", "");
        m_ui->lblDeviceStatus->setStyleSheet("color: #f87171; font-size: 16px;");
        m_currentDeviceId = "";
        AdbManager::instance().setCurrentDeviceId("");

        // Stop any active monitor — there is nothing left to query.
        if (m_dumpsysController)       m_dumpsysController->stopMonitor();
        if (m_configurationController) m_configurationController->stopAllMonitors();

        // Clear device-specific data
        m_settingsModel->setSettings({});
        m_propertiesModel->setProperties({});
        m_availablePropertyDefinitions.clear();
        m_configurationController->updatePropertyNamesCompleter();
        m_ui->txtDumpsysCmdResult->clear();
        if (m_dumpsysController) m_dumpsysController->clearServices();
        m_ui->txtDumpsysService->clear();
        m_ui->txtDumpsysService->setPlaceholderText(tr("Service name (e.g. activity)"));
    } else {
        for (const AdbDevice &device : devices)
            m_ui->cmbDevice->addItem(device.name, device.id);
        m_ui->lblDeviceStatus->setStyleSheet("color: #34d399; font-size: 16px;");

        // Restore previous selection if available, otherwise pick first
        int index = m_ui->cmbDevice->findData(currentDeviceId);
        m_ui->cmbDevice->setCurrentIndex(index >= 0 ? index : 0);

        m_currentDeviceId = m_ui->cmbDevice->currentData().toString();
        AdbManager::instance().setCurrentDeviceId(m_currentDeviceId);

        // Auto-load tables when the first device becomes available.
        if (!m_currentDeviceId.isEmpty() && currentDeviceId.isEmpty()) {
            AdbManager::instance().fetchSettings(m_currentDeviceId);
            AdbManager::instance().fetchProperties(m_currentDeviceId);
            AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
        }
    }
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
