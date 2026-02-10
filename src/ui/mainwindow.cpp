#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "threadtimelogconverter.h"
#include "brieflogconverter.h"
#include "propertydefinitionconverter.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "highlightdelegate.h"
#include "tableconfig.h"
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QRegularExpression>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QCompleter>
#include <QStringListModel>
#include <QWheelEvent>
#include <QScrollBar>
#include <QTimer>
#include <QTextStream>
#include <QSplitter>
#include <QProcess>
#include "settingsdialog.h"
#include "tooltips.h"
#include "qtermwidget.h"
#include <QtConcurrent>
#include <algorithm>
#include <adbcommand.h>

// Row-resize threshold: below this count every row is sized to content on each
// resize pass; above it only the visible viewport is measured for performance.
static constexpr int ROW_RESIZE_THRESHOLD = 50'000;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_logModel(new LogModel(this))
    , m_markLogModel(new MarkLogModel(this))
    , m_settingsModel(new SettingsModel(this))
    , m_propertiesModel(new PropertiesModel(this))
    , m_propertyDefinitionModel(new PropertyDefinitionModel(this))
    , m_logConverter(new ThreadtimeLogConverter())
    , m_filterHistoryManager(new FilterHistoryManager(this))
{
    ui->setupUi(this);

    // Setup main log table view with model
    ui->tableLog->setModel(m_logModel);
    ui->tableLog->horizontalHeader()->setStretchLastSection(false);

    // Set marked rows pointer to model for highlighting
    m_logModel->setMarkedRows(&m_markedRows);

    // Set column widths for main log table
    using namespace TableConfig::LogColumns;
    using namespace TableConfig::ColumnWidths;
    ui->tableLog->setColumnWidth(DATE, LOG_DATE);
    ui->tableLog->setColumnWidth(TIME, LOG_TIME);
    ui->tableLog->setColumnWidth(PID, LOG_PID);
    ui->tableLog->setColumnWidth(TID, LOG_TID);
    ui->tableLog->setColumnWidth(PACKAGE, LOG_PACKAGE);
    ui->tableLog->setColumnWidth(LEVEL, LOG_LEVEL);
    
    // Set Message column to stretch to fill width, other columns are manually resizable
    ui->tableLog->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);

    // Setup mark log table view with model
    ui->tableMarkLog->setModel(m_markLogModel);
    ui->tableMarkLog->horizontalHeader()->setStretchLastSection(false);

    // Set column widths for mark log table
    ui->tableMarkLog->setColumnWidth(DATE, LOG_DATE);
    ui->tableMarkLog->setColumnWidth(TIME, LOG_TIME);
    ui->tableMarkLog->setColumnWidth(PID, LOG_PID);
    ui->tableMarkLog->setColumnWidth(TID, LOG_TID);
    ui->tableMarkLog->setColumnWidth(PACKAGE, LOG_PACKAGE);
    ui->tableMarkLog->setColumnWidth(LEVEL, LOG_LEVEL);
    // DELTA column always fits its content (e.g. "+123456 ms")
    ui->tableMarkLog->horizontalHeader()->setSectionResizeMode(DELTA, QHeaderView::ResizeToContents);
    
    // Set Message column to stretch to fill width; ΔTime (DELTA) is always visible
    ui->tableMarkLog->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);

    // Hide columns by default and sync filter group boxes (both tables, both models now ready)
    applyColumnVisibility({false, false, true, false, false, true, true, true});

    // Setup highlight delegates for filterable columns
    m_pidHighlightDelegate = new HighlightDelegate(this);
    m_packageHighlightDelegate = new HighlightDelegate(this);
    m_tagHighlightDelegate = new HighlightDelegate(this);
    m_messageHighlightDelegate = new HighlightDelegate(this);
    m_messageHighlightDelegate->setWordWrap(true);  // wrap long messages
    ui->tableLog->setItemDelegateForColumn(PID, m_pidHighlightDelegate);
    ui->tableLog->setItemDelegateForColumn(PACKAGE, m_packageHighlightDelegate);
    ui->tableLog->setItemDelegateForColumn(TAG, m_tagHighlightDelegate);
    ui->tableLog->setItemDelegateForColumn(MESSAGE, m_messageHighlightDelegate);

    // Debounce timer: recalculate row heights after TAG or MESSAGE column is resized
    // or new log data arrives, without hammering the UI on every event.
    m_rowResizeTimer = new QTimer(this);
    m_rowResizeTimer->setSingleShot(true);
    m_rowResizeTimer->setInterval(150);
    connect(m_rowResizeTimer, &QTimer::timeout, this, [this]() {
        resizeVisibleRows();
    });
    connect(ui->tableLog->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int section, int /*oldSize*/, int /*newSize*/) {
        if (section == TableConfig::LogColumns::TAG ||
            section == TableConfig::LogColumns::MESSAGE)
            m_rowResizeTimer->start();
    });

    // For large tables (> threshold) re-measure rows that scroll into view
    // since only visible rows are sized in that path.
    connect(ui->tableLog->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) {
        if (m_logModel->rowCount() > ROW_RESIZE_THRESHOLD)
            m_rowResizeTimer->start();
    });

    // Setup splitters
    ui->splitter->setSizes(QList<int>() << 250 << 1150);
    ui->splitterLogTables->setSizes(QList<int>() << 600 << 200);

    // Enable multi-row selection (click row header, Shift+Click for range)
    auto enableTableCopy = [this](QTableView *tv) {
        tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
        QAction *copyAction = new QAction(tv);
        copyAction->setShortcut(QKeySequence::Copy);
        copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(copyAction, &QAction::triggered, this, [this, tv]() { copyTableRows(tv); });
        tv->addAction(copyAction);
    };
    enableTableCopy(ui->tableLog);
    enableTableCopy(ui->tableMarkLog);

    // Enable context menu for main log table (right-click for filter popup)
    ui->tableLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableLog, &QTableView::customContextMenuRequested, this, &MainWindow::onTableContextMenu);

    // Connect double-click for marking logs
    connect(ui->tableLog, &QTableView::doubleClicked, this, &MainWindow::onLogTableDoubleClicked);

    // Connect single-click for displaying cell content
    connect(ui->tableLog, &QTableView::clicked, this, &MainWindow::onLogTableClicked);

    // Connect click on mark log table for scrolling to original
    connect(ui->tableMarkLog, &QTableView::clicked, this, &MainWindow::onMarkLogTableClicked);

    // Right-click context menu on mark table to set anchor row for ΔTime
    ui->tableMarkLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableMarkLog, &QTableView::customContextMenuRequested,
            this, &MainWindow::onMarkLogContextMenu);

    // Connect to AdbManager
    AdbManager &adbManager = AdbManager::instance();
    connect(&adbManager, &AdbManager::devicesChanged, this, &MainWindow::onDevicesChanged);
    connect(&adbManager, &AdbManager::logcatLineReceived, this, &MainWindow::onLogcatLineReceived);
    connect(&adbManager, &AdbManager::dmesgLineReceived,  this, &MainWindow::parseDmesgLine);
    connect(&adbManager, &AdbManager::dmesgStopped, this, [this]() {
        ui->btnKernel->setStyleSheet(QString());
        ui->statusbar->showMessage("Kernel log stopped", 3000);
    });
    connect(&adbManager, &AdbManager::dmesgFailed, this, [this](const QString &reason) {
        ui->btnKernel->setStyleSheet(QString());
        QMessageBox::warning(this, tr("Kernel Log — Root Required"), reason);
    });
    connect(&adbManager, &AdbManager::settingsFetched, this, &MainWindow::onSettingsFetched);
    connect(&adbManager, &AdbManager::propertiesFetched, this, &MainWindow::onPropertiesFetched);
    connect(&adbManager, &AdbManager::propertyDefinitionsFetched, this, &MainWindow::onPropertyDefinitionsFetched);

    // Initialize with current devices
    onDevicesChanged(adbManager.getConnectedDevices());

    setupConnections();

    // Issue #1/#9: filter history is managed by FilterHistoryManager, not MainWindow::eventFilter
    m_filterHistoryManager->track(ui->txtTagFilter);
    m_filterHistoryManager->track(ui->txtPidFilter);
    m_filterHistoryManager->track(ui->txtPackageFilter);
    m_filterHistoryManager->track(ui->txtFindMessage);
    m_filterHistoryManager->track(ui->txtPropertySearch);
    m_filterHistoryManager->track(ui->txtKeyword);

    // Install event filters for Shift+Scroll horizontal scrolling on table viewports
    ui->tableLog->viewport()->installEventFilter(this);
    ui->tableMarkLog->viewport()->installEventFilter(this);
    // Keep event filter on property search for completer-on-focus behaviour
    ui->txtPropertySearch->installEventFilter(this);

    // Initialize tab widget - start with ADB Logcat tab
    ui->tabWidget->setCurrentIndex(0);

    setupConfigurationTables();
    setupSDKTab();
    setupTerminal();
    setupDumpsys();
    setupCradleTab();

    // Enable multi-row selection + Ctrl+C copy for config/SDK tables
    for (QTableView *tv : {ui->tableSettings, ui->tableProperties, ui->tablePropertyDefinitions}) {
        tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
        QAction *copyAct = new QAction(tv);
        copyAct->setShortcut(QKeySequence::Copy);
        copyAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(copyAct, &QAction::triggered, this, [this, tv]() { copyTableRows(tv); });
        tv->addAction(copyAct);
    }

    // Batch-flush timer: coalesces incoming logcat lines into one model insert per 100 ms
    m_batchFlushTimer = new QTimer(this);
    m_batchFlushTimer->setInterval(100);
    connect(m_batchFlushTimer, &QTimer::timeout, this, &MainWindow::flushPendingLines);

    applyFilters();
    updateStatusBar();
    setupTooltips();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Use system theme icon when available (dev machine), fall back to bundled SVG (AppImage)
static QIcon themeIconWithFallback(const QString &themeName, const QString &resourcePath)
{
    return QIcon::fromTheme(themeName, QIcon(resourcePath));
}

void MainWindow::setupConnections()
{
    // Highlight – typing updates visuals; Enter/buttons navigate matches
    connect(ui->txtHighlight, &QLineEdit::textChanged,   this, &MainWindow::onHighlightChanged);
    connect(ui->txtHighlight, &QLineEdit::returnPressed, this, &MainWindow::onHighlightNext);
    connect(ui->btnHighlightNext, &QPushButton::clicked, this, &MainWindow::onHighlightNext);
    connect(ui->btnHighlightPrev, &QPushButton::clicked, this, &MainWindow::onHighlightPrev);

    // Filter connections - apply filter only when Enter is pressed
    connect(ui->txtKeyword,      &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtFindMessage,  &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtStartTime, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtEndTime, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtTagFilter, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtPackageFilter, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtPidFilter, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);

    // Configuration filter connections
    connect(ui->txtFilterSettings, &QLineEdit::textChanged, this, &MainWindow::onSettingsFilterChanged);
    connect(ui->txtFilterSettingsValue, &QLineEdit::textChanged, this, &MainWindow::onSettingsFilterChanged);
    connect(ui->txtFilterProperties, &QLineEdit::textChanged, this, &MainWindow::onPropertiesFilterChanged);
    connect(ui->txtFilterPropertiesValue, &QLineEdit::textChanged, this, &MainWindow::onPropertiesFilterChanged);
    connect(ui->btnRefreshSettings, &QPushButton::clicked, this, &MainWindow::onRefreshSettingsClicked);
    connect(ui->btnRefreshProperties, &QPushButton::clicked, this, &MainWindow::onRefreshPropertiesClicked);

    // SDK tab connections
    connect(ui->txtPropertySearch, &QLineEdit::returnPressed, this, &MainWindow::onAddPropertyDefinition);
    connect(ui->btnAddProperty, &QPushButton::clicked, this, &MainWindow::onAddPropertyDefinition);
    connect(ui->btnClearAllProperties, &QPushButton::clicked, this, &MainWindow::onClearAllPropertyDefinitions);
    connect(ui->btnFetchPropertyDefs, &QPushButton::clicked, this, &MainWindow::onRefreshPropertyDefinitionValues);

    connect(ui->radioVerbosePlus, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioV, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioD, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioI, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioW, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioE, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioA, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);

    // Button connections
    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->btnKernel, &QPushButton::clicked, this, &MainWindow::onKernelClicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::onSaveFileClicked);
    connect(ui->btnColumns, &QPushButton::clicked, this, &MainWindow::onColumnsClicked);
    connect(ui->btnAutoScroll, &QPushButton::toggled, this, &MainWindow::onAutoScrollToggled);
    connect(ui->btnToggleCellContent, &QPushButton::toggled, this, [this](bool checked) {
        ui->txtCellContent->setVisible(checked);
    });
    connect(ui->btnClearAllMarked, &QPushButton::clicked, this, &MainWindow::onClearAllMarkedLog);
    connect(ui->cmbDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDeviceChanged);

    // Terminal toggle
    connect(ui->btnToggleTerminal, &QPushButton::toggled, this, &MainWindow::onToggleTerminal);

    // Application settings
    connect(ui->btnAppSettings, &QPushButton::clicked, this, &MainWindow::onAppSettingsClicked);

    // Column visibility is managed inside App Settings — hide the separate btnColumns
    ui->btnColumns->setVisible(false);

    // File path input - load file when Enter is pressed
    connect(ui->txtFilePath, &QLineEdit::returnPressed, this, &MainWindow::onLoadFileClicked);
    connect(ui->btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);

    // Default file path: <app dir>/log.log
    // For AppImage the APPIMAGE env var holds the real .AppImage file path;
    // fall back to applicationDirPath() when running as a plain binary.
    const QString appimageEnv = qEnvironmentVariable("APPIMAGE");
    const QString appDir = appimageEnv.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : QFileInfo(appimageEnv).absolutePath();
    ui->txtFilePath->setText(appDir + "/log.log");

    // AdbManager signals for live (in-memory) logcat
    AdbManager &adb = AdbManager::instance();
    connect(&adb, &AdbManager::logcatStarted, this, [this]() {
        ui->btnStart->setStyleSheet(
            QStringLiteral("background-color: #c0392b; border: 1px solid #e74c3c; color: white;"));
        ui->statusbar->showMessage("Logcat started", 3000);
    });
    connect(&adb, &AdbManager::logcatStopped, this, [this]() {
        // Drain any lines that arrived just before the process exited
        m_batchFlushTimer->stop();
        flushPendingLines();

        ui->btnStart->setStyleSheet(QString());
        ui->statusbar->showMessage("Logcat stopped", 3000);
    });

    // Issue #7: async save result signals
    connect(&adb, &AdbManager::settingSaveResult,  this, &MainWindow::onSettingSaveResult);
    connect(&adb, &AdbManager::propertySaveResult, this, &MainWindow::onPropertySaveResult);
}

void MainWindow::setupConfigurationTables()
{
    // Setup Settings table with model
    using namespace TableConfig::SettingsColumns;
    using namespace TableConfig::ColumnWidths;
    ui->tableSettings->setModel(m_settingsModel);
    ui->tableSettings->horizontalHeader()->setStretchLastSection(false);
    ui->tableSettings->setColumnWidth(LINE, SETTINGS_LINE);
    ui->tableSettings->setColumnWidth(GROUP, SETTINGS_GROUP);
    ui->tableSettings->setColumnWidth(SETTING, SETTINGS_SETTING);
    ui->tableSettings->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    ui->tableSettings->setColumnWidth(ACTION, SETTINGS_ACTION);

    // Setup VALUE column delegate for editing with margin 1
    ValueDelegate *settingsDelegate = new ValueDelegate(this);
    ui->tableSettings->setItemDelegateForColumn(VALUE, settingsDelegate);

    // Setup Properties table with model
    ui->tableProperties->setModel(m_propertiesModel);
    ui->tableProperties->horizontalHeader()->setStretchLastSection(false);
    ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::LINE, PROPERTIES_LINE);
    ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::PROPERTY, PROPERTIES_PROPERTY);
    ui->tableProperties->horizontalHeader()->setSectionResizeMode(TableConfig::PropertiesColumns::VALUE, QHeaderView::Stretch);
    ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::ACTION, PROPERTIES_ACTION);

    // Setup VALUE column delegate for editing with margin 1
    ValueDelegate *propertiesDelegate = new ValueDelegate(this);
    ui->tableProperties->setItemDelegateForColumn(TableConfig::PropertiesColumns::VALUE, propertiesDelegate);

    // Setup splitters for configuration tab
    ui->splitterConfig->setSizes(QList<int>() << 500 << 700);
    ui->splitterConfigTables->setSizes(QList<int>() << 300 << 300);
}

void MainWindow::setupDumpsys()
{
    // Setup completer for service search — filters to contains-match as user types
    QCompleter *completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setModel(new QStringListModel(QStringList(), completer));

    completer->popup()->setStyleSheet(
        "QListView {"
        "    background-color: #2d2d30;"
        "    color: #cccccc;"
        "    border: 1px solid #3e3e42;"
        "    selection-background-color: #0e639c;"
        "    selection-color: #ffffff;"
        "}");

    ui->txtDumpsysService->setCompleter(completer);

    // Selecting a suggestion auto-runs dumpsys for that service
    connect(completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &text) {
                ui->txtDumpsysService->setText(text);
                onRunDumpsysClicked();
            });

    // Run on Enter
    connect(ui->txtDumpsysService, &QLineEdit::returnPressed,
            this, &MainWindow::onRunDumpsysClicked);

    // Refresh button re-runs dumpsys for whatever is currently in the field
    connect(ui->btnDumpsysRefresh, &QPushButton::clicked,
            this, &MainWindow::onRunDumpsysClicked);

    // Populate service list when AdbManager finishes `dumpsys -l`
    connect(&AdbManager::instance(), &AdbManager::dumpsysListFetched,
            this, &MainWindow::onDumpsysListFetched);

    // Auto-fetch everything when the Android tab is activated
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        QWidget *tab = ui->tabWidget->widget(index);
        if (tab == ui->tabConfiguration && !m_currentDeviceId.isEmpty()) {
            AdbManager::instance().fetchDumpsysList(m_currentDeviceId);
            AdbManager::instance().fetchSettings(m_currentDeviceId);
            AdbManager::instance().fetchProperties(m_currentDeviceId);
        } else if (tab == ui->tabSDK && !m_currentDeviceId.isEmpty()) {
            AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
        }
    });

    // Auto-run when package field is confirmed with Enter
    connect(ui->txtDumpsysPackage, &QLineEdit::returnPressed,
            this, &MainWindow::onRunDumpsysClicked);

    // Search: typing updates highlight; Enter advances to next match
    connect(ui->txtDumpsysSearch, &QLineEdit::textChanged, this, &MainWindow::onDumpsysSearchChanged);
    connect(ui->txtDumpsysSearch, &QLineEdit::returnPressed, this, &MainWindow::onDumpsysSearchNext);

    // Prev/Next buttons with icons
    connect(ui->btnDumpsysSearchPrev, &QPushButton::clicked, this, &MainWindow::onDumpsysSearchPrev);
    connect(ui->btnDumpsysSearchNext, &QPushButton::clicked, this, &MainWindow::onDumpsysSearchNext);

    // AdbManager signal
    connect(&AdbManager::instance(), &AdbManager::dumpsysFetched,
            this, &MainWindow::onDumpsysFetched);
}

// ---------------------------------------------------------------------------
// Cradle Manager tab setup and slots
// ---------------------------------------------------------------------------
void MainWindow::setupCradleTab()
{
    // Firmware source radio: toggle path field enabled state
    connect(ui->radioCustomFirmware, &QRadioButton::toggled, this, [this](bool checked) {
        ui->txtCradleFwPath->setEnabled(checked);
    });

    // Button connections
    connect(ui->btnCradleGet,            &QPushButton::clicked, this, &MainWindow::onCradleGetInfo);
    connect(ui->txtCradleKey,            &QLineEdit::returnPressed, this, &MainWindow::onCradleGetInfo);
    connect(ui->btnCradleQueryFirmware,  &QPushButton::clicked, this, &MainWindow::onCradleQueryFirmware);
    connect(ui->btnCradleUpdateFirmware, &QPushButton::clicked, this, &MainWindow::onCradleUpdateFirmware);
    connect(ui->btnCradleQuerySchedule,  &QPushButton::clicked, this, &MainWindow::onCradleQuerySchedule);
    connect(ui->btnCradleClearOutput,    &QPushButton::clicked,
            this, [this]() { ui->txtCradleOutput->clear(); ui->lblCradleLastCmd->setText("—"); });

    // AdbManager result signal
    connect(&AdbManager::instance(), &AdbManager::cradleCommandFinished,
            this, &MainWindow::onCradleCommandFinished);
}

// Helper: run a cradle command and update the "last command" label
static void runCradle(const QString &deviceId, const QStringList &args,
                      QLabel *lblLastCmd, QWidget *pendingWidget = nullptr)
{
    const QString display = "adb shell cmd cradle_manager " + args.join(' ');
    lblLastCmd->setText(display);
    if (pendingWidget) pendingWidget->setEnabled(false);
    AdbManager::instance().runCradleCommand(deviceId, args);
}

void MainWindow::onCradleGetInfo()
{
    if (m_currentDeviceId.isEmpty()) {
        ui->statusbar->showMessage("No device selected", 3000);
        return;
    }
    QStringList args = {"get"};
    const QString key = ui->txtCradleKey->text().trimmed();
    if (!key.isEmpty()) args << key;

    runCradle(m_currentDeviceId, args, ui->lblCradleLastCmd, ui->btnCradleGet);
}

void MainWindow::onCradleQueryFirmware()
{
    if (m_currentDeviceId.isEmpty()) {
        ui->statusbar->showMessage("No device selected", 3000);
        return;
    }
    QStringList args = {"query-firmware"};
    if (ui->radioDefaultFirmware->isChecked()) {
        args << "--default-firmware";
    } else {
        const QString path = ui->txtCradleFwPath->text().trimmed();
        if (path.isEmpty()) {
            ui->statusbar->showMessage("Please enter a firmware file path", 3000);
            return;
        }
        args << "--path" << path;
    }
    runCradle(m_currentDeviceId, args, ui->lblCradleLastCmd, ui->btnCradleQueryFirmware);
}

void MainWindow::onCradleUpdateFirmware()
{
    if (m_currentDeviceId.isEmpty()) {
        ui->statusbar->showMessage("No device selected", 3000);
        return;
    }
    QStringList args = {"update-firmware"};
    if (ui->radioDefaultFirmware->isChecked()) {
        args << "--default-firmware";
    } else {
        const QString path = ui->txtCradleFwPath->text().trimmed();
        if (path.isEmpty()) {
            ui->statusbar->showMessage("Please enter a firmware file path", 3000);
            return;
        }
        args << "--path" << path;
    }
    // Optional type filter — collect checked types
    QStringList types;
    if (ui->chkFwTypeApplication->isChecked()) types << "Application";
    if (ui->chkFwTypeBootloader->isChecked())  types << "Bootloader";
    if (ui->chkFwTypePreloader->isChecked())   types << "Preloader";
    if (ui->chkFwTypeWlc->isChecked())         types << "WLC";
    if (!types.isEmpty()) {
        args << "--type";
        args << types;
    }
    runCradle(m_currentDeviceId, args, ui->lblCradleLastCmd, ui->btnCradleUpdateFirmware);
}

void MainWindow::onCradleQuerySchedule()
{
    if (m_currentDeviceId.isEmpty()) {
        ui->statusbar->showMessage("No device selected", 3000);
        return;
    }
    QStringList days;
    if (ui->chkCradleMon->isChecked()) days << "1";
    if (ui->chkCradleTue->isChecked()) days << "2";
    if (ui->chkCradleWed->isChecked()) days << "3";
    if (ui->chkCradleThu->isChecked()) days << "4";
    if (ui->chkCradleFri->isChecked()) days << "5";
    if (ui->chkCradleSat->isChecked()) days << "6";
    if (ui->chkCradleSun->isChecked()) days << "7";
    if (days.isEmpty()) {
        ui->statusbar->showMessage("Please select at least one day", 3000);
        return;
    }
    QStringList args = {"query-schedule", "-d"};
    args << days;
    runCradle(m_currentDeviceId, args, ui->lblCradleLastCmd, ui->btnCradleQuerySchedule);
}

void MainWindow::onCradleCommandFinished(const QString &output, const QString &error)
{
    // Re-enable all cradle buttons
    for (QPushButton *btn : {ui->btnCradleGet, ui->btnCradleQueryFirmware,
                             ui->btnCradleUpdateFirmware, ui->btnCradleQuerySchedule}) {
        btn->setEnabled(true);
    }

    if (!error.isEmpty()) {
        ui->txtCradleOutput->appendPlainText("[ERROR]\n" + error);
        if (!output.isEmpty())
            ui->txtCradleOutput->appendPlainText("[OUTPUT]\n" + output);
    } else {
        ui->txtCradleOutput->appendPlainText(output.isEmpty() ? "(no output)" : output);
    }

    // Scroll to bottom
    QTextCursor c = ui->txtCradleOutput->textCursor();
    c.movePosition(QTextCursor::End);
    ui->txtCradleOutput->setTextCursor(c);

    ui->statusbar->showMessage("Cradle command completed", 2000);
}

void MainWindow::setupSDKTab()
{
    // Setup PropertyDefinition table with model
    using namespace TableConfig::PropertyDefColumns;
    using namespace TableConfig::ColumnWidths;
    ui->tablePropertyDefinitions->setModel(m_propertyDefinitionModel);
    ui->tablePropertyDefinitions->horizontalHeader()->setStretchLastSection(false);

    // Set column widths
    ui->tablePropertyDefinitions->setColumnWidth(NAME, PROPDEF_NAME);
    ui->tablePropertyDefinitions->setColumnWidth(ID, PROPDEF_ID);
    ui->tablePropertyDefinitions->setColumnWidth(SUPPORTED, PROPDEF_SUPPORTED);
    ui->tablePropertyDefinitions->setColumnWidth(VALUE, PROPDEF_DEFAULT);
    ui->tablePropertyDefinitions->setColumnWidth(NEED_REBOOT, PROPDEF_NEED_REBOOT);
    ui->tablePropertyDefinitions->setColumnWidth(TYPE, PROPDEF_TYPE);
    ui->tablePropertyDefinitions->setColumnWidth(READ_ONLY, PROPDEF_READ_ONLY);
    ui->tablePropertyDefinitions->setColumnWidth(SET_BUTTON, PROPDEF_SET_BUTTON);
    ui->tablePropertyDefinitions->setColumnWidth(GET_BUTTON, PROPDEF_GET_BUTTON);
    ui->tablePropertyDefinitions->setColumnWidth(REMOVE_BUTTON, PROPDEF_REMOVE_BUTTON);
    
    // Set DEFAULT column to stretch to fill available space
    // This ensures the table fills the tab width and REMOVE button stays at the right edge
    ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(NAME, QHeaderView::Stretch);
    ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(ID, QHeaderView::Fixed);

    // Apply default column visibility:
    // Show: ID, Name, Read Only, Value, Set, Get, Remove
    // Hide: Supported, Need Reboot, Type, Default
    const QVector<bool> defaultPropDefVis = {
        true,   // 0  ID
        true,   // 1  Name
        false,  // 2  Supported
        false,  // 3  Need Reboot
        false,  // 4  Type
        true,   // 5  Read Only
        false,  // 6  Default
        true,   // 7  Value
        true,   // 8  Set
        true,   // 9  Get
        true,   // 10 Remove
    };
    applyPropDefColumnVisibility(defaultPropDefVis);

    // Setup VALUE and DEFAULT column delegates for editing with margin 1
    ValueDelegate *valueDelegate = new ValueDelegate(this);
    ui->tablePropertyDefinitions->setItemDelegateForColumn(VALUE, valueDelegate);

    // When the user commits a value edit (e.g. pressing Enter), trigger set on device
    connect(m_propertyDefinitionModel, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex &topLeft, const QModelIndex &/*bottomRight*/, const QList<int> &/*roles*/) {
                if (topLeft.column() == TableConfig::PropertyDefColumns::VALUE) {
                    onSetPropertyDefinitionClicked(topLeft.row());
                }
            });

    // Setup completer for property search (will be populated when property definitions are fetched)
    QCompleter *completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);

    // Style the completer popup with dark theme
    completer->popup()->setStyleSheet(
        "QListView {"
        "    background-color: #2d2d30;"
        "    color: #cccccc;"
        "    border: 1px solid #3e3e42;"
        "    selection-background-color: #0e639c;"
        "    selection-color: #ffffff;"
        "}");

    ui->txtPropertySearch->setCompleter(completer);

    // Selecting a suggestion from the dropdown should also add the property
    connect(completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &text) {
                ui->txtPropertySearch->setText(text);
                onAddPropertyDefinition();
            });
}

// ---------------------------------------------------------------------------
// Terminal setup
// ---------------------------------------------------------------------------
void MainWindow::setupTerminal()
{
    // Create the terminal widget (1 = start shell immediately)
    m_terminal = new QTermWidget(1, ui->terminalContainer);
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_terminal->setTerminalFont(QFont(QStringLiteral("Monospace"), 10));
    m_terminal->setColorScheme(QStringLiteral("Linux"));
    m_terminal->setTerminalOpacity(1.0);

    // The global dark-theme stylesheet sets "QWidget { background-color: #1e1e1e; color: #cccccc; }"
    // which overrides the QPalette that TerminalDisplay::setBackgroundColor() tries to set.
    // paintEvent() reads palette().window().color() for the background fill, so we must
    // override the inherited rules on the container so the color scheme takes effect.
    ui->terminalContainer->setStyleSheet(
        QStringLiteral("QWidget { background-color: black; color: #b2b2b2; }"));

    // Install event filter on the inner TerminalDisplay widget so we can
    // intercept Ctrl+C / Ctrl+V before TerminalDisplay::keyPressEvent() eats them.
    // QTermWidget wraps a single TerminalDisplay child that actually receives focus.
    for (QWidget *child : m_terminal->findChildren<QWidget*>()) {
        if (child->metaObject()->className() == QStringLiteral("Konsole::TerminalDisplay")) {
            child->installEventFilter(this);
            break;
        }
    }

    // Add the terminal to the container's layout
    ui->terminalLayout->addWidget(m_terminal);

    // Start with terminal hidden
    ui->terminalContainer->hide();

    // Set initial splitter sizes: tabWidget gets all space
    ui->splitterMain->setSizes(QList<int>() << 800 << 0);

    // Handle terminal finished (e.g. user types 'exit')
    connect(m_terminal, &QTermWidget::finished, this, [this]() {
        ui->btnToggleTerminal->setChecked(false);
    });
}

void MainWindow::onToggleTerminal(bool checked)
{
    if (checked) {
        ui->terminalContainer->show();
        // Restore previous sizes, or use a reasonable default
        if (!m_savedSplitterSizes.isEmpty()) {
            ui->splitterMain->setSizes(m_savedSplitterSizes);
        } else {
            int totalHeight = ui->splitterMain->height();
            int termHeight = totalHeight / 3;
            ui->splitterMain->setSizes(QList<int>() << (totalHeight - termHeight) << termHeight);
        }
        m_terminal->setFocus();

        // If the shell has exited, restart it
        if (m_terminal->getShellPID() <= 0) {
            m_terminal->startShellProgram();
        }
    } else {
        // Save current sizes before hiding
        m_savedSplitterSizes = ui->splitterMain->sizes();
        ui->terminalContainer->hide();
    }
}

// ---------------------------------------------------------------------------
// Issue #4: Single factory for inline action buttons – used by all recreate* functions
// ---------------------------------------------------------------------------
QPushButton* MainWindow::createActionButton(const QString &label,
                                             const QString &tooltip,
                                             int maxWidth,
                                             QWidget *parent)
{
    QPushButton *btn = new QPushButton(label, parent);
    btn->setMaximumSize(maxWidth, 25);
    btn->setStyleSheet("QPushButton { font-size: 12px; padding: 2px; }");
    btn->setToolTip(tooltip);
    return btn;
}

void MainWindow::recreateSettingsButtons()
{
    // Clear existing buttons to prevent memory leaks and crashes
    // Use rowCount to respect filtered view
    using namespace TableConfig::SettingsColumns;
    int rowCount = m_settingsModel->rowCount();

    // Remove all existing index widgets
    for (int i = 0; i < rowCount; i++)
    {
        QWidget *oldWidget = ui->tableSettings->indexWidget(m_settingsModel->index(i, ACTION));
        if (oldWidget)
        {
            ui->tableSettings->setIndexWidget(m_settingsModel->index(i, ACTION), nullptr);
            oldWidget->deleteLater();
        }
    }

    // Add save buttons for each visible row
    for (int i = 0; i < rowCount; i++)
    {
        QPushButton *btnSave = createActionButton("", "Save this setting to device", 50, nullptr);
        btnSave->setIcon(QIcon(":/icons/download.svg"));
        ui->tableSettings->setIndexWidget(m_settingsModel->index(i, ACTION), btnSave);
        connect(btnSave, &QPushButton::clicked, this, [this, i]()
                { onSaveSettingClicked(i); });
    }
}

void MainWindow::recreatePropertiesButtons()
{
    // Clear existing buttons to prevent memory leaks and crashes
    // Use rowCount to respect filtered view
    using namespace TableConfig::PropertiesColumns;
    int rowCount = m_propertiesModel->rowCount();

    // Remove all existing index widgets
    for (int i = 0; i < rowCount; i++)
    {
        QWidget *oldWidget = ui->tableProperties->indexWidget(m_propertiesModel->index(i, ACTION));
        if (oldWidget)
        {
            ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, ACTION), nullptr);
            oldWidget->deleteLater();
        }
    }

    // Add save buttons for each visible row
    for (int i = 0; i < rowCount; i++)
    {
        QPushButton *btnSave = createActionButton("Save", "Save this property to device", 50, nullptr);
        btnSave->setIcon(QIcon(":/icons/download.svg"));
        ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, ACTION), btnSave);
        connect(btnSave, &QPushButton::clicked, this, [this, i]()
                { onSavePropertyClicked(i); });
    }
}

void MainWindow::onFilterChanged()
{
    applyFilters();
    updateFilterHighlighting();
}

void MainWindow::onHighlightChanged()
{
    m_highlightRow = -1; // reset navigation on keyword change
    updateFilterHighlighting();
}

// ---------------------------------------------------------------------------
// Highlight find navigation helpers
// ---------------------------------------------------------------------------
static bool rowMatchesKeywords(const LogEntry &entry, const QStringList &kws)
{
    for (const QString &kw : kws) {
        if (kw.isEmpty()) continue;
        if (entry.message.contains(kw, Qt::CaseInsensitive) ||
            entry.tag.contains(kw, Qt::CaseInsensitive) ||
            entry.package.contains(kw, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void MainWindow::onHighlightNext()
{
    const QString text = ui->txtHighlight->text().trimmed();
    if (text.isEmpty() || filteredLogs.isEmpty()) return;

    // Build flat keyword list (split by || and &&)
    QStringList kws;
    for (const QString &p : text.split("||", Qt::SkipEmptyParts))
        for (const QString &q : p.split("&&", Qt::SkipEmptyParts)) {
            const QString kw = q.trimmed();
            if (!kw.isEmpty()) kws << kw;
        }
    if (kws.isEmpty()) return;

    const int n = filteredLogs.size();
    const int start = (m_highlightRow + 1) % n;
    for (int i = 0; i < n; ++i) {
        const int row = (start + i) % n;
        if (rowMatchesKeywords(filteredLogs[row], kws)) {
            m_highlightRow = row;
            m_pendingCenterRow = row;
            ui->tableLog->selectRow(row);
            m_rowResizeTimer->start();
            ui->statusbar->showMessage(
                QString("Highlight: row %1 of %2").arg(row + 1).arg(n), 2000);
            return;
        }
    }
    ui->statusbar->showMessage("No highlight match found", 2000);
}

void MainWindow::onHighlightPrev()
{
    const QString text = ui->txtHighlight->text().trimmed();
    if (text.isEmpty() || filteredLogs.isEmpty()) return;

    QStringList kws;
    for (const QString &p : text.split("||", Qt::SkipEmptyParts))
        for (const QString &q : p.split("&&", Qt::SkipEmptyParts)) {
            const QString kw = q.trimmed();
            if (!kw.isEmpty()) kws << kw;
        }
    if (kws.isEmpty()) return;

    const int n = filteredLogs.size();
    const int start = (m_highlightRow <= 0 ? n : m_highlightRow) - 1;
    for (int i = 0; i < n; ++i) {
        const int row = ((start - i) % n + n) % n;
        if (rowMatchesKeywords(filteredLogs[row], kws)) {
            m_highlightRow = row;
            m_pendingCenterRow = row;
            ui->tableLog->selectRow(row);
            m_rowResizeTimer->start();
            ui->statusbar->showMessage(
                QString("Highlight: row %1 of %2").arg(row + 1).arg(n), 2000);
            return;
        }
    }
    ui->statusbar->showMessage("No highlight match found", 2000);
}

void MainWindow::onSettingsFilterChanged()
{
    QString nameFilter = ui->txtFilterSettings->text();
    QString valueFilter = ui->txtFilterSettingsValue->text();
    m_settingsModel->applyFilter(nameFilter, valueFilter);
    recreateSettingsButtons(); // Recreate buttons for filtered view
}

void MainWindow::onPropertiesFilterChanged()
{
    QString nameFilter = ui->txtFilterProperties->text();
    QString valueFilter = ui->txtFilterPropertiesValue->text();
    m_propertiesModel->applyFilter(nameFilter, valueFilter);
    recreatePropertiesButtons(); // Recreate buttons for filtered view
}

void MainWindow::onRefreshSettingsClicked()
{
    if (m_currentDeviceId.isEmpty())
    {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }

    // Don't clear filter - preserve it when refreshing
    // ui->txtFilterSettings->clear();
    // m_settingsModel->clearFilter();

    // Fetch settings from device via ADB
    AdbManager::instance().fetchSettings(m_currentDeviceId);
}

void MainWindow::onRefreshPropertiesClicked()
{
    if (m_currentDeviceId.isEmpty())
    {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }

    // Don't clear filter - preserve it when refreshing
    // ui->txtFilterProperties->clear();
    // m_propertiesModel->clearFilter();

    // Fetch properties from device via ADB
    AdbManager::instance().fetchProperties(m_currentDeviceId);
}

void MainWindow::onSettingsFetched(const QVector<SettingEntry> &settings)
{
    m_settingsModel->updateSettings(settings); // Update instead of replace to preserve filter
    recreateSettingsButtons();
}

void MainWindow::onPropertiesFetched(const QVector<PropertyEntry> &properties)
{
    m_propertiesModel->updateProperties(properties); // Update instead of replace to preserve filter
    recreatePropertiesButtons();
}

void MainWindow::onSaveSettingClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    if (row < 0 || row >= m_settingsModel->rowCount()) return;

    using namespace TableConfig::SettingsColumns;
    const QString group   = m_settingsModel->data(m_settingsModel->index(row, GROUP)).toString();
    const QString setting = m_settingsModel->data(m_settingsModel->index(row, SETTING)).toString();
    const QString value   = m_settingsModel->data(m_settingsModel->index(row, VALUE)).toString();

    // Issue #7: non-blocking – result arrives via onSettingSaveResult
    AdbManager::instance().saveSettingAsync(row, m_currentDeviceId, group, setting, value);
}

void MainWindow::onSettingSaveResult(int /*row*/, bool success,
                                      const QString &group, const QString &setting,
                                      const QString &newValue, const QString &verifiedValue,
                                      const QString &error)
{
    if (!success) {
        QMessageBox::warning(this, "Failed to Set Value",
                             QString("Failed to set %1.%2:\n%3").arg(group, setting, error));
        return;
    }
    if (verifiedValue != newValue) {
        QMessageBox::warning(this, "Value Not Set",
                             QString("Setting %1.%2 could not be set.\n"
                                     "Expected: %3\nActual: %4\n\n"
                                     "This setting may be read-only or require special permissions.")
                                 .arg(group, setting, newValue,
                                      verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        ui->statusbar->showMessage(
            QString("Successfully set %1.%2 = %3").arg(group, setting, newValue), 3000);
    }
}

void MainWindow::onSavePropertyClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    if (row < 0 || row >= m_propertiesModel->rowCount()) return;

    using namespace TableConfig::PropertiesColumns;
    const QString property = m_propertiesModel->data(m_propertiesModel->index(row, PROPERTY)).toString();
    const QString value    = m_propertiesModel->data(m_propertiesModel->index(row, VALUE)).toString();

    // Issue #7: non-blocking – result arrives via onPropertySaveResult
    AdbManager::instance().savePropertyAsync(row, m_currentDeviceId, property, value);
}

void MainWindow::onPropertySaveResult(int /*row*/, bool success,
                                       const QString &property,
                                       const QString &newValue, const QString &verifiedValue,
                                       const QString &error)
{
    if (!success) {
        QMessageBox::warning(this, "Failed to Set Property",
                             QString("Failed to set %1:\n%2").arg(property, error));
        return;
    }
    if (verifiedValue != newValue) {
        QMessageBox::warning(this, "Property Not Set",
                             QString("Property %1 could not be set.\n"
                                     "Expected: %2\nActual: %3\n\n"
                                     "This property may be read-only or require special permissions.")
                                 .arg(property, newValue,
                                      verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        ui->statusbar->showMessage(
            QString("Successfully set %1 = %2").arg(property, newValue), 3000);
    }
}

void MainWindow::applyFilters()
{
    filteredLogs.clear();
    m_filteredLogsIndex.clear();

    // Build criteria once – avoids rebuilding for every one of the (potentially
    // millions of) entries in the loop below.
    const FilterCriteria criteria = buildFilterCriteria();

    // Parallel filter: QtConcurrent::blockingFiltered evaluates passesFilter()
    // across all available CPU cores while preserving the original order.
    // With 8 cores this gives ~8x speedup vs the previous single-threaded loop.
    filteredLogs = QtConcurrent::blockingFiltered(allLogs,
        [&criteria, this](const LogEntry &entry) {
            return m_logFilter.passesFilter(entry, criteria);
        });

    // Rebuild the index – sequential but O(n) with no allocations per iteration
    m_filteredLogsIndex.reserve(filteredLogs.size());
    for (int i = 0; i < filteredLogs.size(); ++i)
        m_filteredLogsIndex[filteredLogs[i].id] = i;

    // Update model with filtered data
    m_logModel->setLogs(filteredLogs);
    
    // Rebuild m_markedRows to reflect which marked logs are in current filtered view
    m_markedRows.clear();
    
    // Get all marked logs from mark log model
    int markedCount = m_markLogModel->getMarkedCount();
    for (int i = 0; i < markedCount; ++i)
    {
        int allLogsIndex = m_markLogModel->getOriginalIndex(i);
        int filteredRow = findLogInFilteredLogs(allLogsIndex);
        if (filteredRow >= 0)
        {
            // This marked log is visible in current filtered view
            m_markedRows.insert(filteredRow);
        }
    }
    
    // Update highlighting with new marked rows
    m_logModel->setMarkedRows(&m_markedRows);

    // Update filter keyword highlighting
    updateFilterHighlighting();

    updateFilterCount();
    updateStatusBar();

    // m_logModel->setLogs() calls beginResetModel/endResetModel which resets
    // every row's cached height back to the default section size.  Kick the
    // debounce timer so visible rows are re-measured once the event loop settles.
    if (m_rowResizeTimer)
        m_rowResizeTimer->start();
}

bool MainWindow::passesFilter(const LogEntry &entry)
{
    FilterCriteria criteria = buildFilterCriteria();
    return m_logFilter.passesFilter(entry, criteria);
}

FilterCriteria MainWindow::buildFilterCriteria() const
{
    FilterCriteria criteria;

    // Issue #3: operator detection is centralised in FilterCriteria::applyFilter()
    // Keyword filter uses regex directly
    criteria.keywordFilter = ui->txtKeyword->text().trimmed();
    if (!criteria.keywordFilter.isEmpty()) {
        criteria.keywordRegex = QRegularExpression(
            criteria.keywordFilter,
            QRegularExpression::CaseInsensitiveOption);
    }
    FilterCriteria::applyFilter(criteria.messageFilter, criteria.messageOperator,
                                ui->txtFindMessage->text());
    FilterCriteria::applyFilter(criteria.tagFilter,     criteria.tagOperator,
                                ui->txtTagFilter->text());
    FilterCriteria::applyFilter(criteria.packageFilter, criteria.packageOperator,
                                ui->txtPackageFilter->text());
    FilterCriteria::applyFilter(criteria.pidFilter,     criteria.pidOperator,
                                ui->txtPidFilter->text());

    criteria.startTime = ui->txtStartTime->text();
    criteria.endTime   = ui->txtEndTime->text();
    criteria.tidFilter = "";
    criteria.tidOperator = FilterOperator::OR;

    if      (ui->radioVerbosePlus->isChecked()) criteria.minLevel = "V";
    else if (ui->radioV->isChecked())           criteria.minLevel = "V";
    else if (ui->radioD->isChecked())           criteria.minLevel = "D";
    else if (ui->radioI->isChecked())           criteria.minLevel = "I";
    else if (ui->radioW->isChecked())           criteria.minLevel = "W";
    else if (ui->radioE->isChecked())           criteria.minLevel = "E";
    else if (ui->radioA->isChecked())           criteria.minLevel = "A";

    // Pre-parse all string filters so passesFilter() does zero splitting/allocations
    // per-entry on the hot path (called up to 1M times during applyFilters).
    criteria.parsedMessage = ParsedFilter::build(criteria.messageFilter);
    criteria.parsedTag     = ParsedFilter::build(criteria.tagFilter);
    criteria.parsedPackage = ParsedFilter::build(criteria.packageFilter);
    criteria.parsedPid     = ParsedFilter::build(criteria.pidFilter);
    criteria.parsedTid     = ParsedFilter::build(criteria.tidFilter);
    // Pre-compute level ordinal: 0=V 1=D 2=I 3=W 4=E 5=A, -1=no filter
    static const auto lvlOrdinal = [](const QString &s) -> int {
        if (s == QLatin1String("V")) return 0;
        if (s == QLatin1String("D")) return 1;
        if (s == QLatin1String("I")) return 2;
        if (s == QLatin1String("W")) return 3;
        if (s == QLatin1String("E")) return 4;
        if (s == QLatin1String("A")) return 5;
        return -1;
    };
    criteria.minLevelIndex = lvlOrdinal(criteria.minLevel);

    return criteria;
}

void MainWindow::updateFilterCount()
{
    ui->lblFilterCount->setText(QString("Showing: %1 / %2")
                                    .arg(filteredLogs.size())
                                    .arg(allLogs.size()));
}

// Resize rows to fit their content.
// For tables with <= ROW_RESIZE_THRESHOLD rows every row is measured so that
// off-screen rows have the correct height before the user scrolls to them
// (important for accurate PositionAtCenter navigation).
// Above the threshold only the visible viewport is measured to avoid the
// O(N) cost becoming noticeable with very large log captures.
void MainWindow::resizeVisibleRows()
{
    auto *view = ui->tableLog;
    auto *vp   = view->viewport();
    const int rowCount = m_logModel->rowCount();
    if (rowCount == 0 || !vp) return;

    if (rowCount <= ROW_RESIZE_THRESHOLD) {
        // Resize every row so heights are correct everywhere, not just on-screen.
        view->resizeRowsToContents();
    } else {
        // Fall back to visible-only for huge tables.
        const int first = view->rowAt(0);
        if (first < 0) return;
        int last = view->rowAt(vp->height() - 1);
        if (last < 0 || last >= rowCount)
            last = rowCount - 1;
        for (int r = first; r <= last; ++r)
            view->resizeRowToContents(r);

        // Also resize the pending center row so PositionAtCenter is accurate
        // even when it is currently off-screen.
        if (m_pendingCenterRow >= 0 &&
            (m_pendingCenterRow < first || m_pendingCenterRow > last))
            view->resizeRowToContents(m_pendingCenterRow);
    }

    // If a navigation action requested a centered row, scroll to it now that
    // all relevant row heights have been finalised.
    if (m_pendingCenterRow >= 0) {
        view->scrollTo(m_logModel->index(m_pendingCenterRow, 0),
                       QAbstractItemView::PositionAtCenter);
        m_pendingCenterRow = -1;
    }
}

// Issue #11: read actual RSS memory from /proc/self/status instead of hardcoded 42
void MainWindow::updateMemoryUsage()
{
    QFile file("/proc/self/status");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith("VmRSS:")) {
            const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2)
                memoryUsage = parts[1].toLongLong() / 1024; // kB -> MB
            break;
        }
    }
}

void MainWindow::updateStatusBar()
{
    updateMemoryUsage();
    QString status = QString("UTF-8  Lines: %1    Mem: %2MB  \u25cf %3")
                         .arg(filteredLogs.size())
                         .arg(memoryUsage)
                         .arg(isPaused ? "Paused" : "Running");
    ui->statusbar->showMessage(status);
}

void MainWindow::onStartClicked()
{
    AdbManager &mgr = AdbManager::instance();

    if (mgr.isLogcatRunning()) {
        mgr.stopLogcat();
        // UI updates handled via logcatStopped signal
    } else {
        if (m_currentDeviceId.isEmpty()) {
            ui->statusbar->showMessage("No device selected", 3000);
            return;
        }

        // Clear table and all in-memory state so the live session starts fresh
        m_pendingLines.clear();
        allLogs.clear();
        filteredLogs.clear();
        m_allLogsIndex.clear();
        m_filteredLogsIndex.clear();
        m_nextLogId = 0;
        m_logModel->clear();
        m_markedRows.clear();
        m_markLogModel->clear();
        updateFilterCount();

        if (!mgr.startLogcat(m_currentDeviceId))
            ui->statusbar->showMessage("Failed to start logcat", 5000);
        // Success: UI updates handled via logcatStarted signal
    }
}

void MainWindow::onClearClicked()
{
    allLogs.clear();
    filteredLogs.clear();
    m_allLogsIndex.clear();
    m_filteredLogsIndex.clear();
    m_nextLogId = 0;
    m_logModel->clear();
    m_markedRows.clear();
    m_markLogModel->clear();
    updateFilterCount();
    updateStatusBar();
    
    // Clear device buffer if device is connected
    if (!m_currentDeviceId.isEmpty())
    {
        const QString adbPath = AdbManager::instance().getAdbPath();
        QProcess clearProcess;
        const bool isDmesg = AdbManager::instance().isDmesgRunning();
        clearProcess.start(adbPath,
            isDmesg ? AdbCommand::clearDmesg(m_currentDeviceId)
                    : AdbCommand::clearLogcat(m_currentDeviceId));

        const bool cleared = clearProcess.waitForFinished(3000);
        ui->statusbar->showMessage(
            cleared ? (isDmesg ? "Cleared local logs and device dmesg buffer"
                               : "Cleared local logs and device logcat buffer")
                    : "Cleared local logs (device buffer clear failed)",
            3000);
    }
    else
    {
        ui->statusbar->showMessage("Cleared local logs", 3000);
    }
}

void MainWindow::onColumnsClicked()
{
    // Create dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Column Visibility");
    dialog.setMinimumWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Add title label
    QLabel *titleLabel = new QLabel("Select columns to display:", &dialog);
    titleLabel->setStyleSheet("font-weight: bold; margin-bottom: 10px;");
    layout->addWidget(titleLabel);

    // Create checkboxes for each column (ΔTime is mark-table-only and always visible — excluded here)
    QVector<QCheckBox *> checkboxes;
    QStringList columnNames = {"Date", "Time", "PID", "TID", "Package", "Lvl", "Tag", "Message"};

    for (int i = 0; i < columnNames.size(); ++i)
    {
        QCheckBox *checkbox = new QCheckBox(columnNames[i], &dialog);
        checkbox->setChecked(!ui->tableLog->isColumnHidden(i));
        checkboxes.append(checkbox);
        layout->addWidget(checkbox);
    }

    // Add buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Apply dark theme to dialog
    dialog.setStyleSheet(
        "QDialog { background-color: #2d2d30; color: #cccccc; }"
        "QLabel { color: #cccccc; }"
        "QCheckBox { color: #cccccc; padding: 5px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator::unchecked { border: 1px solid #3e3e42; background-color: #1e1e1e; }"
        "QCheckBox::indicator::checked { border: 1px solid #007acc; background-color: #007acc; }"
        "QPushButton { background-color: #0e639c; border: none; border-radius: 3px; padding: 6px 12px; color: white; }"
        "QPushButton:hover { background-color: #1177bb; }");

    // Show dialog and apply changes to both tables
    if (dialog.exec() == QDialog::Accepted)
    {
        for (int i = 0; i < checkboxes.size(); ++i)
        {
            bool hidden = !checkboxes[i]->isChecked();
            ui->tableLog->setColumnHidden(i, hidden);
            ui->tableMarkLog->setColumnHidden(i, hidden);
        }
    }
}

void MainWindow::onAutoScrollToggled(bool checked)
{
    if (checked)
    {
        ui->tableLog->scrollToBottom();
    }
}

void MainWindow::onAppSettingsClicked()
{
    // Build log column-visibility vector (indices 0-7)
    QVector<bool> colVis;
    for (int c = 0; c < TableConfig::LogColumns::TOTAL_COLUMNS; ++c)
        colVis.append(!ui->tableLog->isColumnHidden(c));

    // Build property definition column-visibility vector (indices 0-10)
    QVector<bool> propDefColVis;
    for (int c = 0; c < TableConfig::PropertyDefColumns::TOTAL_COLUMNS; ++c)
        propDefColVis.append(!ui->tablePropertyDefinitions->isColumnHidden(c));

    SettingsDialog dlg(QApplication::font(), colVis, propDefColVis, this);
    if (dlg.exec() == QDialog::Accepted) {
        applyAppFont(dlg.selectedFont());
        applyColumnVisibility(dlg.columnVisibility());
        applyPropDefColumnVisibility(dlg.propDefColumnVisibility());
    }
}

void MainWindow::applyColumnVisibility(const QVector<bool> &vis)
{
    using namespace TableConfig::LogColumns;
    for (int c = 0; c < vis.size(); ++c) {
        ui->tableLog->setColumnHidden(c, !vis[c]);
        ui->tableMarkLog->setColumnHidden(c, !vis[c]);
    }
    // Sync filter group boxes: hide when their column is invisible
    if (vis.size() > PACKAGE) ui->groupBox_2->setVisible(vis[PACKAGE]); // Package
    if (vis.size() > PID)     ui->groupBox_3->setVisible(vis[PID]);     // PID
    if (vis.size() > TIME)    ui->groupBox_6->setVisible(vis[TIME]);    // Time
    if (vis.size() > TAG)     ui->groupBox->setVisible(vis[TAG]);       // Tag
    if (vis.size() > MESSAGE) ui->groupBox_5->setVisible(vis[MESSAGE]); // Message
}

void MainWindow::applyPropDefColumnVisibility(const QVector<bool> &vis)
{
    using namespace TableConfig::PropertyDefColumns;
    for (int c = 0; c < vis.size() && c < TOTAL_COLUMNS; ++c)
        ui->tablePropertyDefinitions->setColumnHidden(c, !vis[c]);
}

void MainWindow::setupTooltips()
{
    // Top bar
    ui->btnToggleTerminal->setToolTip(tr(Tooltips::btnToggleTerminal));
    ui->btnAppSettings->setToolTip(tr(Tooltips::btnAppSettings));

    // ADB Logcat toolbar
    ui->btnStart->setToolTip(tr(Tooltips::btnStart));
    ui->btnKernel->setToolTip(tr(Tooltips::btnKernel));
    ui->btnAutoScroll->setToolTip(tr(Tooltips::btnAutoScroll));
    ui->btnColumns->setToolTip(tr(Tooltips::btnColumns));
    ui->btnToggleCellContent->setToolTip(tr(Tooltips::btnToggleCellContent));
    ui->btnClear->setToolTip(tr(Tooltips::btnClear));
    ui->btnClearAllMarked->setToolTip(tr(Tooltips::btnClearAllMarked));
    ui->btnSave->setToolTip(tr(Tooltips::btnSave));
    ui->btnOpen->setToolTip(tr(Tooltips::btnOpen));

    // SDK tab
    ui->btnAddProperty->setToolTip(tr(Tooltips::btnAddProperty));
    ui->btnClearAllProperties->setToolTip(tr(Tooltips::btnClearAllProps));
    ui->btnFetchPropertyDefs->setToolTip(tr(Tooltips::btnFetchPropertyDefs));
}

void MainWindow::applyAppFont(const QFont &font)
{
    QApplication::setFont(font);

    // Force every existing widget to pick up the new font
    const auto topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget *w : topLevelWidgets) {
        w->setFont(font);
        // Propagate via stylesheet refresh so all children update
        w->setStyleSheet(w->styleSheet());
    }

    // Terminal keeps its own monospace font
    if (m_terminal) {
        QFont termFont(QStringLiteral("Monospace"), font.pointSize() > 0 ? font.pointSize() : 10);
        m_terminal->setTerminalFont(termFont);
    }
}

void MainWindow::onDeviceChanged(int index)
{
    // UI placeholder - device selection changed
    QString deviceName = ui->cmbDevice->itemText(index);
    QString deviceId = ui->cmbDevice->itemData(index).toString();

    // Update both local and AdbManager's current device
    m_currentDeviceId = deviceId;
    AdbManager::instance().setCurrentDeviceId(deviceId);

    if (!deviceId.isEmpty())
    {
        ui->statusbar->showMessage(QString("Selected device: %1").arg(deviceName), 2000);
        // Refresh dumpsys service list for the newly selected device
        m_dumpsysServices.clear();
        ui->txtDumpsysService->setPlaceholderText(tr("Loading services..."));
        ui->txtDumpsysService->clear();
        AdbManager::instance().fetchDumpsysList(deviceId);
    }
}

void MainWindow::onTableContextMenu(const QPoint &pos)
{
    QModelIndex index = ui->tableLog->indexAt(pos);
    if (!index.isValid())
        return;

    int column = index.column();
    QString value = m_logModel->data(index, Qt::DisplayRole).toString();
    QString filterType;
    QString displayName;

    // Issue #10: use named constants instead of magic column numbers
    using namespace TableConfig::LogColumns;
    switch (column)
    {
    case PID:
        filterType = "pid";
        displayName = "PID";
        break;
    case TID:
        filterType = "tid";
        displayName = "TID";
        break;
    case PACKAGE:
        filterType = "package";
        displayName = "Package";
        break;
    case TAG:
        filterType = "tag";
        displayName = "Tag";
        break;
    default:
        return;
    }

    // Create context menu
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2d2d30; color: #cccccc; border: 1px solid #3e3e42; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #0e639c; }");

    QString orText = QString("Add '%1' to %2 filter (OR)").arg(value).arg(displayName);
    QString andText = QString("Add '%1' to %2 filter (AND)").arg(value).arg(displayName);
    QAction *addFilterOrAction = menu.addAction(orText);
    QAction *addFilterAndAction = menu.addAction(andText);

    QAction *selected = menu.exec(ui->tableLog->viewport()->mapToGlobal(pos));
    if (selected == addFilterOrAction)
    {
        addToFilter(filterType, value, FilterOperator::OR);
    }
    else if (selected == addFilterAndAction)
    {
        addToFilter(filterType, value, FilterOperator::AND);
    }
}

void MainWindow::addToFilter(const QString &filterType, const QString &value, FilterOperator op)
{
    QLineEdit *filterField = nullptr;

    if (filterType == "tag")
    {
        filterField = ui->txtTagFilter;
    }
    else if (filterType == "package")
    {
        filterField = ui->txtPackageFilter;
    }
    else if (filterType == "pid")
    {
        filterField = ui->txtPidFilter;
    }
    else if (filterType == "tid")
    {
        // Note: There's no TID filter in the current UI, but we can add it to PID filter
        // or you could add a TID filter field to the UI
        filterField = ui->txtPidFilter;
    }

    if (!filterField)
        return;

    QString currentFilter = filterField->text().trimmed();
    QString newFilter;
    QString separator = (op == FilterOperator::OR) ? "||" : "&&";

    if (currentFilter.isEmpty())
    {
        newFilter = value;
    }
    else
    {
        // Check if value already exists in filter
        QStringList filterParts;
        if (currentFilter.contains("&&"))
        {
            filterParts = currentFilter.split("&&");
        }
        else if (currentFilter.contains("||"))
        {
            filterParts = currentFilter.split("||");
        }
        else
        {
            filterParts = currentFilter.split("|");
        }

        bool valueExists = false;
        for (const QString &part : filterParts)
        {
            if (part.trimmed() == value)
            {
                valueExists = true;
                break;
            }
        }

        if (!valueExists)
        {
            newFilter = currentFilter + separator + value;
        }
        else
        {
            // Value already in filter
            ui->statusbar->showMessage(QString("'%1' is already in the filter").arg(value), 2000);
            return;
        }
    }

    filterField->setText(newFilter);
    QString opText = (op == FilterOperator::OR) ? "OR" : "AND";
    ui->statusbar->showMessage(QString("Added '%1' to filter (%2)").arg(value).arg(opText), 2000);
}

void MainWindow::onDevicesChanged(const QList<AdbDevice> &devices)
{
    // Save current selection
    QString currentDeviceId = ui->cmbDevice->currentData().toString();

    // Clear and repopulate device list
    ui->cmbDevice->clear();

    if (devices.isEmpty())
    {
        ui->cmbDevice->addItem("No devices found", "");
        ui->lblDeviceStatus->setStyleSheet("color: #f87171; font-size: 16px;"); // Red
        m_currentDeviceId = "";
        AdbManager::instance().setCurrentDeviceId("");
    }
    else
    {
        for (const AdbDevice &device : devices)
        {
            ui->cmbDevice->addItem(device.name, device.id);
        }
        ui->lblDeviceStatus->setStyleSheet("color: #34d399; font-size: 16px;"); // Green

        // Restore previous selection if still available
        int index = ui->cmbDevice->findData(currentDeviceId);
        if (index >= 0)
        {
            ui->cmbDevice->setCurrentIndex(index);
        }
        else
        {
            // Select first device if previous selection not available
            ui->cmbDevice->setCurrentIndex(0);
        }

        // Update current device ID
        QString selectedDeviceId = ui->cmbDevice->currentData().toString();
        m_currentDeviceId = selectedDeviceId;
        AdbManager::instance().setCurrentDeviceId(selectedDeviceId);
    }
}

void MainWindow::onLogcatLineReceived(const QString &line)
{
    // Buffer the raw line; flushPendingLines() processes the batch every 100 ms
    m_pendingLines.append(line);
    if (!m_batchFlushTimer->isActive())
        m_batchFlushTimer->start();
}

void MainWindow::flushPendingLines()
{
    if (m_pendingLines.isEmpty()) {
        m_batchFlushTimer->stop();
        return;
    }

    // Swap out the buffer so new lines emitted during this call accumulate separately
    QVector<QString> lines;
    lines.swap(m_pendingLines);

    QVector<LogEntry> toAdd;
    toAdd.reserve(lines.size());

    // Build criteria once for the whole batch so we do not recreate it per line.
    const FilterCriteria criteria = buildFilterCriteria();

    for (const QString &line : lines) {
        LogEntry entry = m_logConverter->convert(line);
        if (!entry.isValid())
            continue;

        entry.id = ++m_nextLogId;
        m_allLogsIndex[entry.id] = allLogs.size();
        allLogs.append(entry);

        if (m_logFilter.passesFilter(entry, criteria)) {
            m_filteredLogsIndex[entry.id] = filteredLogs.size();
            filteredLogs.append(entry);
            toAdd.append(entry);
        }
    }

    if (!toAdd.isEmpty()) {
        m_logModel->addLogs(toAdd);   // single beginInsertRows/endInsertRows for the whole batch

        // Recalculate row heights for the newly added rows (debounced)
        m_rowResizeTimer->start();

        if (ui->btnAutoScroll->isChecked())
            ui->tableLog->scrollToBottom();

        updateFilterCount();
    }
}

void MainWindow::onLoadFileClicked()
{
    QString filePath = ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty())
    {
        ui->statusbar->showMessage("Please enter a file path", 3000);
        return;
    }

    loadLogsFromFile(filePath);
}

void MainWindow::onOpenFileClicked()
{
    QString currentPath = ui->txtFilePath->text().trimmed();
    QString defaultPath = currentPath.isEmpty() ? QDir::homePath() : currentPath;

    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Log File",
        defaultPath,
        "Log Files (*.log *.txt);;All Files (*.*)");

    if (!filePath.isEmpty())
    {
        ui->txtFilePath->setText(filePath);
        loadLogsFromFile(filePath);
    }
}

void MainWindow::onSaveFileClicked()
{
    QString filePath = ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty())
    {
        ui->statusbar->showMessage("Please enter a file path", 3000);
        return;
    }

    // Add .log extension if not present
    if (!filePath.endsWith(".log") && !filePath.endsWith(".txt"))
    {
        filePath += ".log";
    }

    QString errorMsg;
    bool success = m_fileManager.saveToFile(filePath, allLogs, errorMsg);

    if (success)
    {
        ui->statusbar->showMessage(QString("Saved %1 log entries to %2")
                                       .arg(allLogs.size())
                                       .arg(filePath),
                                   3000);
    }
    else
    {
        ui->statusbar->showMessage(QString("Failed to save: %1").arg(errorMsg), 5000);
    }
}

void MainWindow::loadLogsFromFile(const QString &filePath)
{
    if (m_isLoadingFile) {
        ui->statusbar->showMessage("File loading already in progress…", 3000);
        return;
    }
    m_isLoadingFile = true;

    // Disable the open button while the background task runs
    ui->btnOpen->setEnabled(false);
    ui->statusbar->showMessage("Loading log file…", 0);

    // Converters captured by value so they live on the worker thread safely
    QVector<LogConverterPtr> converters;
    converters.append(LogConverterPtr(new ThreadtimeLogConverter()));
    converters.append(LogConverterPtr(new BriefLogConverter()));

    // Run the expensive I/O + parsing on a thread-pool thread.
    // We also assign monotonic IDs and build the index here so the main thread
    // only has to move() the data into place (O(1) transfers).
    QFuture<FileLoadResult> future = QtConcurrent::run(
        [filePath, converters]() -> FileLoadResult {
            FileLoadResult result;
            result.filePath = filePath;
            FileManager fm;
            QVector<LogEntry> raw = fm.readFromFileAuto(
                filePath, converters, result.converter, result.errorMsg);
            result.parsedCount = fm.getLastParsedCount();
            result.lineCount   = fm.getLastLineCount();

            if (raw.isEmpty())
                return result;

            // Assign IDs and build the index on the worker thread
            quint64 nextId = 0;
            result.entries.reserve(raw.size());
            result.allLogsIndex.reserve(raw.size());
            for (LogEntry &e : raw) {
                e.id = ++nextId;
                result.allLogsIndex[e.id] = result.entries.size();
                result.entries.append(std::move(e));
            }
            result.nextLogId = nextId;
            return result;
        });

    // Wire up the watcher (created lazily once)
    if (!m_fileLoaderWatcher) {
        m_fileLoaderWatcher = new QFutureWatcher<FileLoadResult>(this);
        connect(m_fileLoaderWatcher, &QFutureWatcher<FileLoadResult>::finished,
                this, &MainWindow::onFileLoadFinished);
    } else {
        // Cancel any previous watcher (should already be finished, but be safe)
        m_fileLoaderWatcher->cancel();
        m_fileLoaderWatcher->waitForFinished();
    }
    m_fileLoaderWatcher->setFuture(future);
}

// Called on the main thread when the background load finishes.
void MainWindow::onFileLoadFinished()
{
    m_isLoadingFile = false;
    ui->btnOpen->setEnabled(true);

    // takeResult() moves ownership out of the QFuture (Qt6 only) so the large
    // QVector<LogEntry> and QHash are transferred, not copied.
    FileLoadResult res = m_fileLoaderWatcher->future().takeResult();

    if (res.entries.isEmpty() && !res.errorMsg.isEmpty()) {
        ui->statusbar->showMessage(
            QString("Failed to load file: %1").arg(res.errorMsg), 5000);
        return;
    }

    if (res.entries.isEmpty()) {
        ui->statusbar->showMessage("No valid log entries found in file", 5000);
        return;
    }

    const int entryCount = res.entries.size();

    // -----------------------------------------------------------------------
    // O(1) data hand-off: std::move the vectors and hashes – no element loops.
    // -----------------------------------------------------------------------
    allLogs          = std::move(res.entries);      // O(1)
    m_allLogsIndex   = std::move(res.allLogsIndex); // O(1)
    m_nextLogId      = res.nextLogId;

    m_markLogModel->clear();

    if (res.converter)
        m_logConverter = res.converter;

    // Clear filter widgets (no filter change signals yet – connections fire on
    // the next event loop tick, after we've fully updated the model).
    const QSignalBlocker b1(ui->txtFindMessage);
    const QSignalBlocker b2(ui->txtStartTime);
    const QSignalBlocker b3(ui->txtEndTime);
    const QSignalBlocker b4(ui->txtTagFilter);
    const QSignalBlocker b5(ui->txtPackageFilter);
    const QSignalBlocker b6(ui->txtPidFilter);
    ui->txtFindMessage->clear();
    ui->txtStartTime->clear();
    ui->txtEndTime->clear();
    ui->txtTagFilter->clear();
    ui->txtPackageFilter->clear();
    ui->txtPidFilter->clear();

    // -----------------------------------------------------------------------
    // Since all filters are cleared, filteredLogs == allLogs.
    // Qt's implicit sharing makes both assignments O(1) – no data is copied
    // until one of the containers is independently modified.
    // -----------------------------------------------------------------------
    filteredLogs         = allLogs;        // O(1) implicit share
    m_filteredLogsIndex  = m_allLogsIndex; // O(1) implicit share
    m_markedRows.clear();

    m_logModel->setLogs(filteredLogs);     // also O(1) – QVector assignment inside
    m_logModel->setMarkedRows(&m_markedRows);

    updateFilterHighlighting();
    updateFilterCount();
    updateStatusBar();

    ui->statusbar->showMessage(
        QString("Loaded %1 log entries from %2 (Format: %3, Parsed: %4/%5)")
            .arg(entryCount)
            .arg(res.filePath)
            .arg(res.converter ? res.converter->name() : "Unknown")
            .arg(res.parsedCount)
            .arg(res.lineCount),
        5000);

    // setLogs() above resets all cached row heights; re-measure visible rows.
    m_rowResizeTimer->start();
}

void MainWindow::onLogTableDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || index.row() >= filteredLogs.size())
    {
        return;
    }

    int filteredRow = index.row();
    const LogEntry &entry = filteredLogs[filteredRow];
    
    // Find this entry's index in allLogs (stable across filter changes)
    int allLogsIndex = findLogInAllLogs(entry);
    if (allLogsIndex < 0)
    {
        // Entry not found in allLogs (shouldn't happen)
        return;
    }

    // Toggle mark state using allLogs index
    if (m_markLogModel->isMarked(allLogsIndex))
    {
        // Unmark
        m_markedRows.remove(filteredRow);
        m_markLogModel->removeMarkedLog(allLogsIndex);
    }
    else
    {
        // Mark
        m_markedRows.insert(filteredRow);
        m_markLogModel->addMarkedLog(entry, allLogsIndex);
    }

    // Notify model to update highlighting
    m_logModel->setMarkedRows(&m_markedRows);
}

// Issue #5: shared helper – eliminates near-identical code in the two click handlers
void MainWindow::showCellContent(QTableView *tableView,
                                  const QAbstractItemModel *model,
                                  const QModelIndex &index)
{
    if (!ui->btnToggleCellContent->isChecked())
        return;

    const QString content = model->data(index, Qt::DisplayRole).toString();
    ui->txtCellContent->setPlainText(content);

    QTextCursor cursor = ui->txtCellContent->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->txtCellContent->setTextCursor(cursor);

    tableView->scrollTo(index, QAbstractItemView::EnsureVisible);

    // Issue #10: use named constant instead of magic number 7
    using namespace TableConfig::LogColumns;
    if (index.column() != MESSAGE) {
        QScrollBar *hBar = tableView->horizontalScrollBar();
        if (hBar && hBar->isVisible()) {
            const QRect cellRect = tableView->visualRect(index);
            const int viewWidth  = tableView->viewport()->width();
            if (cellRect.right() > viewWidth)
                hBar->setValue(hBar->value() + cellRect.right() - viewWidth + 50);
        }
    }
}

void MainWindow::onLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) { ui->txtCellContent->clear(); return; }
    showCellContent(ui->tableLog, m_logModel, index);
}

void MainWindow::onMarkLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) { ui->txtCellContent->clear(); return; }
    showCellContent(ui->tableMarkLog, m_markLogModel, index);
    int allLogsIndex = m_markLogModel->getOriginalIndex(index.row());
    if (allLogsIndex < 0 || allLogsIndex >= allLogs.size())
    {
        return;
    }
    
    // Find this log in the current filtered view
    int filteredRow = findLogInFilteredLogs(allLogsIndex);
    if (filteredRow < 0)
    {
        // Log is not in current filtered view - find nearest visible log
        filteredRow = findNearestVisibleLog(allLogsIndex);
        
        if (filteredRow < 0)
        {
            // No visible logs found at all (shouldn't happen if filteredLogs has any items)
            ui->statusbar->showMessage("No visible logs found with current filters", 3000);
            return;
        }
        
        // Found nearest visible log - scroll to it with message
        m_pendingCenterRow = filteredRow;
        ui->tableLog->selectRow(filteredRow);
        m_rowResizeTimer->start();
        ui->statusbar->showMessage("Marked log is filtered out - scrolled to nearest visible log", 3000);
        return;
    }

    // Scroll to the row and position it in the middle of the viewport
    m_pendingCenterRow = filteredRow;
    ui->tableLog->selectRow(filteredRow);
    m_rowResizeTimer->start();
}

// ---------------------------------------------------------------------------
// Clear all marked log entries
// ---------------------------------------------------------------------------

void MainWindow::onClearAllMarkedLog()
{
    m_markedRows.clear();
    m_markLogModel->clear();
    m_logModel->setMarkedRows(&m_markedRows);
}

// ---------------------------------------------------------------------------
// Mark table context menu — right-click to set the ΔTime anchor row
// ---------------------------------------------------------------------------

void MainWindow::onMarkLogContextMenu(const QPoint &pos)
{
    const QModelIndex idx = ui->tableMarkLog->indexAt(pos);
    if (!idx.isValid())
        return;

    const int clickedRow = idx.row();
    const bool alreadyAnchor = (clickedRow == m_markLogModel->anchorRow());

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2d2d30; color: #cccccc; border: 1px solid #3e3e42; }"
        "QMenu::item:selected { background-color: #094771; }"
        "QMenu::item:disabled { color: #666666; }");

    QAction *setAnchorAction = menu.addAction(tr("\u25b6  Set as start time (\u0394T\u00a0=\u00a00)"));
    setAnchorAction->setEnabled(!alreadyAnchor);
    if (alreadyAnchor)
        setAnchorAction->setText(tr("\u25b6  Start time (already set)"));

    menu.addSeparator();

    QAction *unmarkAction = menu.addAction(tr("\u2715  Unmark this row"));

    const QAction *chosen = menu.exec(ui->tableMarkLog->viewport()->mapToGlobal(pos));
    if (chosen == setAnchorAction && !alreadyAnchor) {
        m_markLogModel->setAnchorRow(clickedRow);
    } else if (chosen == unmarkAction) {
        const int allLogsIndex = m_markLogModel->getOriginalIndex(clickedRow);
        if (allLogsIndex >= 0) {
            const int filteredRow = findLogInFilteredLogs(allLogsIndex);
            m_markedRows.remove(filteredRow);
            m_markLogModel->removeMarkedLog(allLogsIndex);
            m_logModel->setMarkedRows(&m_markedRows);
        }
    }
}

// ---------------------------------------------------------------------------
// Issue #9: eventFilter decomposed into focused private helpers
// ---------------------------------------------------------------------------

bool MainWindow::handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent)
{
    if (!(wheelEvent->modifiers() & Qt::ShiftModifier))
        return false;

    QTableView *tableView = nullptr;
    if (obj == ui->tableLog->viewport())         tableView = ui->tableLog;
    else if (obj == ui->tableMarkLog->viewport()) tableView = ui->tableMarkLog;
    if (!tableView) return false;

    QScrollBar *hBar = tableView->horizontalScrollBar();
    if (!hBar) return false;

    const int steps  = wheelEvent->angleDelta().y() / 120;
    hBar->setValue(hBar->value() - steps * hBar->singleStep());
    return true;
}

bool MainWindow::handleCompleterFocusEvent(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj)
    Q_UNUSED(event)
    return false;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Intercept Ctrl+C / Ctrl+V in the terminal's TerminalDisplay widget
    if (m_terminal && event->type() == QEvent::KeyPress) {
        // Check if this object is a child of the terminal widget
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w && w->parent() && w->parent() == m_terminal) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            if (ke->modifiers() == Qt::ControlModifier) {
                if (ke->key() == Qt::Key_C) {
                    if (!m_terminal->selectedText(true).isEmpty()) {
                        m_terminal->copyClipboard();
                    } else {
                        m_terminal->sendText(QStringLiteral("\x03"));
                    }
                    return true;
                }
                if (ke->key() == Qt::Key_V) {
                    m_terminal->pasteClipboard();
                    return true;
                }
            }
        }
    }

    // Ctrl+C on any QTableView copies selected rows
    // (handled via QAction shortcuts added in constructor — no event filter needed)

    if (event->type() == QEvent::Wheel) {
        if (handleShiftScrollEvent(obj, static_cast<QWheelEvent*>(event)))
            return true;
    }
    handleCompleterFocusEvent(obj, event);
    return QMainWindow::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// Copy selected rows from a QTableView (visible columns, tab-separated)
// ---------------------------------------------------------------------------
void MainWindow::copyTableRows(QTableView *tableView)
{
    QAbstractItemModel *model = tableView->model();
    if (!model)
        return;

    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        // If no full rows selected, try to get the current row
        QModelIndex current = tableView->currentIndex();
        if (!current.isValid())
            return;
        selectedRows.append(model->index(current.row(), 0));
    }

    // Sort by row number
    std::sort(selectedRows.begin(), selectedRows.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });

    // Collect visible column indices
    QList<int> visibleCols;
    for (int c = 0; c < model->columnCount(); ++c) {
        if (!tableView->isColumnHidden(c))
            visibleCols.append(c);
    }

    QStringList lines;
    for (const QModelIndex &rowIdx : selectedRows) {
        int row = rowIdx.row();
        QStringList cells;
        for (int col : visibleCols) {
            cells << model->data(model->index(row, col), Qt::DisplayRole).toString();
        }
        lines << cells.join(QStringLiteral("\t"));
    }

    QApplication::clipboard()->setText(lines.join(QStringLiteral("\n")));
    ui->statusbar->showMessage(
        QString("Copied %1 row(s) to clipboard").arg(selectedRows.size()), 2000);
}

// ── Dumpsys slots ──────────────────────────────────────────────────────────

void MainWindow::onRunDumpsysClicked()
{
    if (m_currentDeviceId.isEmpty()) {
        ui->statusbar->showMessage("No device selected", 3000);
        return;
    }

    const QString service = ui->txtDumpsysService->text().trimmed();
    const QString pkg = ui->txtDumpsysPackage->text().trimmed();

    QString args = service;
    if (!pkg.isEmpty())
        args = args.isEmpty() ? pkg : args + " " + pkg;

    if (args.trimmed().isEmpty()) {
        ui->statusbar->showMessage("No service specified", 3000);
        return;
    }

    ui->txtDumpsysOutput->setPlainText("Running dumpsys " + args + "…");
    AdbManager::instance().fetchDumpsys(m_currentDeviceId, args);
}

void MainWindow::onDumpsysFetched(const QString &output)
{
    ui->txtDumpsysOutput->setPlainText(output);
    // Re-apply highlights if a search term is active
    const QString needle = ui->txtDumpsysSearch->text();
    applyDumpsysHighlights(needle);
    if (!needle.isEmpty()) {
        QTextCursor c = ui->txtDumpsysOutput->textCursor();
        c.movePosition(QTextCursor::Start);
        ui->txtDumpsysOutput->setTextCursor(c);
        ui->txtDumpsysOutput->find(needle);
    }
    ui->statusbar->showMessage(
        QString("Dumpsys: %1 lines").arg(output.count('\n')), 3000);
}

void MainWindow::applyDumpsysHighlights(const QString &needle)
{
    QList<QTextEdit::ExtraSelection> extras;

    if (!needle.isEmpty()) {
        QTextCharFormat fmt;
        fmt.setBackground(QColor("#b8860b"));   // dark-gold background
        fmt.setForeground(QColor("#ffffff"));

        QTextDocument *doc = ui->txtDumpsysOutput->document();
        QTextCursor cur(doc);
        int count = 0;
        while (!(cur = doc->find(needle, cur, QTextDocument::FindCaseSensitively)).isNull()) {
            QTextEdit::ExtraSelection sel;
            sel.format = fmt;
            sel.cursor = cur;
            extras.append(sel);
            ++count;
        }
        ui->statusbar->showMessage(QString("%1 match(es)").arg(count), 0);
    } else {
        ui->statusbar->clearMessage();
    }

    ui->txtDumpsysOutput->setExtraSelections(extras);
}

void MainWindow::onDumpsysSearchChanged()
{
    const QString needle = ui->txtDumpsysSearch->text();

    // Highlight all matches
    applyDumpsysHighlights(needle);

    if (needle.isEmpty()) return;

    // Scroll to the first match
    QTextCursor c = ui->txtDumpsysOutput->textCursor();
    c.movePosition(QTextCursor::Start);
    ui->txtDumpsysOutput->setTextCursor(c);
    ui->txtDumpsysOutput->find(needle);
}

void MainWindow::onDumpsysSearchNext()
{
    const QString needle = ui->txtDumpsysSearch->text();
    if (needle.isEmpty()) return;
    if (!ui->txtDumpsysOutput->find(needle)) {
        QTextCursor c = ui->txtDumpsysOutput->textCursor();
        c.movePosition(QTextCursor::Start);
        ui->txtDumpsysOutput->setTextCursor(c);
        ui->txtDumpsysOutput->find(needle);
    }
}

void MainWindow::onDumpsysSearchPrev()
{
    const QString needle = ui->txtDumpsysSearch->text();
    if (needle.isEmpty()) return;
    if (!ui->txtDumpsysOutput->find(needle, QTextDocument::FindBackward)) {
        QTextCursor c = ui->txtDumpsysOutput->textCursor();
        c.movePosition(QTextCursor::End);
        ui->txtDumpsysOutput->setTextCursor(c);
        ui->txtDumpsysOutput->find(needle, QTextDocument::FindBackward);
    }
}


void MainWindow::onDumpsysListFetched(const QStringList &services)
{
    m_dumpsysServices = services;

    // Update completer model
    QCompleter *completer = ui->txtDumpsysService->completer();
    if (completer)
        qobject_cast<QStringListModel *>(completer->model())->setStringList(services);

    ui->txtDumpsysService->setPlaceholderText(tr("Service name..."));

    ui->statusbar->showMessage(
        QString("Dumpsys: %1 services available").arg(services.size()), 3000);

    // If a service is already typed, re-run it; otherwise run the first one
    if (ui->txtDumpsysService->text().trimmed().isEmpty() && !services.isEmpty())
        ui->txtDumpsysService->setText(services.first());

    onRunDumpsysClicked();
}

// ── End Dumpsys slots ──────────────────────────────────────────────────────

void MainWindow::onSearchPropertyDefinition()
{
    // Search is handled by QCompleter automatically
    // This slot is triggered when user presses Enter
    QString searchText = ui->txtPropertySearch->text().trimmed();

    if (searchText.isEmpty())
    {
        return;
    }

    // Try to find the property in available definitions
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions)
    {
        if (propDef.name.compare(searchText, Qt::CaseInsensitive) == 0)
        {
            // Show property information in status bar
            ui->statusbar->showMessage(
                QString("Found: %1 (ID: %2, Supported: %3)")
                    .arg(propDef.name)
                    .arg(propDef.id)
                    .arg(propDef.isSupported ? "Yes" : "No"),
                3000);
            return;
        }
    }

    ui->statusbar->showMessage(QString("Property '%1' not found").arg(searchText), 3000);
}

void MainWindow::onAddPropertyDefinition()
{
    QString searchText = ui->txtPropertySearch->text().trimmed();

    if (searchText.isEmpty())
    {
        QMessageBox::warning(this, "No Property Selected", "Please enter or select a property name.");
        return;
    }

    // Find the property in available definitions
    PropertyDefinition selectedProp;
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions)
    {
        if (propDef.name.compare(searchText, Qt::CaseInsensitive) == 0)
        {
            selectedProp = propDef;
            break;
        }
    }

    if (!selectedProp.isValid())
    {
        QMessageBox::warning(this, "Property Not Found",
                             QString("Property '%1' not found in available definitions.\nPlease fetch property definitions first.").arg(searchText));
        return;
    }

    // Add to model
    int oldRowCount = m_propertyDefinitionModel->rowCount();
    m_propertyDefinitionModel->addPropertyDefinition(selectedProp);
    int newRowCount = m_propertyDefinitionModel->rowCount();

    if (newRowCount > oldRowCount)
    {
        const int row = newRowCount - 1;
        using namespace TableConfig::PropertyDefColumns;

        static const QString iconBtnStyle =
            "QPushButton { padding: 0px; border: none; background: transparent; border-radius: 4px; }"
            "QPushButton:hover { background-color: rgba(255,255,255,40); }"
            "QPushButton:pressed { background-color: rgba(255,255,255,70); }";

        auto makeCenteredBtn = [&](const QString &icon, const QString &tooltip) -> std::pair<QWidget*, QPushButton*> {
            QPushButton *btn = new QPushButton(nullptr);
            btn->setIcon(QIcon(icon));
            btn->setIconSize(QSize(18, 18));
            btn->setFixedSize(28, 28);
            btn->setFlat(true);
            btn->setStyleSheet(iconBtnStyle);
            btn->setToolTip(tooltip);
            QWidget *cell = new QWidget();
            cell->setStyleSheet("background: transparent;");
            QHBoxLayout *lay = new QHBoxLayout(cell);
            lay->setContentsMargins(0, 0, 0, 0);
            lay->setAlignment(Qt::AlignCenter);
            lay->addWidget(btn);
            return {cell, btn};
        };

        auto [cellSet, btnSet] = makeCenteredBtn(":/icons/download.svg", "Set property value");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, SET_BUTTON), cellSet);
        connect(btnSet, &QPushButton::clicked, this, [this, row]()
                { onSetPropertyDefinitionClicked(row); });

        auto [cellGet, btnGet] = makeCenteredBtn(":/icons/refresh.svg", "Get property value");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, GET_BUTTON), cellGet);
        connect(btnGet, &QPushButton::clicked, this, [this, row]()
                { onGetPropertyDefinitionClicked(row); });

        auto [cellRemove, btnRemove] = makeCenteredBtn(":/icons/edit-delete.svg", "Remove property from list");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, REMOVE_BUTTON), cellRemove);
        connect(btnRemove, &QPushButton::clicked, this, [this, row]()
                { onRemovePropertyDefinitionClicked(row); });

        ui->statusbar->showMessage(QString("Added property: %1").arg(selectedProp.name), 2000);
        ui->txtPropertySearch->clear();
    }
    else
    {
        ui->statusbar->showMessage(QString("Property '%1' already in list").arg(selectedProp.name), 2000);
    }
}

void MainWindow::onRefreshPropertyDefinitionValues()
{
    if (m_currentDeviceId.isEmpty())
    {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }

    const QVector<PropertyDefinition> properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (properties.isEmpty())
    {
        ui->statusbar->showMessage("No property definitions loaded.", 3000);
        return;
    }

    ui->btnFetchPropertyDefs->setEnabled(false);
    ui->statusbar->showMessage(QString("Refreshing values for %1 properties...").arg(properties.size()), 0);

    const QString deviceId = m_currentDeviceId;

    // Run all ADB "get" calls in the background so the UI stays responsive.
    // Collect (row, updatedProp) pairs, then apply them on the main thread.
    QtConcurrent::run([this, properties, deviceId]() {
        using Pair = std::pair<int, PropertyDefinition>;
        QVector<Pair> results;
        results.reserve(properties.size());

        for (int row = 0; row < properties.size(); ++row) {
            const PropertyDefinition &propDef = properties[row];
            const QString queryKey = propDef.id.isEmpty() ? propDef.name : propDef.id;

            QString output, error;
            bool ok = AdbManager::instance().getPropertyDefinitionValue(deviceId, queryKey, output, error);
            if (!ok) continue;

            // Try parsing each output line, fall back to full output
            PropertyDefinition updated;
            const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
            for (const QString &ln : lines) {
                updated = PropertyDefinitionConverter::parseLine(ln.trimmed());
                if (updated.isValid()) break;
            }
            if (!updated.isValid())
                updated = PropertyDefinitionConverter::parseLine(output);
            if (!updated.isValid()) continue;

            if (updated.name.isEmpty())
                updated.name = propDef.name;

            results.append({row, updated});
        }

        // Apply on the main thread
        QMetaObject::invokeMethod(this, [this, results]() {
            for (const auto &[row, updated] : results)
                m_propertyDefinitionModel->updatePropertyDefinition(row, updated);

            ui->btnFetchPropertyDefs->setEnabled(true);
            ui->statusbar->showMessage(
                QString("Refreshed values for %1 / %2 properties")
                    .arg(results.size())
                    .arg(m_propertyDefinitionModel->rowCount()),
                3000);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::onFetchPropertyDefinitions()
{
    if (m_currentDeviceId.isEmpty())
    {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }

    ui->statusbar->showMessage("Fetching property definitions...", 0);

    // Fetch property definitions from device via ADB
    AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
}

void MainWindow::onPropertyDefinitionsFetched(const QVector<PropertyDefinition> &propertyDefinitions)
{
    m_availablePropertyDefinitions = propertyDefinitions;
    updatePropertyNamesCompleter();

    ui->statusbar->showMessage(
        QString("Fetched %1 property definitions").arg(propertyDefinitions.size()),
        3000);
}

void MainWindow::onGetPropertyDefinitionClicked(int row)
{
    if (m_currentDeviceId.isEmpty())
    {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }

    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size())
    {
        return;
    }

    const PropertyDefinition &propDef = properties[row];

    // Use the numeric id for the lookup (e.g. "cmd configuration_manager get 786447")
    const QString queryKey = propDef.id.isEmpty() ? propDef.name : propDef.id;

    QString output;
    QString error;
    bool success = AdbManager::instance().getPropertyDefinitionValue(m_currentDeviceId, queryKey, output, error);

    if (success)
    {
        // The output may contain multiple lines (e.g. status prefix).
        // Try each non-empty line until we get a valid parse.
        PropertyDefinition updatedProp;
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &ln : lines) {
            updatedProp = PropertyDefinitionConverter::parseLine(ln.trimmed());
            if (updatedProp.isValid())
                break;
        }
        // Fallback: try the full output as one block
        if (!updatedProp.isValid())
            updatedProp = PropertyDefinitionConverter::parseLine(output);

        if (updatedProp.isValid())
        {
            // Preserve the name from the original property in case it's not in the output
            if (updatedProp.name.isEmpty())
                updatedProp.name = propDef.name;

            // Update all columns in the model with the parsed property definition
            m_propertyDefinitionModel->updatePropertyDefinition(row, updatedProp);

            ui->statusbar->showMessage(QString("Updated property: %1").arg(updatedProp.name), 2000);
        }
        else
        {
            QMessageBox::warning(this, "Parse Error",
                                 QString("Failed to parse output for %1\n\nRaw output:\n%2")
                                 .arg(propDef.name, output.left(500)));
        }
    }
    else
    {
        QMessageBox::warning(this, "Failed to Get Value",
                             QString("Failed to get %1:\n%2").arg(propDef.name, error));
    }
}
    
void MainWindow::onSetPropertyDefinitionClicked(int row)
{
    if (m_currentDeviceId.isEmpty())
    {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }

    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size())
    {
        return;
    }

    const PropertyDefinition &propDef = properties[row];

    // Check if property is read-only
    if (propDef.readOnly)
    {
        QMessageBox::warning(this, "Read-Only Property",
                             QString("Property '%1' is marked as read-only and cannot be modified.").arg(propDef.name));
        return;
    }

    // Get current value from the VALUE column
    using namespace TableConfig::PropertyDefColumns;
    QString value = m_propertyDefinitionModel->data(m_propertyDefinitionModel->index(row, VALUE), Qt::DisplayRole).toString();
    value = value.replace("\"", "\\\"");
    value = value.replace("{", "\\{");

    // Set the property value
    QString error;
    bool success = AdbManager::instance().setPropertyDefinitionValue(m_currentDeviceId, propDef.id, value, error);

    if (success)
    {
        ui->statusbar->showMessage(QString("Set %1 = %2").arg(propDef.id, value), 3000);
    }
    else
    {
        QMessageBox::warning(this, "Failed to Set Value",
                             QString("Failed to set %1:\n%2").arg(propDef.id, error));
    }
}

void MainWindow::onRemovePropertyDefinitionClicked(int row)
{
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size())
    {
        return;
    }

    const PropertyDefinition &propDef = properties[row];

    m_propertyDefinitionModel->removePropertyDefinition(row);


    ui->statusbar->showMessage(QString("Removed property: %1").arg(propDef.name), 2000);
}

void MainWindow::onClearAllPropertyDefinitions()
{
    if (m_propertyDefinitionModel->rowCount() == 0)
    {
        QMessageBox::information(this, "Empty List", "The property list is already empty.");
        return;
    }

    // Confirm clear all
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Clear All Properties",
                                  QString("Remove all %1 properties from the list?").arg(m_propertyDefinitionModel->rowCount()),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        m_propertyDefinitionModel->clear();
        ui->statusbar->showMessage("Cleared all properties", 2000);
    }
}

void MainWindow::updatePropertyNamesCompleter()
{
    // Extract property names from available definitions
    QStringList propertyNames;
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions)
    {
        propertyNames.append(propDef.name);
    }

    QCompleter *completer = ui->txtPropertySearch->completer();
    if (completer)
    {
        if (propertyNames.isEmpty())
        {
            // If list is empty, set an empty model so completer shows nothing
            completer->setModel(new QStringListModel(QStringList(), completer));
        }
        else
        {
            // Set the model with property names
            QStringListModel *model = new QStringListModel(propertyNames, completer);
            completer->setModel(model);
        }
    }
}

void MainWindow::updateFilterHighlighting()
{
    // Helper function to extract keywords from filter text (splits by || and &&)
    auto extractKeywords = [](const QString &filterText) -> QStringList {
        QStringList keywords;
        if (filterText.isEmpty()) {
            return keywords;
        }
        
        // Split by || first (OR operator)
        QStringList orParts = filterText.split("||", Qt::SkipEmptyParts);
        for (const QString &orPart : orParts) {
            // Then split each part by && (AND operator)
            QStringList andParts = orPart.split("&&", Qt::SkipEmptyParts);
            for (QString keyword : andParts) {
                keyword = keyword.trimmed();
                if (!keyword.isEmpty()) {
                    keywords.append(keyword);
                }
            }
        }
        keywords.removeDuplicates();
        return keywords;
    };
    
    // Extract keywords from each filter field
    QStringList messageKeywords = extractKeywords(ui->txtFindMessage->text());
    QStringList tagKeywords = extractKeywords(ui->txtTagFilter->text());
    QStringList packageKeywords = extractKeywords(ui->txtPackageFilter->text());
    QStringList pidKeywords = extractKeywords(ui->txtPidFilter->text());
    
    // Also include highlight field keywords in message, tag and package columns
    QStringList highlightKeywords = extractKeywords(ui->txtHighlight->text());
    // Keyword filter also acts as a highlight source for the same three columns
    QStringList keywordFilterKws = extractKeywords(ui->txtKeyword->text());
    highlightKeywords.append(keywordFilterKws);
    highlightKeywords.removeDuplicates();

    messageKeywords.append(highlightKeywords);
    messageKeywords.removeDuplicates();
    tagKeywords.append(highlightKeywords);
    tagKeywords.removeDuplicates();
    packageKeywords.append(highlightKeywords);
    packageKeywords.removeDuplicates();
    
    // Update each delegate with its keywords
    if (pidKeywords.isEmpty()) {
        m_pidHighlightDelegate->clearKeywords();
    } else {
        m_pidHighlightDelegate->setKeywords(pidKeywords);
    }
    
    if (packageKeywords.isEmpty()) {
        m_packageHighlightDelegate->clearKeywords();
    } else {
        m_packageHighlightDelegate->setKeywords(packageKeywords);
    }
    
    if (tagKeywords.isEmpty()) {
        m_tagHighlightDelegate->clearKeywords();
    } else {
        m_tagHighlightDelegate->setKeywords(tagKeywords);
    }
    
    if (messageKeywords.isEmpty()) {
        m_messageHighlightDelegate->clearKeywords();
    } else {
        m_messageHighlightDelegate->setKeywords(messageKeywords);
    }
    
    // Force repaint of the table to show/update highlights
    ui->tableLog->viewport()->update();
}

// ---------------------------------------------------------------------------
// Issue #6: O(1) id-keyed lookup replaces O(n) content comparison
// ---------------------------------------------------------------------------

int MainWindow::findLogInAllLogs(const LogEntry &entry) const
{
    return m_allLogsIndex.value(entry.id, -1);
}

int MainWindow::findLogInFilteredLogs(int allLogsIndex) const
{
    if (allLogsIndex < 0 || allLogsIndex >= allLogs.size())
        return -1;
    return m_filteredLogsIndex.value(allLogs[allLogsIndex].id, -1);
}

int MainWindow::findNearestVisibleLog(int allLogsIndex) const
{
    if (allLogsIndex < 0 || allLogsIndex >= allLogs.size())
        return -1;

    // Search forward first, then backward
    for (int i = allLogsIndex + 1; i < allLogs.size(); ++i) {
        const int row = m_filteredLogsIndex.value(allLogs[i].id, -1);
        if (row >= 0) return row;
    }
    for (int i = allLogsIndex - 1; i >= 0; --i) {
        const int row = m_filteredLogsIndex.value(allLogs[i].id, -1);
        if (row >= 0) return row;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Dmesg / Kernel log
// ---------------------------------------------------------------------------

void MainWindow::onKernelClicked()
{
    AdbManager &mgr = AdbManager::instance();

    // Toggle: if already running, stop
    if (mgr.isDmesgRunning()) {
        mgr.stopDmesg();
        return;
    }

    if (m_currentDeviceId.isEmpty()) {
        ui->statusbar->showMessage("No device selected", 3000);
        return;
    }

    // Stop logcat if it's running (they share the same table)
    if (mgr.isLogcatRunning())
        mgr.stopLogcat();

    // Clear table and all in-memory state
    m_pendingLines.clear();
    allLogs.clear();
    filteredLogs.clear();
    m_allLogsIndex.clear();
    m_filteredLogsIndex.clear();
    m_nextLogId = 0;
    m_logModel->clear();
    m_markedRows.clear();
    m_markLogModel->clear();
    updateFilterCount();

    if (!mgr.startDmesg(m_currentDeviceId)) {
        ui->statusbar->showMessage("Failed to start dmesg", 5000);
        return;
    }

    ui->btnKernel->setStyleSheet(
        QStringLiteral("background-color: #c0392b; border: 1px solid #e74c3c; color: white;"));
    ui->statusbar->showMessage("Kernel log started (adb shell dmesg -w)", 5000);
}

void MainWindow::parseDmesgLine(const QString &line)
{
    // Kernel log format examples:
    //   [1028391.266821] [UFW BLOCK] IN=enp0s31f6 OUT= ...
    //   [1028391.266821] some message without tag brackets
    static const QRegularExpression re(
        R"(^\[\s*([\d.]+)\]\s*(?:\[([^\]]*)\])?\s*(.*)$)");

    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch())
        return;

    LogEntry entry;
    entry.time    = m.captured(1).trimmed();
    entry.tag     = m.captured(2).trimmed();
    entry.message = m.captured(3).trimmed();

    if (entry.tag.isEmpty())
        entry.tag = QStringLiteral("KERNEL");

    entry.level = QStringLiteral("I");

    // Feed through the same batch pipeline as logcat lines
    entry.id = ++m_nextLogId;
    m_allLogsIndex[entry.id] = allLogs.size();
    allLogs.append(entry);

    if (passesFilter(entry)) {
        m_filteredLogsIndex[entry.id] = filteredLogs.size();
        filteredLogs.append(entry);
        m_logModel->addLog(entry);

        if (ui->btnAutoScroll->isChecked())
            ui->tableLog->scrollToBottom();

        updateFilterCount();
    }
}
