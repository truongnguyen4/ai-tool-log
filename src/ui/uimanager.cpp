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
#include "qtermwidget.h"
#include "valuedelegate.h"
#include "propertydefinitionbackend.h"
#include <adbcommand.h>

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QScrollBar>
#include <QRegularExpression>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
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
#include <QTextStream>
#include <QSplitter>
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
#include <QFrame>
#include <QGridLayout>
#include <QDrag>
#include <QMimeData>
#include <algorithm>
#include "toggleswitch.h"

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

void UiManager::initialize()
{
    // ── Create models ─────────────────────────────────────────────────────────
    m_logModel                = new LogModel(this);
    m_markLogModel            = new MarkLogModel(this);
    m_settingsModel           = new SettingsModel(this);
    m_propertiesModel         = new PropertiesModel(this);
    m_propertyDefinitionModel = new PropertyDefinitionModel(this);
    m_filterHistoryManager    = new FilterHistoryManager(this);
    m_propDefBackend          = new PropertyDefinitionBackend(this);

    // ── Setup UI sections (order matters: models before tables) ───────────────
    setupLogTable();
    setupConfigurationTables();
    setupSDKTab();
    setupTerminal();
    setupDumpsys();
    setupCradleTab();
    setupTooltips();
    setupFilterHistory();
    setupSplittersAndMisc();
    setupDevicesTab();

    // ── Wire up all signal/slot connections ───────────────────────────────────
    connectAdbManagerSignals();
    connectFilterSignals();
    connectButtonSignals();
    connectTableSignals();

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

    // ── Start socket listener for live settings updates from device ───────────
    setupSocketListener();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Setup — initialise visual components
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::setupLogTable()
{
    using namespace TableConfig::LogColumns;
    using namespace TableConfig::ColumnWidths;

    // ── Main log table ────────────────────────────────────────────────────────
    m_ui->tableLog->setModel(m_logModel);
    m_ui->tableLog->horizontalHeader()->setStretchLastSection(false);
    m_logModel->setMarkedRows(&m_markedRows);

    m_ui->tableLog->setColumnWidth(DATE,    LOG_DATE);
    m_ui->tableLog->setColumnWidth(TIME,    LOG_TIME);
    m_ui->tableLog->setColumnWidth(PID,     LOG_PID);
    m_ui->tableLog->setColumnWidth(TID,     LOG_TID);
    m_ui->tableLog->setColumnWidth(PACKAGE, LOG_PACKAGE);
    m_ui->tableLog->setColumnWidth(LEVEL,   LOG_LEVEL);
    m_ui->tableLog->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);

    // ── Mark log table ────────────────────────────────────────────────────────
    m_ui->tableMarkLog->setModel(m_markLogModel);
    m_ui->tableMarkLog->horizontalHeader()->setStretchLastSection(false);

    m_ui->tableMarkLog->setColumnWidth(DATE,    LOG_DATE);
    m_ui->tableMarkLog->setColumnWidth(TIME,    LOG_TIME);
    m_ui->tableMarkLog->setColumnWidth(PID,     LOG_PID);
    m_ui->tableMarkLog->setColumnWidth(TID,     LOG_TID);
    m_ui->tableMarkLog->setColumnWidth(PACKAGE, LOG_PACKAGE);
    m_ui->tableMarkLog->setColumnWidth(LEVEL,   LOG_LEVEL);
    m_ui->tableMarkLog->horizontalHeader()->setSectionResizeMode(DELTA,   QHeaderView::ResizeToContents);
    m_ui->tableMarkLog->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);

    // Hide low-value columns by default and sync dependent filter group boxes
    applyColumnVisibility({false, false, true, false, false, true, true, true});

    // ── Highlight delegates ───────────────────────────────────────────────────
    m_pidHighlightDelegate     = new HighlightDelegate(this);
    m_packageHighlightDelegate = new HighlightDelegate(this);
    m_tagHighlightDelegate     = new HighlightDelegate(this);
    m_messageHighlightDelegate = new HighlightDelegate(this);
    m_messageHighlightDelegate->setWordWrap(true);

    m_ui->tableLog->setItemDelegateForColumn(PID,     m_pidHighlightDelegate);
    m_ui->tableLog->setItemDelegateForColumn(PACKAGE, m_packageHighlightDelegate);
    m_ui->tableLog->setItemDelegateForColumn(TAG,     m_tagHighlightDelegate);
    m_ui->tableLog->setItemDelegateForColumn(MESSAGE, m_messageHighlightDelegate);

    // Install a plain HighlightDelegate (no keywords) as the view-level
    // delegate so that DATE, TIME, TID and LEVEL columns also honour
    // Qt::BackgroundRole for marked rows (per-column delegates take priority).
    m_ui->tableLog->setItemDelegate(new HighlightDelegate(this));

    // ── Row-resize debounce timer ─────────────────────────────────────────────
    m_rowResizeTimer = new QTimer(this);
    m_rowResizeTimer->setSingleShot(true);
    m_rowResizeTimer->setInterval(150);
    connect(m_rowResizeTimer, &QTimer::timeout, this, [this]() { resizeVisibleRows(); });

    connect(m_ui->tableLog->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int section, int, int) {
        if (section == TableConfig::LogColumns::TAG ||
            section == TableConfig::LogColumns::MESSAGE)
            m_rowResizeTimer->start();
    });
    connect(m_ui->tableLog->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) {
        if (m_logModel->rowCount() > ROW_RESIZE_THRESHOLD)
            m_rowResizeTimer->start();
    });

    // ── Multi-select + Ctrl+C copy ────────────────────────────────────────────
    enableTableCopyAction(m_ui->tableLog);
    enableTableCopyAction(m_ui->tableMarkLog);

    // ── Install event filters for Shift+Scroll on viewports ──────────────────
    m_ui->tableLog->viewport()->installEventFilter(m_mainWindow);
    m_ui->tableMarkLog->viewport()->installEventFilter(m_mainWindow);
}

void UiManager::setupConfigurationTables()
{
    using namespace TableConfig::SettingsColumns;
    using namespace TableConfig::ColumnWidths;

    // ── Settings table ────────────────────────────────────────────────────────
    m_ui->tableSettings->setModel(m_settingsModel);
    m_ui->tableSettings->horizontalHeader()->setStretchLastSection(false);
    m_ui->tableSettings->setColumnWidth(LINE,    SETTINGS_LINE);
    m_ui->tableSettings->setColumnWidth(GROUP,   SETTINGS_GROUP);
    m_ui->tableSettings->setColumnWidth(SETTING, SETTINGS_SETTING);
    m_ui->tableSettings->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    m_ui->tableSettings->setColumnWidth(ACTION,  SETTINGS_ACTION);

    ValueDelegate *settingsDelegate = new ValueDelegate(this);
    m_ui->tableSettings->setItemDelegateForColumn(VALUE, settingsDelegate);
    m_ui->tableSettings->setWordWrap(true);
    m_ui->tableSettings->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->tableSettings->setSelectionBehavior(QAbstractItemView::SelectItems);

    // ── Properties table ──────────────────────────────────────────────────────
    m_ui->tableProperties->setModel(m_propertiesModel);
    m_ui->tableProperties->horizontalHeader()->setStretchLastSection(false);
    m_ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::LINE,     PROPERTIES_LINE);
    m_ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::PROPERTY, PROPERTIES_PROPERTY);
    m_ui->tableProperties->horizontalHeader()->setSectionResizeMode(
        TableConfig::PropertiesColumns::VALUE, QHeaderView::Stretch);
    m_ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::ACTION, PROPERTIES_ACTION);

    ValueDelegate *propertiesDelegate = new ValueDelegate(this);
    m_ui->tableProperties->setItemDelegateForColumn(
        TableConfig::PropertiesColumns::VALUE, propertiesDelegate);
    m_ui->tableProperties->setWordWrap(true);
    m_ui->tableProperties->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->tableProperties->setSelectionBehavior(QAbstractItemView::SelectItems);

    // ── Config splitters ──────────────────────────────────────────────────────
    m_ui->splitterConfig->setSizes(QList<int>() << 500 << 700);
    m_ui->splitterConfigTables->setSizes(QList<int>() << 300 << 300);
}

void UiManager::setupSDKTab()
{
    using namespace TableConfig::PropertyDefColumns;
    using namespace TableConfig::ColumnWidths;

    m_ui->tablePropertyDefinitions->setModel(m_propertyDefinitionModel);
    m_ui->tablePropertyDefinitions->horizontalHeader()->setStretchLastSection(false);

    m_ui->tablePropertyDefinitions->setColumnWidth(NAME,          PROPDEF_NAME);
    m_ui->tablePropertyDefinitions->setColumnWidth(ID,            PROPDEF_ID);
    m_ui->tablePropertyDefinitions->setColumnWidth(SUPPORTED,     PROPDEF_SUPPORTED);
    m_ui->tablePropertyDefinitions->setColumnWidth(VALUE,         PROPDEF_DEFAULT);
    m_ui->tablePropertyDefinitions->setColumnWidth(NEED_REBOOT,   PROPDEF_NEED_REBOOT);
    m_ui->tablePropertyDefinitions->setColumnWidth(TYPE,          PROPDEF_TYPE);
    m_ui->tablePropertyDefinitions->setColumnWidth(READ_ONLY,     PROPDEF_READ_ONLY);
    m_ui->tablePropertyDefinitions->setColumnWidth(SET_BUTTON,    PROPDEF_SET_BUTTON);
    m_ui->tablePropertyDefinitions->setColumnWidth(GET_BUTTON,    PROPDEF_GET_BUTTON);
    m_ui->tablePropertyDefinitions->setColumnWidth(REMOVE_BUTTON, PROPDEF_REMOVE_BUTTON);

    m_ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(NAME,  QHeaderView::Stretch);
    m_ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    m_ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(ID,    QHeaderView::Fixed);
    m_ui->tablePropertyDefinitions->setWordWrap(true);
    m_ui->tablePropertyDefinitions->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->tablePropertyDefinitions->setSelectionBehavior(QAbstractItemView::SelectItems);

    // Default column visibility: show ID, Name, Type, Read Only, Value, Set, Get, Remove
    applyPropDefColumnVisibility({
        true,   // 0  ID
        true,   // 1  Name
        false,  // 2  Supported
        false,  // 3  Need Reboot
        true,   // 4  Type
        true,   // 5  Read Only
        false,  // 6  Default
        true,   // 7  Value
        true,   // 8  Set
        true,   // 9  Get
        true,   // 10 Remove
    });

    ValueDelegate *valueDelegate = new ValueDelegate(this);
    m_ui->tablePropertyDefinitions->setItemDelegateForColumn(VALUE, valueDelegate);

    // Auto-trigger set when user edits the Value cell inline, but only for non-empty values.
    // An empty value means the user cleared the cell; they must press the Set button explicitly.
    // Skip when the change originates from a socket update to avoid sending the value back to device.
    connect(m_propertyDefinitionModel, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex &topLeft, const QModelIndex &, const QList<int> &) {
        if (m_propertyDefinitionModel->isUpdatingFromSocket())
            return;
        if (topLeft.column() != TableConfig::PropertyDefColumns::VALUE)
            return;
        const QString newValue = m_propertyDefinitionModel->data(topLeft, Qt::DisplayRole).toString();
        if (!newValue.isEmpty())
            onSetPropertyDefinitionClicked(topLeft.row());
    });

    // ── Property search completer ─────────────────────────────────────────────
    QCompleter *completer = new QCompleter(new QStringListModel(this), this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->popup()->setStyleSheet(
        "QListView {"
        "    background-color: #2d2d30;"
        "    color: #cccccc;"
        "    border: 1px solid #3e3e42;"
        "    selection-background-color: #0e639c;"
        "    selection-color: #ffffff;"
        "}");
    m_ui->txtPropertySearch->setCompleter(completer);

    // Mouse click on a suggestion: activated fires but returnPressed does NOT.
    connect(completer->popup(), &QAbstractItemView::clicked,
            this, [this, completer](const QModelIndex &index) {
        const QString text = completer->completionModel()->data(index).toString();
        if (!text.isEmpty())
            m_ui->txtPropertySearch->setText(text);
        onAddPropertyDefinition();
    });

    // Show all properties when the search field is cleared
    connect(m_ui->txtPropertySearch, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty()) {
            QCompleter *c = m_ui->txtPropertySearch->completer();
            if (c && c->model() && c->model()->rowCount() > 0) {
                c->setCompletionPrefix("");
                c->complete();
            }
        }
    });

    // Allow the completer focus event filter on this widget
    m_ui->txtPropertySearch->installEventFilter(m_mainWindow);
}

void UiManager::setupTerminal()
{
    m_terminal = new QTermWidget(1, m_ui->terminalContainer);
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_terminal->setTerminalFont(QFont(QStringLiteral("Monospace"), 10));
    m_terminal->setColorScheme(QStringLiteral("Linux"));
    m_terminal->setTerminalOpacity(1.0);

    // Override the dark-theme stylesheet on the container so the terminal's
    // own color scheme takes effect (the global stylesheet would stomp it otherwise).
    m_ui->terminalContainer->setStyleSheet(
        QStringLiteral("QWidget { background-color: black; color: #b2b2b2; }"));

    // Install event filter on the inner TerminalDisplay so we can intercept
    // Ctrl+C / Ctrl+V before TerminalDisplay::keyPressEvent() consumes them.
    for (QWidget *child : m_terminal->findChildren<QWidget*>()) {
        if (child->metaObject()->className() == QStringLiteral("Konsole::TerminalDisplay")) {
            child->installEventFilter(m_mainWindow);
            break;
        }
    }

    m_ui->terminalLayout->addWidget(m_terminal);
    m_ui->terminalContainer->hide();

    // Start with terminal collapsed (tabWidget gets all space)
    m_ui->splitterMain->setSizes(QList<int>() << 800 << 0);

    connect(m_terminal, &QTermWidget::finished, this, [this]() {
        m_ui->btnToggleTerminal->setChecked(false);
    });
}

void UiManager::setupDumpsys()
{
    // ── Completer for service search ──────────────────────────────────────────
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
    m_ui->txtDumpsysService->setCompleter(completer);

    // Selecting a suggestion auto-runs dumpsys for that service
    connect(completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &text) {
        m_ui->txtDumpsysService->setText(text);
        onRunDumpsysClicked();
    });

    // ── Dumpsys controls ──────────────────────────────────────────────────────
    connect(m_ui->txtDumpsysService,  &QLineEdit::returnPressed,    this, &UiManager::onRunDumpsysClicked);
    connect(m_ui->txtDumpsysService,  &QLineEdit::textChanged,       this, &UiManager::updateDumpsysCommandText);
    connect(m_ui->btnDumpsysRefresh,  &QPushButton::clicked,        this, &UiManager::onRunDumpsysClicked);
    connect(m_ui->txtDumpsysCommand,   &QLineEdit::returnPressed,    this, &UiManager::onRunDumpsysCmdClicked);
    connect(m_ui->txtPackageFilter,   &QLineEdit::textChanged,       this, &UiManager::updateDumpsysCommandText);
    connect(m_ui->txtDumpsysSearch,   &QLineEdit::textChanged,      this, &UiManager::onDumpsysSearchChanged);
    connect(m_ui->txtDumpsysSearch,   &QLineEdit::returnPressed,    this, &UiManager::onDumpsysSearchNext);
    connect(m_ui->btnDumpsysSearchPrev, &QPushButton::clicked,      this, &UiManager::onDumpsysSearchPrev);
    connect(m_ui->btnDumpsysSearchNext, &QPushButton::clicked,      this, &UiManager::onDumpsysSearchNext);

    connect(&AdbManager::instance(), &AdbManager::dumpsysListFetched, this, &UiManager::onDumpsysListFetched);
    connect(&AdbManager::instance(), &AdbManager::dumpsysFetched,        this, &UiManager::onDumpsysFetched);
    connect(&AdbManager::instance(), &AdbManager::rawAdbCommandFinished, this, &UiManager::onRawAdbCommandFinished);

    // Initial splitter ratio: txtDumpsysCmdResult 75%, txtDumpsysResult 25%
    m_ui->splitterDumpsysOutput->setSizes({750, 250});

    // Auto-fetch relevant data when switching to Configuration or SDK tabs
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        QWidget *tab = m_ui->tabWidget->widget(index);
        if (tab == m_ui->tabConfiguration && !m_currentDeviceId.isEmpty()) {
            AdbManager::instance().fetchDumpsysList(m_currentDeviceId);
            AdbManager::instance().fetchSettings(m_currentDeviceId);
            AdbManager::instance().fetchProperties(m_currentDeviceId);
        } else if (tab == m_ui->tabSDK && !m_currentDeviceId.isEmpty()) {
            AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
        }
    });
}

void UiManager::setupCradleTab()
{
    connect(m_ui->radioCustomFirmware, &QRadioButton::toggled, this, [this](bool checked) {
        m_ui->txtCradleFwPath->setEnabled(checked);
    });

    connect(m_ui->btnCradleGet,            &QPushButton::clicked,    this, &UiManager::onCradleGetInfo);
    connect(m_ui->txtCradleKey,            &QLineEdit::returnPressed, this, &UiManager::onCradleGetInfo);
    connect(m_ui->btnCradleQueryFirmware,  &QPushButton::clicked,    this, &UiManager::onCradleQueryFirmware);
    connect(m_ui->btnCradleUpdateFirmware, &QPushButton::clicked,    this, &UiManager::onCradleUpdateFirmware);
    connect(m_ui->btnCradleQuerySchedule,  &QPushButton::clicked,    this, &UiManager::onCradleQuerySchedule);
    connect(m_ui->btnCradleClearOutput,    &QPushButton::clicked,
            this, [this]() { m_ui->txtCradleOutput->clear(); m_ui->lblCradleLastCmd->setText("—"); });

    connect(&AdbManager::instance(), &AdbManager::cradleCommandFinished,
            this, &UiManager::onCradleCommandFinished);
}

void UiManager::setupSocketListener()
{
    constexpr quint16 kHostPort = 5555;

    m_socketServer                    = new SocketServer(kHostPort, this);
    m_settingsSocketHandler           = new SettingsSocketHandler(this);
    m_systemPropertySocketHandler     = new SystemPropertySocketHandler(this);
    m_propertyDefinitionSocketHandler = new PropertyDefinitionSocketHandler(this);

    m_socketServer->registerHandler(m_settingsSocketHandler);
    m_socketServer->registerHandler(m_systemPropertySocketHandler);
    m_socketServer->registerHandler(m_propertyDefinitionSocketHandler);

    connect(m_settingsSocketHandler, &SettingsSocketHandler::settingsReceived,
            this, [this](const QVector<SettingEntry> &settings) {
        m_settingsModel->updateSettings(settings, false);
    });

    connect(m_systemPropertySocketHandler, &SystemPropertySocketHandler::propertiesReceived,
            this, [this](const QVector<PropertyEntry> &properties) {
        m_propertiesModel->updateProperties(properties, false);
    });

    connect(m_propertyDefinitionSocketHandler, &PropertyDefinitionSocketHandler::propertyDefinitionReceived,
            this, [this](const QString &id, const QString &value) {
        PropertyDefinition entry;
        entry.id    = id;
        entry.value = value;
        m_propertyDefinitionModel->updatePropertyDefinitions({entry}, false);
    });

    if (!m_socketServer->start()) {
        qDebug() << "UiManager: SocketServer retry scheduled for port" << kHostPort;
        return;
    }
    qDebug() << "UiManager: SocketServer started on port" << kHostPort;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Devices Tab
// ─────────────────────────────────────────────────────────────────────────────

// Helper: build a DeviceInfo from an AdbDevice + its group assignment.
static UiManager::DeviceInfo makeDeviceInfo(const AdbDevice &d, const QString &group)
{
    UiManager::DeviceInfo info;
    info.serial = d.id;
    info.name   = d.name;
    info.group  = group;
    info.online = d.isOnline;
    return info;
}

void UiManager::setupDevicesTab()
{
    // Connect DevicesManager -> refresh the sidebar whenever anything changes.
    connect(&DevicesManager::instance(), &DevicesManager::devicesOrGroupsChanged,
            this, &UiManager::onDevicesOrGroupsChanged, Qt::UniqueConnection);

    // Connect DevicesManager -> update dashboard when device details arrive.
    connect(&DevicesManager::instance(), &DevicesManager::deviceDetailsFetched,
            this, &UiManager::onDeviceDetailsFetched, Qt::UniqueConnection);

    // Hide the "New Group" button (no longer used).
    m_ui->devBtnAddGroup->hide();

    // ── Quick-action button connections ──────────────────────────────────────
    // Returns all target serials: either the single selected device,
    // or all online devices in the selected group.
    auto selectedSerials = [this]() -> QStringList {
        // Return all checked devices that are currently online
        if (!m_checkedDevices.isEmpty()) {
            QStringList serials;
            DevicesManager &dm = DevicesManager::instance();
            const QList<AdbDevice> connected = dm.connectedDevices();
            QSet<QString> onlineIds;
            for (const AdbDevice &d : connected)
                if (d.isOnline) onlineIds.insert(d.id);
            for (const QString &id : m_checkedDevices)
                if (onlineIds.contains(id)) serials.append(id);
            return serials;
        }
        // Fallback: single selected device
        if (!m_selectedDeviceRow) return {};
        const QString s = m_deviceRowMap.value(m_selectedDeviceRow).serial;
        return s.isEmpty() ? QStringList{} : QStringList{s};
    };

    auto selectedSerial = [this]() -> QString {
        if (!m_selectedDeviceRow) return {};
        return m_deviceRowMap.value(m_selectedDeviceRow).serial;
    };

    // Reboot device
    connect(m_ui->devBtnReboot, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::rebootDevice(s));
    });

    // Reboot sideload
    connect(m_ui->devBtnForceStop, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::rebootSideload(s));
    });

    // Reboot bootloader
    connect(m_ui->devBtnClearCache, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::rebootBootloader(s));
    });

    // Volume up
    connect(m_ui->devBtnSyslog, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::volumeUp(s));
    });

    // Volume down
    connect(m_ui->devBtnUnlock, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::volumeDown(s));
    });

    // Connect WiFi (uses static input fields in devInfoGrid)
    {
        auto &mgr = DevicesManager::instance();
        m_ui->devWifiSsidEdit->setText(mgr.savedWifiSsid());
        m_ui->devWifiPassEdit->setText(mgr.savedWifiPassword());
    }
    connect(m_ui->devBtnConnectWifi, &QPushButton::clicked, this, [this, selectedSerials]() {
        const QStringList serials = selectedSerials();
        if (serials.isEmpty()) return;

        const QString ssid = m_ui->devWifiSsidEdit->text().trimmed();
        const QString pass = m_ui->devWifiPassEdit->text();
        if (ssid.isEmpty()) return;

        auto &mgr = DevicesManager::instance();
        mgr.saveWifiCredentials(ssid, pass);
        for (const QString &serial : serials)
            mgr.runAdbCommand(AdbCommand::connectWifi(serial, ssid, pass));
    });

    // Browse firmware path
    connect(m_ui->devBtnBrowseFirmware, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            m_mainWindow, tr("Select Firmware Directory"),
            m_ui->devFirmwarePathEdit->text().isEmpty()
                ? QDir::homePath()
                : m_ui->devFirmwarePathEdit->text());
        if (!dir.isEmpty())
            m_ui->devFirmwarePathEdit->setText(dir);
    });

    // All action buttons start disabled (enabled when devices are checked)
    for (QPushButton *btn : {m_ui->devBtnReboot,
                              m_ui->devBtnSyslog, m_ui->devBtnClearCache,
                              m_ui->devBtnUnlock,
                              m_ui->devBtnForceStop, m_ui->devBtnAdbWireless,
                              m_ui->devBtnAdbRoot, m_ui->devBtnAdbUnroot,
                              m_ui->devBtnRebootFastboot,
                              m_ui->devBtnPowerKey,
                              m_ui->devBtnConnectWifi,
                              m_ui->devBtnFlash,
                              m_ui->devBtnDeployConfig})
        btn->setEnabled(!m_checkedDevices.isEmpty());

    // Flash firmware: reboot bootloader + run download.sh with progress dialog
    connect(m_ui->devBtnFlash, &QPushButton::clicked, this, [this, selectedSerials]() {
        const QStringList serials = selectedSerials();
        if (serials.isEmpty()) {
            QMessageBox::warning(m_mainWindow, tr("No Device"),
                                 tr("Please select a device first."));
            return;
        }

        const QString firmwarePath = m_ui->devFirmwarePathEdit->text().trimmed();
        if (firmwarePath.isEmpty()) {
            QMessageBox::warning(m_mainWindow, tr("No Firmware Path"),
                                 tr("Please select a firmware directory first."));
            return;
        }

        const QFileInfo scriptInfo(firmwarePath + "/download.sh");
        if (!scriptInfo.exists()) {
            QMessageBox::warning(m_mainWindow, tr("Script Not Found"),
                                 tr("download.sh not found in the firmware directory."));
            return;
        }

        // Create progress dialog
        QDialog *dlg = new QDialog(m_mainWindow);
        dlg->setWindowTitle(tr("Flash Firmware"));
        dlg->resize(700, 500);
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        QVBoxLayout *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        QLabel *statusLabel = new QLabel(tr("Flashing %1 device(s)...").arg(serials.size()));
        statusLabel->setStyleSheet("color: #cccccc; font-weight: bold; font-size: 13px;");
        layout->addWidget(statusLabel);

        QTextEdit *outputView = new QTextEdit();
        outputView->setReadOnly(true);
        outputView->setStyleSheet(
            "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3e3e42;"
            " border-radius: 4px; font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; }");
        layout->addWidget(outputView, 1);

        QPushButton *closeBtn = new QPushButton(tr("Close"));
        closeBtn->setEnabled(false);
        closeBtn->setStyleSheet(
            "QPushButton { background-color: #37373d; color: #cccccc; border: 1px solid #3e3e42;"
            " border-radius: 4px; padding: 8px 24px; font-weight: bold; }"
            "QPushButton:hover { background-color: #3e3e50; border-color: #007acc; }"
            "QPushButton:disabled { color: #5a5a5a; }");
        layout->addWidget(closeBtn, 0, Qt::AlignRight);

        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

        dlg->show();

        // Track completion of all device processes
        auto remaining = new int(serials.size());
        auto hasErrors = new bool(false);

        // Launch a separate process per device in parallel
        for (const QString &serial : serials) {
            const QString cmd = QString("export ANDROID_SERIAL=%1 ; adb -s %1 reboot bootloader && ./download.sh").arg(serial);
            outputView->append(QStringLiteral("[%1] $ %2\n").arg(serial, cmd));

            QProcess *proc = new QProcess(dlg);
            proc->setWorkingDirectory(firmwarePath);
            proc->setProcessChannelMode(QProcess::MergedChannels);

            connect(proc, &QProcess::readyRead, dlg, [proc, outputView, serial]() {
                const QString text = QString::fromUtf8(proc->readAll());
                const QStringList lines = text.split('\n');
                for (const QString &line : lines) {
                    if (!line.isEmpty()) {
                        outputView->moveCursor(QTextCursor::End);
                        outputView->insertPlainText(QStringLiteral("[%1] %2\n").arg(serial, line));
                    }
                }
                outputView->moveCursor(QTextCursor::End);
            });

            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    dlg, [serial, statusLabel, outputView, closeBtn, remaining, hasErrors]
                    (int exitCode, QProcess::ExitStatus status) {
                const bool ok = (status == QProcess::NormalExit && exitCode == 0);
                outputView->append(QStringLiteral("[%1] --- Finished (exit code: %2) ---\n")
                                       .arg(serial).arg(exitCode));
                if (!ok)
                    *hasErrors = true;

                --(*remaining);
                if (*remaining <= 0) {
                    const bool allOk = !(*hasErrors);
                    statusLabel->setText(allOk ? QStringLiteral("All devices flashed successfully.")
                                               : QStringLiteral("Flashing completed with errors on some devices."));
                    statusLabel->setStyleSheet(
                        allOk ? "color: #34d399; font-weight: bold; font-size: 13px;"
                              : "color: #f44336; font-weight: bold; font-size: 13px;");
                    closeBtn->setEnabled(true);
                    delete remaining;
                    delete hasErrors;
                }
            });

            connect(proc, &QProcess::errorOccurred, dlg,
                    [serial, statusLabel, outputView, closeBtn, remaining, hasErrors](QProcess::ProcessError error) {
                Q_UNUSED(error);
                outputView->append(QStringLiteral("[%1] --- Failed to start command ---\n").arg(serial));
                *hasErrors = true;

                --(*remaining);
                if (*remaining <= 0) {
                    statusLabel->setText(QStringLiteral("Flashing completed with errors on some devices."));
                    statusLabel->setStyleSheet("color: #f44336; font-weight: bold; font-size: 13px;");
                    closeBtn->setEnabled(true);
                    delete remaining;
                    delete hasErrors;
                }
            });

            proc->start("/bin/bash", QStringList() << "-c" << cmd);
        }
    });

    // ADB Wireless connect
    connect(m_ui->devBtnAdbWireless, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().enableAdbWireless(s);
    });

    // ADB root
    connect(m_ui->devBtnAdbRoot, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::root(s));
    });

    // ADB unroot
    connect(m_ui->devBtnAdbUnroot, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::unroot(s));
    });

    // Reboot fastboot
    connect(m_ui->devBtnRebootFastboot, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::rebootFastboot(s));
    });

    // Power key input
    connect(m_ui->devBtnPowerKey, &QPushButton::clicked, this, [=]() {
        for (const QString &s : selectedSerials())
            DevicesManager::instance().runAdbCommand(AdbCommand::powerKey(s));
    });

    // ── Configuration tab: toggle switches are defined in mainwindow.ui ─────
    // devStayAwakeToggle and devAllowMockToggle are ToggleSwitch widgets.

    // Helper: build configuration JSON from current toggle states
    auto buildConfigJson = [this]() -> QJsonObject {
        QJsonObject root;
        QJsonObject deviceSettings;
        deviceSettings["stay_awake"] = m_ui->devStayAwakeToggle->isChecked();
        deviceSettings["allow_mock_modem"] = m_ui->devAllowMockToggle->isChecked();
        deviceSettings["verifier_verify_adb_installs"] = m_ui->devVerifyAdbToggle->isChecked();
        deviceSettings["system_locale"] = m_ui->devLocaleEdit->text().trimmed();
        deviceSettings["time_12_24"] = m_ui->devTimeFormatToggle->isChecked(); // true = 24h
        root["device_settings"] = deviceSettings;
        QJsonObject displaySettings;
        root["display"] = displaySettings;
        return root;
    };

    // Helper: apply configuration JSON to toggle switches
    auto applyConfigJson = [this](const QJsonObject &root) {
        if (root.contains("device_settings")) {
            QJsonObject ds = root["device_settings"].toObject();
            if (ds.contains("stay_awake"))
                m_ui->devStayAwakeToggle->setChecked(ds["stay_awake"].toBool());
            if (ds.contains("allow_mock_modem"))
                m_ui->devAllowMockToggle->setChecked(ds["allow_mock_modem"].toBool());
            // Backward compat: old presets may have allow_mock_locations
            if (ds.contains("allow_mock_locations") && !ds.contains("allow_mock_modem"))
                m_ui->devAllowMockToggle->setChecked(ds["allow_mock_locations"].toBool());
            if (ds.contains("verifier_verify_adb_installs"))
                m_ui->devVerifyAdbToggle->setChecked(ds["verifier_verify_adb_installs"].toBool());
            if (ds.contains("system_locale"))
                m_ui->devLocaleEdit->setText(ds["system_locale"].toString());
            if (ds.contains("time_12_24"))
                m_ui->devTimeFormatToggle->setChecked(ds["time_12_24"].toBool());
        }
    };

    // ── Auto-update JSON view when config changes ────────────────────────
    auto updateJsonView = [this, buildConfigJson]() {
        QJsonDocument doc(buildConfigJson());
        m_ui->devJsonView->setPlainText(
            QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    };

    // Connect all toggles
    connect(m_ui->devStayAwakeToggle, &ToggleSwitch::toggled, this, updateJsonView);
    connect(m_ui->devAllowMockToggle, &ToggleSwitch::toggled, this, updateJsonView);
    connect(m_ui->devVerifyAdbToggle, &ToggleSwitch::toggled, this, updateJsonView);
    connect(m_ui->devTimeFormatToggle, &ToggleSwitch::toggled, this, updateJsonView);

    // Connect locale input on Enter and on text change
    connect(m_ui->devLocaleEdit, &QLineEdit::returnPressed, this, updateJsonView);
    connect(m_ui->devLocaleEdit, &QLineEdit::textChanged, this, updateJsonView);

    // Generate initial JSON view
    updateJsonView();

    // ── Export JSON button ──────────────────────────────────────────────────
    connect(m_ui->devBtnExportJson, &QPushButton::clicked, this, [this, buildConfigJson]() {
        const QString filePath = QFileDialog::getSaveFileName(
            m_mainWindow, tr("Export Device Configuration"),
            QDir::homePath() + "/device_config.json",
            tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;

        QJsonDocument doc(buildConfigJson());
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(m_mainWindow, tr("Export Failed"),
                                 tr("Could not write to file:\n%1").arg(filePath));
            return;
        }
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        QMessageBox::information(m_mainWindow, tr("Export Successful"),
                                 tr("Configuration exported to:\n%1").arg(filePath));
    });

    // ── Import JSON button ──────────────────────────────────────────────────
    connect(m_ui->devBtnImportJson, &QPushButton::clicked, this, [this, applyConfigJson]() {
        const QString filePath = QFileDialog::getOpenFileName(
            m_mainWindow, tr("Import Device Configuration"),
            QDir::homePath(),
            tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(m_mainWindow, tr("Import Failed"),
                                 tr("Could not open file:\n%1").arg(filePath));
            return;
        }
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (doc.isNull()) {
            QMessageBox::warning(m_mainWindow, tr("Import Failed"),
                                 tr("Invalid JSON:\n%1").arg(parseError.errorString()));
            return;
        }

        applyConfigJson(doc.object());

        // Update the JSON preview
        m_ui->devJsonView->setPlainText(
            QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

        QMessageBox::information(m_mainWindow, tr("Import Successful"),
                                 tr("Configuration loaded from:\n%1").arg(filePath));
    });

    // ── Deploy to Device button ─────────────────────────────────────────────
    connect(m_ui->devBtnDeployConfig, &QPushButton::clicked, this, [this, selectedSerials, buildConfigJson]() {
        const QStringList serials = selectedSerials();
        if (serials.isEmpty()) {
            QMessageBox::warning(m_mainWindow, tr("No Device"),
                                 tr("Please select a device or group first."));
            return;
        }

        // Read config from JSON view if available, otherwise from toggles
        QJsonObject config;
        const QString jsonText = m_ui->devJsonView->toPlainText().trimmed();
        if (!jsonText.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8());
            if (!doc.isNull() && doc.isObject())
                config = doc.object();
        }
        if (config.isEmpty())
            config = buildConfigJson();

        QJsonObject ds = config["device_settings"].toObject();

        int deployed = 0;
        auto &mgr = DevicesManager::instance();
        for (const QString &serial : serials) {
            if (ds.contains("stay_awake"))
                mgr.runAdbCommand(AdbCommand::stayAwake(serial, ds["stay_awake"].toBool()));
            if (ds.contains("allow_mock_modem"))
                mgr.runAdbCommand(AdbCommand::setMockModem(serial, ds["allow_mock_modem"].toBool()));
            if (ds.contains("verifier_verify_adb_installs"))
                mgr.runAdbCommand(AdbCommand::setVerifyAdbInstalls(serial, ds["verifier_verify_adb_installs"].toBool()));
            if (ds.contains("system_locale") && !ds["system_locale"].toString().isEmpty())
                mgr.runAdbCommand(AdbCommand::setSystemLocale(serial, ds["system_locale"].toString()));
            if (ds.contains("time_12_24"))
                mgr.runAdbCommand(AdbCommand::setTimeFormat(serial, ds["time_12_24"].toBool()));
            ++deployed;
        }

        QMessageBox::information(m_mainWindow, tr("Deploy Successful"),
                                 tr("Configuration deployed to %1 device(s).").arg(deployed));
    });

    // ── Preset management ───────────────────────────────────────────────────

    // Save preset — shows input dialog to type a name
    connect(m_ui->devBtnSavePreset, &QPushButton::clicked, this,
            [this, buildConfigJson]() {
        bool ok = false;
        const QString name = QInputDialog::getText(
            m_mainWindow,
            tr("Save Configuration Preset"),
            tr("Enter a name for this preset:"),
            QLineEdit::Normal,
            QString(),
            &ok).trimmed();
        if (!ok || name.isEmpty()) return;

        QJsonDocument doc(buildConfigJson());
        DevicesManager::instance().saveConfigPreset(name, doc.toJson(QJsonDocument::Compact));
        QMessageBox::information(m_mainWindow, tr("Save Preset"),
                                 tr("Preset \"%1\" saved successfully.").arg(name));
    });

    // Load preset — shows a dialog with a list + delete option
    connect(m_ui->devBtnLoadPreset, &QPushButton::clicked, this,
            [this, applyConfigJson]() {
        const QStringList names = DevicesManager::instance().listConfigPresets();
        if (names.isEmpty()) {
            QMessageBox::information(m_mainWindow, tr("Load Preset"),
                                     tr("No saved presets found."));
            return;
        }

        QDialog dialog(m_mainWindow);
        dialog.setWindowTitle(tr("Load Configuration Preset"));
        dialog.setMinimumSize(360, 300);

        auto *layout     = new QVBoxLayout(&dialog);
        auto *label      = new QLabel(tr("Select a preset to load:"), &dialog);
        auto *listWidget = new QListWidget(&dialog);
        listWidget->addItems(names);
        listWidget->setCurrentRow(0);

        auto *btnBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

        auto *btnDelete = new QPushButton(tr("Delete"), &dialog);
        btnBox->addButton(btnDelete, QDialogButtonBox::ResetRole);

        layout->addWidget(label);
        layout->addWidget(listWidget);
        layout->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(btnDelete, &QPushButton::clicked, &dialog, [&]() {
            QListWidgetItem *item = listWidget->currentItem();
            if (!item) return;
            const QString n = item->text();
            const auto reply = QMessageBox::question(
                &dialog, tr("Delete Preset"),
                tr("Delete \"%1\"?").arg(n),
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) return;
            DevicesManager::instance().deleteConfigPreset(n);
            delete listWidget->takeItem(listWidget->row(item));
        });
        connect(listWidget, &QListWidget::doubleClicked, &dialog, &QDialog::accept);

        if (dialog.exec() != QDialog::Accepted) return;

        QListWidgetItem *selected = listWidget->currentItem();
        if (!selected) return;

        const QByteArray data = DevicesManager::instance().loadConfigPreset(selected->text());
        if (data.isEmpty()) return;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) return;

        applyConfigJson(doc.object());
        m_ui->devJsonView->setPlainText(
            QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    });

    refreshDevicesTab();
}

void UiManager::refreshDevicesTab()
{
    const QString colText   = "#cccccc";
    const QString colMuted  = "#8a8a8a";
    const QString colAccent = "#007acc";
    const QString colGreen  = "#34d399";
    const QString colBorder = "#3e3e42";

    // ── Device row factory (with checkbox) ────────────────────────────────────
    auto makeDeviceRow = [&](const DeviceInfo &info, bool selected, bool checked) -> QWidget* {
        const QString statusColor = info.online ? colGreen : colMuted;
        const QString dotChar     = info.online ? QStringLiteral("●") : QStringLiteral("○");

        QWidget *w = new QWidget();
        w->setObjectName(QStringLiteral("devRow_") + info.serial);
        const QString rowStyle = selected
            ? QString("QWidget#devRow_%1 { background-color: #4a4a52; border-left: 2px solid %2; }")
                  .arg(info.serial, colAccent)
            : QString("QWidget#devRow_%1 { background-color: transparent; border-left: 2px solid transparent; }")
                  .arg(info.serial);
        w->setStyleSheet(rowStyle);
        w->setCursor(Qt::PointingHandCursor);

        QHBoxLayout *hl = new QHBoxLayout(w);
        hl->setContentsMargins(selected ? 12 : 14, 8, 12, 8);
        hl->setSpacing(10);

        // Checkbox
        QCheckBox *cb = new QCheckBox();
        cb->setObjectName(QStringLiteral("devCheck_") + info.serial);
        cb->setChecked(checked);
        cb->setFixedSize(24, 24);
        cb->setStyleSheet(
            "QCheckBox { spacing: 0px; }"
            "QCheckBox::indicator {"
            "  width: 18px; height: 18px;"
            "  border: 2px solid #5a5a5e;"
            "  border-radius: 3px;"
            "  background: transparent;"
            "}"
            "QCheckBox::indicator:checked {"
            "  background-color: #007acc;"
            "  border-color: #007acc;"
            "}"
            "QCheckBox::indicator:hover {"
            "  border-color: #007acc;"
            "}"
        );
        connect(cb, &QCheckBox::toggled, this, [this, serial = info.serial](bool on) {
            if (on)
                m_checkedDevices.insert(serial);
            else
                m_checkedDevices.remove(serial);
            refreshCheckedDevicesList();
        });
        hl->addWidget(cb);

        QLabel *dot = new QLabel(dotChar);
        dot->setStyleSheet(QString("color: %1;").arg(statusColor));
        dot->setFixedWidth(12);

        QWidget *nameW = new QWidget();
        nameW->setStyleSheet("background: transparent;");
        QVBoxLayout *vl = new QVBoxLayout(nameW);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(1);

        QLabel *nameLbl = new QLabel(info.name);
        nameLbl->setStyleSheet(
            QString("color: %1; font-weight: %2;")
            .arg(colText, selected ? "bold" : "normal"));

        QLabel *serialLbl = new QLabel(info.serial);
        serialLbl->setStyleSheet(
            QString("color: %1;").arg(colMuted));

        vl->addWidget(nameLbl);
        vl->addWidget(serialLbl);

        hl->addWidget(dot);
        hl->addWidget(nameW, 1);

        // Register row for click handling
        m_deviceRowMap[w] = info;
        w->installEventFilter(m_mainWindow);
        return w;
    };

    // ── Clear current list ────────────────────────────────────────────────────
    QVBoxLayout *devListVLayout = m_ui->devListVLayout;
    while (devListVLayout->count() > 0) {
        QLayoutItem *item = devListVLayout->takeAt(0);
        if (QWidget *cw = item->widget()) cw->deleteLater();
        delete item;
    }

    // Remember previously selected device serial so we can restore it.
    const QString prevSerial = m_selectedDeviceRow
        ? m_deviceRowMap.value(m_selectedDeviceRow).serial
        : QString();
    m_deviceRowMap.clear();
    m_selectedDeviceRow = nullptr;

    // ── Populate device list ──────────────────────────────────────────────────
    DevicesManager &dm = DevicesManager::instance();
    const QList<AdbDevice> connected = dm.connectedDevices();

    QWidget *firstRow     = nullptr;
    QWidget *restoredRow  = nullptr;

    if (connected.isEmpty()) {
        QLabel *placeholder = new QLabel(tr("No devices connected.\nConnect a device via USB or WiFi."));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(QString("color: %1;").arg(colMuted));
        devListVLayout->addWidget(placeholder);
        devListVLayout->addStretch();
        updateDeviceDetails({});
        return;
    }

    // Remove checked serials that are no longer connected
    QSet<QString> connectedIds;
    for (const AdbDevice &d : connected)
        connectedIds.insert(d.id);
    m_checkedDevices.intersect(connectedIds);

    for (const AdbDevice &d : connected) {
        const DeviceInfo info = makeDeviceInfo(d, QString());
        const bool selected = (!prevSerial.isEmpty() && d.id == prevSerial);
        const bool checked  = m_checkedDevices.contains(d.id);
        QWidget *row = makeDeviceRow(info, selected, checked);
        devListVLayout->addWidget(row);
        if (!firstRow)   firstRow   = row;
        if (!prevSerial.isEmpty() && d.id == prevSerial)
            restoredRow = row;
    }

    devListVLayout->addStretch();

    // ── Restore / initial selection ───────────────────────────────────────────
    if (restoredRow) {
        m_selectedDeviceRow = restoredRow;
        updateDeviceDetails(m_deviceRowMap[restoredRow]);
    } else {
        // No previously selected device — show default empty state
        updateDeviceDetails({});
    }

    // ── Update the checked devices list in the right panel ───────────────────
    refreshCheckedDevicesList();
}

void UiManager::onDevicesOrGroupsChanged()
{
    refreshDevicesTab();
}

void UiManager::selectDeviceRow(QWidget *row, const DeviceInfo &info)
{
    const QString colAccent = "#007acc";

    // Deselect previous
    if (m_selectedDeviceRow && m_selectedDeviceRow != row) {
        const QString prevName = m_selectedDeviceRow->objectName();
        m_selectedDeviceRow->setStyleSheet(
            QString("QWidget#%1 { background-color: transparent; border-left: 2px solid transparent; }")
            .arg(prevName));
        if (auto *hl = qobject_cast<QHBoxLayout*>(m_selectedDeviceRow->layout())) {
            for (int i = 0; i < hl->count(); ++i) {
                if (auto *nameW = qobject_cast<QWidget*>(hl->itemAt(i)->widget())) {
                    if (nameW->layout()) {
                        if (auto *lbl = qobject_cast<QLabel*>(nameW->layout()->itemAt(0)->widget()))
                            lbl->setStyleSheet("color: #cccccc; font-weight: normal;");
                    }
                }
            }
        }
    }

    // Select new row
    m_selectedDeviceRow = row;
    const QString rowName = row->objectName();
    row->setStyleSheet(
        QString("QWidget#%1 { background-color: #4a4a52; border-left: 2px solid %2; }")
        .arg(rowName, colAccent));
    row->layout()->setContentsMargins(12, 8, 12, 8);

    updateDeviceDetails(info);
}

// ---------------------------------------------------------------------------
// Refresh the checked devices list in the right panel (SYSTEM INFORMATION area)
// ---------------------------------------------------------------------------
void UiManager::refreshCheckedDevicesList()
{
    const QString colText   = "#cccccc";
    const QString colMuted  = "#8a8a8a";
    const QString colGreen  = "#34d399";
    const QString colBorder = "#3e3e42";
    const QString colAccent = "#007acc";

    QVBoxLayout *listLayout = m_ui->devDeviceListVLayout;
    while (listLayout->count() > 0) {
        QLayoutItem *item = listLayout->takeAt(0);
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    if (m_checkedDevices.isEmpty()) {
        QLabel *empty = new QLabel(tr("No devices selected.\nTick checkboxes in the sidebar to select devices."));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        empty->setStyleSheet(
            QString("color: %1; font-style: italic;"
                    " background: transparent; border: none;").arg(colMuted));
        listLayout->addWidget(empty);

        // Disable all action buttons when no devices are checked
        for (QPushButton *btn : {m_ui->devBtnReboot,
                                  m_ui->devBtnSyslog, m_ui->devBtnClearCache,
                                  m_ui->devBtnUnlock,
                                  m_ui->devBtnForceStop, m_ui->devBtnAdbWireless,
                                  m_ui->devBtnAdbRoot, m_ui->devBtnAdbUnroot,
                                  m_ui->devBtnRebootFastboot,
                                  m_ui->devBtnPowerKey,
                                  m_ui->devBtnConnectWifi,
                                  m_ui->devBtnFlash,
                                  m_ui->devBtnDeployConfig})
            btn->setEnabled(false);
        return;
    }

    DevicesManager &dm = DevicesManager::instance();
    const QList<AdbDevice> connected = dm.connectedDevices();
    QMap<QString, AdbDevice> deviceById;
    for (const AdbDevice &d : connected)
        deviceById.insert(d.id, d);

    for (const QString &id : m_checkedDevices) {
        const bool online = deviceById.contains(id) && deviceById[id].isOnline;
        const QString name = deviceById.contains(id) ? deviceById[id].name : id;
        const QString statusColor = online ? colGreen : colMuted;
        const QString dotChar     = online ? QStringLiteral("●") : QStringLiteral("○");

        QWidget *rowW = new QWidget();
        rowW->setObjectName(QStringLiteral("devSelectedRow_") + id);
        rowW->setStyleSheet(
            QString("background: transparent; border-bottom: 1px solid %1;").arg(colBorder));
        rowW->setCursor(Qt::PointingHandCursor);
        QHBoxLayout *rl = new QHBoxLayout(rowW);
        rl->setContentsMargins(0, 8, 0, 8);
        rl->setSpacing(10);

        QLabel *dot = new QLabel(dotChar);
        dot->setStyleSheet(QString("color: %1; background: transparent; border: none;").arg(statusColor));
        dot->setFixedWidth(14);

        QLabel *nameLbl = new QLabel(name);
        nameLbl->setStyleSheet(
            QString("color: %1; background: transparent; border: none;").arg(colText));

        QLabel *serialLbl = new QLabel(id);
        serialLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        serialLbl->setStyleSheet(
            QString("color: %1; background: transparent; border: none;").arg(colMuted));

        QLabel *statusLbl = new QLabel(online ? tr("Online") : tr("Offline"));
        statusLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusLbl->setStyleSheet(
            QString("color: %1; background: transparent; border: none;").arg(statusColor));
        statusLbl->setFixedWidth(50);

        rl->addWidget(dot);
        rl->addWidget(nameLbl, 1);
        rl->addWidget(serialLbl, 1);
        rl->addWidget(statusLbl);

        // Click on this row to show device details
        rowW->installEventFilter(m_mainWindow);
        listLayout->addWidget(rowW);
    }

    // Enable/disable all action buttons based on checked devices
    const bool hasChecked = !m_checkedDevices.isEmpty();
    for (QPushButton *btn : {m_ui->devBtnReboot,
                              m_ui->devBtnSyslog, m_ui->devBtnClearCache,
                              m_ui->devBtnUnlock,
                              m_ui->devBtnForceStop, m_ui->devBtnAdbWireless,
                              m_ui->devBtnAdbRoot, m_ui->devBtnAdbUnroot,
                              m_ui->devBtnRebootFastboot,
                              m_ui->devBtnPowerKey,
                              m_ui->devBtnConnectWifi,
                              m_ui->devBtnFlash,
                              m_ui->devBtnDeployConfig})
        btn->setEnabled(hasChecked);
}

void UiManager::updateDeviceDetails(const DeviceInfo &info)
{
    const bool online = info.online;
    const bool hasDevice = !info.serial.isEmpty();

    // ── Header bar ──────────────────────────────────────────────────────────
    m_ui->devNameLabel->setText(hasDevice ? info.name : tr("Select a device"));

    // ── Dashboard info ────────────────────────────────────────────────────────
    m_ui->devBatteryValue->setText(online ? tr("...") : tr("--"));
    m_ui->devIpValue->setText(online ? tr("...") : tr("--"));
    m_ui->devNetworkValue->setText(online ? tr("...") : tr("--"));

    // Kick off async fetch for battery + IP
    if (online && !info.serial.isEmpty())
        DevicesManager::instance().fetchDeviceDetails(info.serial);

    // ── Action buttons: disable when offline OR no checked devices ────────────
    const bool canAct = online && !m_checkedDevices.isEmpty();
    for (QPushButton *btn : {m_ui->devBtnReboot,
                              m_ui->devBtnSyslog, m_ui->devBtnClearCache,
                              m_ui->devBtnUnlock,
                              m_ui->devBtnForceStop, m_ui->devBtnAdbWireless,
                              m_ui->devBtnAdbRoot, m_ui->devBtnAdbUnroot,
                              m_ui->devBtnRebootFastboot,
                              m_ui->devBtnPowerKey,
                              m_ui->devBtnConnectWifi,
                              m_ui->devBtnFlash,
                              m_ui->devBtnDeployConfig
                            })
        btn->setEnabled(canAct);

    // ── System information: reset all values ────────────────────────────────
    const QString placeholder = online ? tr("...") : tr("--");
    m_ui->devSiManufacturerValue->setText(placeholder);
    m_ui->devSiModelValue->setText(placeholder);
    m_ui->devSiAndroidVersionValue->setText(placeholder);
    m_ui->devSiSdkVersionValue->setText(placeholder);
    m_ui->devSiBuildNumberValue->setText(placeholder);
    m_ui->devSiBuildFingerprintValue->setText(placeholder);
    m_ui->devSiSecurityPatchValue->setText(placeholder);
    m_ui->devSiKernelVersionValue->setText(placeholder);
    m_ui->devSiAbiValue->setText(placeholder);
}

void UiManager::onDeviceDetailsFetched(const DeviceDetails &details)
{
    // Only update if the currently selected device matches.
    if (!m_selectedDeviceRow) return;
    const DeviceInfo &sel = m_deviceRowMap.value(m_selectedDeviceRow);
    if (sel.serial != details.serial) return;

    m_ui->devBatteryValue->setText(details.batteryLevel);
    m_ui->devIpValue->setText(details.ipAddress);
    m_ui->devNetworkValue->setText(details.networkStatus);

    // ── System information: update static labels ─────────────────────────────
    m_ui->devSiManufacturerValue->setText(details.manufacturer);
    m_ui->devSiModelValue->setText(details.model);
    m_ui->devSiAndroidVersionValue->setText(details.androidVersion);
    m_ui->devSiSdkVersionValue->setText(details.sdkVersion);
    m_ui->devSiBuildNumberValue->setText(details.buildNumber);
    m_ui->devSiBuildFingerprintValue->setText(details.buildFingerprint);
    m_ui->devSiSecurityPatchValue->setText(details.securityPatch);
    m_ui->devSiKernelVersionValue->setText(details.kernelVersion);
    m_ui->devSiAbiValue->setText(details.abi);
}


void UiManager::setupTooltips()
{
    m_ui->btnToggleTerminal->setToolTip(tr(Tooltips::btnToggleTerminal));
    m_ui->btnAppSettings->setToolTip(tr(Tooltips::btnAppSettings));
    m_ui->btnStart->setToolTip(tr(Tooltips::btnStart));
    m_ui->btnKernel->setToolTip(tr(Tooltips::btnKernel));
    m_ui->btnAutoScroll->setToolTip(tr(Tooltips::btnAutoScroll));
    m_ui->btnColumns->setToolTip(tr(Tooltips::btnColumns));
    m_ui->btnToggleCellContent->setToolTip(tr(Tooltips::btnToggleCellContent));
    m_ui->btnClear->setToolTip(tr(Tooltips::btnClear));
    m_ui->btnClearAllMarked->setToolTip(tr(Tooltips::btnClearAllMarked));
    m_ui->btnSave->setToolTip(tr(Tooltips::btnSave));
    m_ui->btnOpen->setToolTip(tr(Tooltips::btnOpen));
    m_ui->btnClearAllProperties->setToolTip(tr(Tooltips::btnClearAllProps));
    m_ui->btnFetchPropertyDefs->setToolTip(tr(Tooltips::btnFetchPropertyDefs));
}

void UiManager::setupFilterHistory()
{
    m_filterHistoryManager->track(m_ui->txtTagFilter,              QStringLiteral("tag"));
    m_filterHistoryManager->track(m_ui->txtPidFilter,              QStringLiteral("pid"));
    m_filterHistoryManager->track(m_ui->txtPackageFilter,          QStringLiteral("package"));
    m_filterHistoryManager->track(m_ui->txtFindMessage,            QStringLiteral("findMessage"));
    m_filterHistoryManager->track(m_ui->txtPropertySearch);
    m_filterHistoryManager->track(m_ui->txtKeyword,                QStringLiteral("keyword"));
    m_filterHistoryManager->trackDebounced(m_ui->txtFilterSettings,         QStringLiteral("settingsKey"));
    m_filterHistoryManager->trackDebounced(m_ui->txtFilterSettingsValue,    QStringLiteral("settingsValue"));
    m_filterHistoryManager->trackDebounced(m_ui->txtFilterProperties,       QStringLiteral("propertiesKey"));
    m_filterHistoryManager->trackDebounced(m_ui->txtFilterPropertiesValue,  QStringLiteral("propertiesValue"));

    // When the user picks a keyword/filter history item from the dropdown,
    // apply the filter immediately (same behavior as pressing Enter).
    for (QLineEdit *field : {m_ui->txtKeyword, m_ui->txtTagFilter,
                              m_ui->txtPidFilter, m_ui->txtPackageFilter,
                              m_ui->txtFindMessage}) {
        QCompleter *c = m_filterHistoryManager->completerFor(field);
        if (!c) continue;
        connect(c, QOverload<const QString &>::of(&QCompleter::activated),
                this, [this](const QString &) {
            // Defer by one event-loop tick so Qt's internal completer setText
            // has finished writing to the QLineEdit before we read it.
            QTimer::singleShot(0, this, &UiManager::onFilterChanged);
        });
    }
}

void UiManager::teardownSocket()
{
    if (m_socketServer)
        m_socketServer->stop();
    if (!m_currentDeviceId.isEmpty())
        AdbManager::instance().removeReversePort(m_currentDeviceId);
}

void UiManager::persistFilterHistory()
{
    m_filterHistoryManager->persistAll();
}

void UiManager::setupSplittersAndMisc()
{
    m_ui->splitter->setSizes(QList<int>() << 250 << 1150);
    m_ui->splitterLogTables->setSizes(QList<int>() << 600 << 200);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Signal connections
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::connectAdbManagerSignals()
{
    AdbManager &adb = AdbManager::instance();

    connect(&adb, &AdbManager::devicesChanged,           this, &UiManager::onDevicesChanged);
    connect(&adb, &AdbManager::logcatLineReceived,        this, &UiManager::onLogcatLineReceived);
    connect(&adb, &AdbManager::dmesgLineReceived,         this, &UiManager::parseDmesgLine);
    connect(&adb, &AdbManager::settingsFetched,           this, &UiManager::onSettingsFetched);
    connect(&adb, &AdbManager::propertiesFetched,         this, &UiManager::onPropertiesFetched);
    connect(&adb, &AdbManager::propertyDefinitionsFetched,this, &UiManager::onPropertyDefinitionsFetched);
    connect(&adb, &AdbManager::settingSaveResult,         this, &UiManager::onSettingSaveResult);
    connect(&adb, &AdbManager::propertySaveResult,        this, &UiManager::onPropertySaveResult);

    connect(&adb, &AdbManager::dmesgStopped, this, [this]() {
        m_ui->btnKernel->setStyleSheet(QString());
        m_ui->btnStart->setEnabled(true);
        m_ui->statusbar->showMessage("Kernel log stopped", 3000);
    });
    connect(&adb, &AdbManager::dmesgFailed, this, [this](const QString &reason) {
        m_ui->btnKernel->setStyleSheet(QString());
        m_ui->btnStart->setEnabled(true);
        QMessageBox::warning(m_mainWindow, tr("Kernel Log — Root Required"), reason);
    });
    connect(&adb, &AdbManager::logcatStarted, this, [this]() {
        m_ui->btnStart->setStyleSheet(
            QStringLiteral("background-color: #c0392b; border: 1px solid #e74c3c; color: white;"));
        m_ui->btnKernel->setEnabled(false);
        m_ui->statusbar->showMessage("Logcat started", 3000);
    });
    connect(&adb, &AdbManager::logcatStopped, this, [this]() {
        m_batchFlushTimer->stop();
        flushPendingLines();
        m_ui->btnStart->setStyleSheet(QString());
        m_ui->btnKernel->setEnabled(true);
        m_ui->statusbar->showMessage("Logcat stopped", 3000);
    });

    // Populate device list with whatever is already connected
    onDevicesChanged(adb.getConnectedDevices());
}

void UiManager::connectFilterSignals()
{
    // Highlight: typing updates visuals, Enter/buttons navigate matches
    connect(m_ui->txtHighlight,      &QLineEdit::textChanged,   this, &UiManager::onHighlightChanged);
    connect(m_ui->txtHighlight,      &QLineEdit::returnPressed, this, &UiManager::onHighlightNext);
    connect(m_ui->btnHighlightNext,  &QPushButton::clicked,     this, &UiManager::onHighlightNext);
    connect(m_ui->btnHighlightPrev,  &QPushButton::clicked,     this, &UiManager::onHighlightPrev);

    // Log filters: apply on Enter
    connect(m_ui->txtKeyword,       &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);
    connect(m_ui->txtFindMessage,   &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);
    connect(m_ui->txtStartTime,     &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);
    connect(m_ui->txtEndTime,       &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);
    connect(m_ui->txtTagFilter,     &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);
    connect(m_ui->txtPackageFilter, &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);
    connect(m_ui->txtPidFilter,     &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);

    // Log-level radio buttons
    connect(m_ui->radioVerbosePlus, &QRadioButton::toggled, this, &UiManager::onFilterChanged);
    connect(m_ui->radioV,           &QRadioButton::toggled, this, &UiManager::onFilterChanged);
    connect(m_ui->radioD,           &QRadioButton::toggled, this, &UiManager::onFilterChanged);
    connect(m_ui->radioI,           &QRadioButton::toggled, this, &UiManager::onFilterChanged);
    connect(m_ui->radioW,           &QRadioButton::toggled, this, &UiManager::onFilterChanged);
    connect(m_ui->radioE,           &QRadioButton::toggled, this, &UiManager::onFilterChanged);
    connect(m_ui->radioA,           &QRadioButton::toggled, this, &UiManager::onFilterChanged);

    // Configuration tab filters (live: text changed, not Enter)
    connect(m_ui->txtFilterSettings,       &QLineEdit::textChanged, this, &UiManager::onSettingsFilterChanged);
    connect(m_ui->txtFilterSettingsValue,  &QLineEdit::textChanged, this, &UiManager::onSettingsFilterChanged);
    connect(m_ui->txtFilterProperties,     &QLineEdit::textChanged, this, &UiManager::onPropertiesFilterChanged);
    connect(m_ui->txtFilterPropertiesValue,&QLineEdit::textChanged, this, &UiManager::onPropertiesFilterChanged);
    connect(m_ui->btnRefreshSettings,   &QPushButton::clicked, this, &UiManager::onRefreshSettingsClicked);
    connect(m_ui->btnRefreshProperties, &QPushButton::clicked, this, &UiManager::onRefreshPropertiesClicked);
}

void UiManager::connectButtonSignals()
{
    // Toolbar / main controls
    connect(m_ui->btnStart,        &QPushButton::clicked,  this, &UiManager::onStartClicked);
    connect(m_ui->btnKernel,       &QPushButton::clicked,  this, &UiManager::onKernelClicked);
    connect(m_ui->btnClear,        &QPushButton::clicked,  this, &UiManager::onClearClicked);
    connect(m_ui->btnSave,         &QPushButton::clicked,  this, &UiManager::onSaveFileClicked);
    connect(m_ui->btnColumns,      &QPushButton::clicked,  this, &UiManager::onColumnsClicked);
    connect(m_ui->btnAutoScroll,   &QPushButton::toggled,  this, &UiManager::onAutoScrollToggled);
    connect(m_ui->btnClearAllMarked,&QPushButton::clicked, this, &UiManager::onClearAllMarkedLog);
    connect(m_ui->btnToggleTerminal,&QPushButton::toggled, this, &UiManager::onToggleTerminal);
    connect(m_ui->btnAppSettings,  &QPushButton::clicked,  this, &UiManager::onAppSettingsClicked);

    connect(m_ui->btnToggleCellContent, &QPushButton::toggled, this, [this](bool checked) {
        m_ui->txtCellContent->setVisible(checked);
    });

    // btnColumns is superseded by App Settings — keep it hidden
    m_ui->btnColumns->setVisible(false);

    // File I/O
    connect(m_ui->txtFilePath, &QLineEdit::returnPressed, this, &UiManager::onLoadFileClicked);
    connect(m_ui->btnOpen,     &QPushButton::clicked,     this, &UiManager::onOpenFileClicked);

    // SDK
    connect(m_ui->txtPropertySearch,    &QLineEdit::returnPressed, this, [this]() {
        qDebug() << "[Completer] returnPressed";
        onAddPropertyDefinition();
    });
    connect(m_ui->btnClearAllProperties,    &QPushButton::clicked, this, &UiManager::onClearAllPropertyDefinitions);
    connect(m_ui->btnFetchPropertyDefs,     &QPushButton::clicked, this, &UiManager::onRefreshPropertyDefinitionValues);
    connect(m_ui->btnSavePropertySet,       &QPushButton::clicked, this, &UiManager::onSavePropertySet);
    connect(m_ui->btnLoadPropertySet,       &QPushButton::clicked, this, &UiManager::onLoadPropertySet);
    connect(m_ui->btnExportPropertySet,     &QPushButton::clicked, this, &UiManager::onExportPropertySet);
    connect(m_ui->btnImportPropertySet,     &QPushButton::clicked, this, &UiManager::onImportPropertySet);

    // Device selector
    connect(m_ui->cmbDevice, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UiManager::onDeviceChanged);

    // Default file path: <app dir>/log.log
    const QString appimageEnv = qEnvironmentVariable("APPIMAGE");
    const QString appDir = appimageEnv.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : QFileInfo(appimageEnv).absolutePath();
    m_ui->txtFilePath->setText(appDir + "/log.log");
}

void UiManager::connectTableSignals()
{
    // Main log table
    m_ui->tableLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->tableLog, &QTableView::customContextMenuRequested,
            this, &UiManager::onTableContextMenu);
    connect(m_ui->tableLog, &QTableView::doubleClicked,
            this, &UiManager::onLogTableDoubleClicked);
    connect(m_ui->tableLog, &QTableView::clicked,
            this, &UiManager::onLogTableClicked);

    // Mark log table
    connect(m_ui->tableMarkLog, &QTableView::clicked,
            this, &UiManager::onMarkLogTableClicked);
    m_ui->tableMarkLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->tableMarkLog, &QTableView::customContextMenuRequested,
            this, &UiManager::onMarkLogContextMenu);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Filter & Highlight
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onFilterChanged()
{
    applyFilters();
    updateFilterHighlighting();
}

void UiManager::onHighlightChanged()
{
    m_highlightRow = -1; // reset navigation on keyword change
    updateFilterHighlighting();
}

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

void UiManager::onHighlightNext()
{
    const QString text = m_ui->txtHighlight->text().trimmed();
    if (text.isEmpty() || filteredLogs.isEmpty()) return;

    QStringList kws;
    for (const QString &p : text.split("||", Qt::SkipEmptyParts))
        for (const QString &q : p.split("&&", Qt::SkipEmptyParts)) {
            const QString kw = q.trimmed();
            if (!kw.isEmpty()) kws << kw;
        }
    if (kws.isEmpty()) return;

    const int n     = filteredLogs.size();
    const int start = (m_highlightRow + 1) % n;
    for (int i = 0; i < n; ++i) {
        const int row = (start + i) % n;
        if (rowMatchesKeywords(filteredLogs[row], kws)) {
            m_highlightRow     = row;
            m_pendingCenterRow = row;
            m_ui->tableLog->selectRow(row);
            m_rowResizeTimer->start();
            m_ui->statusbar->showMessage(
                QString("Highlight: row %1 of %2").arg(row + 1).arg(n), 2000);
            return;
        }
    }
    m_ui->statusbar->showMessage("No highlight match found", 2000);
}

void UiManager::onHighlightPrev()
{
    const QString text = m_ui->txtHighlight->text().trimmed();
    if (text.isEmpty() || filteredLogs.isEmpty()) return;

    QStringList kws;
    for (const QString &p : text.split("||", Qt::SkipEmptyParts))
        for (const QString &q : p.split("&&", Qt::SkipEmptyParts)) {
            const QString kw = q.trimmed();
            if (!kw.isEmpty()) kws << kw;
        }
    if (kws.isEmpty()) return;

    const int n     = filteredLogs.size();
    const int start = (m_highlightRow <= 0 ? n : m_highlightRow) - 1;
    for (int i = 0; i < n; ++i) {
        const int row = ((start - i) % n + n) % n;
        if (rowMatchesKeywords(filteredLogs[row], kws)) {
            m_highlightRow     = row;
            m_pendingCenterRow = row;
            m_ui->tableLog->selectRow(row);
            m_rowResizeTimer->start();
            m_ui->statusbar->showMessage(
                QString("Highlight: row %1 of %2").arg(row + 1).arg(n), 2000);
            return;
        }
    }
    m_ui->statusbar->showMessage("No highlight match found", 2000);
}

void UiManager::updateFilterHighlighting()
{
    auto extractKeywords = [](const QString &filterText) -> QStringList {
        QStringList keywords;
        if (filterText.isEmpty()) return keywords;
        for (const QString &orPart : filterText.split("||", Qt::SkipEmptyParts))
            for (QString kw : orPart.split("&&", Qt::SkipEmptyParts)) {
                kw = kw.trimmed();
                if (!kw.isEmpty()) keywords.append(kw);
            }
        keywords.removeDuplicates();
        return keywords;
    };

    QStringList messageKeywords = extractKeywords(m_ui->txtFindMessage->text());
    QStringList tagKeywords     = extractKeywords(m_ui->txtTagFilter->text());
    QStringList packageKeywords = extractKeywords(m_ui->txtPackageFilter->text());
    QStringList pidKeywords     = extractKeywords(m_ui->txtPidFilter->text());

    // Highlight and keyword-filter words also illuminate all three text columns
    QStringList highlightKeywords = extractKeywords(m_ui->txtHighlight->text());
    highlightKeywords.append(extractKeywords(m_ui->txtKeyword->text()));
    highlightKeywords.removeDuplicates();

    messageKeywords.append(highlightKeywords); messageKeywords.removeDuplicates();
    tagKeywords.append(highlightKeywords);     tagKeywords.removeDuplicates();
    packageKeywords.append(highlightKeywords); packageKeywords.removeDuplicates();

    pidKeywords.isEmpty()     ? m_pidHighlightDelegate->clearKeywords()
                              : m_pidHighlightDelegate->setKeywords(pidKeywords);
    packageKeywords.isEmpty() ? m_packageHighlightDelegate->clearKeywords()
                              : m_packageHighlightDelegate->setKeywords(packageKeywords);
    tagKeywords.isEmpty()     ? m_tagHighlightDelegate->clearKeywords()
                              : m_tagHighlightDelegate->setKeywords(tagKeywords);
    messageKeywords.isEmpty() ? m_messageHighlightDelegate->clearKeywords()
                              : m_messageHighlightDelegate->setKeywords(messageKeywords);

    m_ui->tableLog->viewport()->update();
}

void UiManager::onSettingsFilterChanged()
{
    m_settingsModel->applyFilter(m_ui->txtFilterSettings->text(),
                                 m_ui->txtFilterSettingsValue->text());
    recreateSettingsButtons();
}

void UiManager::onPropertiesFilterChanged()
{
    m_propertiesModel->applyFilter(m_ui->txtFilterProperties->text(),
                                   m_ui->txtFilterPropertiesValue->text());
    recreatePropertiesButtons();
}

FilterCriteria UiManager::buildFilterCriteria() const
{
    FilterCriteria criteria;

    criteria.keywordFilter = m_ui->txtKeyword->text().trimmed();
    if (!criteria.keywordFilter.isEmpty())
        criteria.keywordRegex = QRegularExpression(criteria.keywordFilter,
                                                   QRegularExpression::CaseInsensitiveOption);

    FilterCriteria::applyFilter(criteria.messageFilter, criteria.messageOperator,
                                m_ui->txtFindMessage->text());
    FilterCriteria::applyFilter(criteria.tagFilter,     criteria.tagOperator,
                                m_ui->txtTagFilter->text());
    FilterCriteria::applyFilter(criteria.packageFilter, criteria.packageOperator,
                                m_ui->txtPackageFilter->text());
    FilterCriteria::applyFilter(criteria.pidFilter,     criteria.pidOperator,
                                m_ui->txtPidFilter->text());

    criteria.startTime  = m_ui->txtStartTime->text();
    criteria.endTime    = m_ui->txtEndTime->text();
    criteria.tidFilter  = "";
    criteria.tidOperator = FilterOperator::OR;

    if      (m_ui->radioVerbosePlus->isChecked()) criteria.minLevel = "V";
    else if (m_ui->radioV->isChecked())           criteria.minLevel = "V";
    else if (m_ui->radioD->isChecked())           criteria.minLevel = "D";
    else if (m_ui->radioI->isChecked())           criteria.minLevel = "I";
    else if (m_ui->radioW->isChecked())           criteria.minLevel = "W";
    else if (m_ui->radioE->isChecked())           criteria.minLevel = "E";
    else if (m_ui->radioA->isChecked())           criteria.minLevel = "A";

    criteria.parsedMessage = ParsedFilter::build(criteria.messageFilter);
    criteria.parsedTag     = ParsedFilter::build(criteria.tagFilter);
    criteria.parsedPackage = ParsedFilter::build(criteria.packageFilter);
    criteria.parsedPid     = ParsedFilter::build(criteria.pidFilter);
    criteria.parsedTid     = ParsedFilter::build(criteria.tidFilter);

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

void UiManager::applyFilters()
{
    filteredLogs.clear();
    m_filteredLogsIndex.clear();

    const FilterCriteria criteria = buildFilterCriteria();

    filteredLogs = QtConcurrent::blockingFiltered(allLogs,
        [&criteria, this](const LogEntry &entry) {
            return m_logFilter.passesFilter(entry, criteria);
        });

    m_filteredLogsIndex.reserve(filteredLogs.size());
    for (int i = 0; i < filteredLogs.size(); ++i)
        m_filteredLogsIndex[filteredLogs[i].id] = i;

    m_logModel->setLogs(filteredLogs);

    // Rebuild m_markedRows to reflect which marked logs are in current filtered view
    m_markedRows.clear();
    int markedCount = m_markLogModel->getMarkedCount();
    for (int i = 0; i < markedCount; ++i) {
        int allLogsIndex  = m_markLogModel->getOriginalIndex(i);
        int filteredRow   = findLogInFilteredLogs(allLogsIndex);
        if (filteredRow >= 0) m_markedRows.insert(filteredRow);
    }
    m_logModel->setMarkedRows(&m_markedRows);

    updateFilterHighlighting();
    updateFilterCount();
    updateStatusBar();

    if (m_rowResizeTimer) m_rowResizeTimer->start();
}

bool UiManager::passesFilter(const LogEntry &entry)
{
    return m_logFilter.passesFilter(entry, buildFilterCriteria());
}

void UiManager::updateFilterCount()
{
    m_ui->lblFilterCount->setText(
        QString("Showing: %1 / %2").arg(filteredLogs.size()).arg(allLogs.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Device / Logcat
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onDeviceChanged(int index)
{
    const QString oldDeviceId = m_currentDeviceId;
    const QString deviceName  = m_ui->cmbDevice->itemText(index);
    const QString deviceId    = m_ui->cmbDevice->itemData(index).toString();

    m_currentDeviceId = deviceId;
    AdbManager::instance().setCurrentDeviceId(deviceId);

    // On device switch: tear down old reverse port and drop stale socket
    if (!oldDeviceId.isEmpty() && oldDeviceId != deviceId) {
        AdbManager::instance().removeReversePort(oldDeviceId);
        if (m_socketServer)
            m_socketServer->disconnectClient();
    }

    if (!deviceId.isEmpty()) {
        m_ui->statusbar->showMessage(QString("Selected device: %1").arg(deviceName), 2000);
        m_dumpsysServices.clear();
        const QString prevService = m_ui->txtDumpsysService->text().trimmed();
        m_ui->txtDumpsysService->setPlaceholderText(tr("Loading services..."));
        AdbManager::instance().setupReversePort(deviceId);
        AdbManager::instance().fetchDumpsysList(deviceId);

        // Re-fetch the same dumpsys service on the new device
        if (!prevService.isEmpty()) {
            m_ui->txtDumpsysCmdResult->setPlainText(QStringLiteral("..."));
            AdbManager::instance().fetchDumpsys(deviceId, prevService);
        }
    }
    updateDumpsysCommandText();
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

        // Clear device-specific data
        m_settingsModel->setSettings({});
        m_propertiesModel->setProperties({});
        m_availablePropertyDefinitions.clear();
        updatePropertyNamesCompleter();
        if (m_socketServer)
            m_socketServer->disconnectClient();
        m_ui->txtDumpsysCmdResult->clear();
        m_dumpsysServices.clear();
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
            AdbManager::instance().setupReversePort(m_currentDeviceId);
            AdbManager::instance().fetchSettings(m_currentDeviceId);
            AdbManager::instance().fetchProperties(m_currentDeviceId);
            AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
        }
    }
}

void UiManager::onLogcatLineReceived(const QString &line)
{
    m_pendingLines.append(line);
    if (!m_batchFlushTimer->isActive())
        m_batchFlushTimer->start();
}

void UiManager::onStartClicked()
{
    AdbManager &mgr = AdbManager::instance();

    if (mgr.isLogcatRunning()) {
        mgr.stopLogcat();
        return;
    }

    if (m_currentDeviceId.isEmpty()) {
        m_ui->statusbar->showMessage("No device selected", 3000);
        return;
    }

    // Clear all in-memory state for a fresh live session
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
        m_ui->statusbar->showMessage("Failed to start logcat", 5000);
}

void UiManager::onClearClicked()
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

    if (!m_currentDeviceId.isEmpty()) {
        const QString adbPath = AdbManager::instance().getAdbPath();
        QProcess clearProcess;
        const bool isDmesg = AdbManager::instance().isDmesgRunning();
        clearProcess.start(adbPath,
            isDmesg ? AdbCommand::clearDmesg(m_currentDeviceId)
                    : AdbCommand::clearLogcat(m_currentDeviceId));

        const bool cleared = clearProcess.waitForFinished(3000);
        m_ui->statusbar->showMessage(
            cleared ? (isDmesg ? "Cleared local logs and device dmesg buffer"
                               : "Cleared local logs and device logcat buffer")
                    : "Cleared local logs (device buffer clear failed)",
            3000);
    } else {
        m_ui->statusbar->showMessage("Cleared local logs", 3000);
    }
}

void UiManager::flushPendingLines()
{
    if (m_pendingLines.isEmpty()) {
        m_batchFlushTimer->stop();
        return;
    }

    QVector<QString> lines;
    lines.swap(m_pendingLines);

    QVector<LogEntry> toAdd;
    toAdd.reserve(lines.size());
    const FilterCriteria criteria = buildFilterCriteria();

    for (const QString &line : lines) {
        LogEntry entry = m_logConverter->convert(line);
        if (!entry.isValid()) continue;

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
        m_logModel->addLogs(toAdd);
        m_rowResizeTimer->start();
        if (m_ui->btnAutoScroll->isChecked())
            m_ui->tableLog->scrollToBottom();
        updateFilterCount();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Kernel (dmesg)
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onKernelClicked()
{
    AdbManager &mgr = AdbManager::instance();

    if (mgr.isDmesgRunning()) {
        mgr.stopDmesg();
        return;
    }

    if (m_currentDeviceId.isEmpty()) {
        m_ui->statusbar->showMessage("No device selected", 3000);
        return;
    }

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
        m_ui->statusbar->showMessage("Failed to start dmesg", 5000);
        return;
    }

    m_ui->btnKernel->setStyleSheet(
        QStringLiteral("background-color: #c0392b; border: 1px solid #e74c3c; color: white;"));
    m_ui->btnStart->setEnabled(false);
    m_ui->statusbar->showMessage("Kernel log started (adb shell dmesg -w)", 5000);
}

void UiManager::parseDmesgLine(const QString &line)
{
    static const QRegularExpression re(
        R"(^\[\s*([\d.]+)\]\s*(?:\[([^\]]*)\])?\s*(.*)$)");

    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch()) return;

    LogEntry entry;
    entry.time    = m.captured(1).trimmed();
    entry.tag     = m.captured(2).trimmed();
    entry.message = m.captured(3).trimmed();
    if (entry.tag.isEmpty()) entry.tag = QStringLiteral("KERNEL");
    entry.level = QStringLiteral("I");

    entry.id = ++m_nextLogId;
    m_allLogsIndex[entry.id] = allLogs.size();
    allLogs.append(entry);

    if (passesFilter(entry)) {
        m_filteredLogsIndex[entry.id] = filteredLogs.size();
        filteredLogs.append(entry);
        m_logModel->addLog(entry);
        if (m_ui->btnAutoScroll->isChecked())
            m_ui->tableLog->scrollToBottom();
        updateFilterCount();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: File I/O
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onLoadFileClicked()
{
    const QString filePath = m_ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
        m_ui->statusbar->showMessage("Please enter a file path", 3000);
        return;
    }
    loadLogsFromFile(filePath);
}

void UiManager::onOpenFileClicked()
{
    const QString currentPath = m_ui->txtFilePath->text().trimmed();
    const QString defaultPath = currentPath.isEmpty() ? QDir::homePath() : currentPath;

    const QString filePath = QFileDialog::getOpenFileName(
        m_mainWindow, "Open Log File", defaultPath,
        "Log Files (*.log *.txt);;All Files (*.*)");

    if (!filePath.isEmpty()) {
        m_ui->txtFilePath->setText(filePath);
        loadLogsFromFile(filePath);
    }
}

void UiManager::onSaveFileClicked()
{
    QString filePath = m_ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
        m_ui->statusbar->showMessage("Please enter a file path", 3000);
        return;
    }
    if (!filePath.endsWith(".log") && !filePath.endsWith(".txt"))
        filePath += ".log";

    QString errorMsg;
    if (m_fileManager.saveToFile(filePath, allLogs, errorMsg))
        m_ui->statusbar->showMessage(
            QString("Saved %1 log entries to %2").arg(allLogs.size()).arg(filePath), 3000);
    else
        m_ui->statusbar->showMessage(QString("Failed to save: %1").arg(errorMsg), 5000);
}

void UiManager::loadLogsFromFile(const QString &filePath)
{
    if (m_isLoadingFile) {
        m_ui->statusbar->showMessage("File loading already in progress…", 3000);
        return;
    }
    m_isLoadingFile = true;
    m_ui->btnOpen->setEnabled(false);
    m_ui->statusbar->showMessage("Loading log file…", 0);

    QVector<LogConverterPtr> converters;
    converters.append(LogConverterPtr(new ThreadtimeLogConverter()));
    converters.append(LogConverterPtr(new BriefLogConverter()));

    QFuture<FileLoadResult> future = QtConcurrent::run(
        [filePath, converters]() -> FileLoadResult {
            FileLoadResult result;
            result.filePath = filePath;
            FileManager fm;
            QVector<LogEntry> raw = fm.readFromFileAuto(
                filePath, converters, result.converter, result.errorMsg);
            result.parsedCount = fm.getLastParsedCount();
            result.lineCount   = fm.getLastLineCount();
            if (raw.isEmpty()) return result;

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

    if (!m_fileLoaderWatcher) {
        m_fileLoaderWatcher = new QFutureWatcher<FileLoadResult>(this);
        connect(m_fileLoaderWatcher, &QFutureWatcher<FileLoadResult>::finished,
                this, &UiManager::onFileLoadFinished);
    } else {
        m_fileLoaderWatcher->cancel();
        m_fileLoaderWatcher->waitForFinished();
    }
    m_fileLoaderWatcher->setFuture(future);
}

void UiManager::onFileLoadFinished()
{
    m_isLoadingFile = false;
    m_ui->btnOpen->setEnabled(true);

    FileLoadResult res = m_fileLoaderWatcher->future().takeResult();

    if (res.entries.isEmpty() && !res.errorMsg.isEmpty()) {
        m_ui->statusbar->showMessage(
            QString("Failed to load file: %1").arg(res.errorMsg), 5000);
        return;
    }
    if (res.entries.isEmpty()) {
        m_ui->statusbar->showMessage("No valid log entries found in file", 5000);
        return;
    }

    const int entryCount = res.entries.size();

    allLogs          = std::move(res.entries);
    m_allLogsIndex   = std::move(res.allLogsIndex);
    m_nextLogId      = res.nextLogId;
    m_markLogModel->clear();
    if (res.converter) m_logConverter = res.converter;

    // Block signals while clearing filter fields (avoid spurious filter-change events)
    const QSignalBlocker b1(m_ui->txtFindMessage);
    const QSignalBlocker b2(m_ui->txtStartTime);
    const QSignalBlocker b3(m_ui->txtEndTime);
    const QSignalBlocker b4(m_ui->txtTagFilter);
    const QSignalBlocker b5(m_ui->txtPackageFilter);
    const QSignalBlocker b6(m_ui->txtPidFilter);
    m_ui->txtFindMessage->clear();
    m_ui->txtStartTime->clear();
    m_ui->txtEndTime->clear();
    m_ui->txtTagFilter->clear();
    m_ui->txtPackageFilter->clear();
    m_ui->txtPidFilter->clear();

    // All filters cleared → filteredLogs == allLogs (O(1) implicit-share copies)
    filteredLogs        = allLogs;
    m_filteredLogsIndex = m_allLogsIndex;
    m_markedRows.clear();

    m_logModel->setLogs(filteredLogs);
    m_logModel->setMarkedRows(&m_markedRows);

    updateFilterHighlighting();
    updateFilterCount();
    updateStatusBar();

    m_ui->statusbar->showMessage(
        QString("Loaded %1 log entries from %2 (Format: %3, Parsed: %4/%5)")
            .arg(entryCount)
            .arg(res.filePath)
            .arg(res.converter ? res.converter->name() : "Unknown")
            .arg(res.parsedCount)
            .arg(res.lineCount),
        5000);

    m_rowResizeTimer->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Configuration Tab (Settings & Properties)
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onRefreshSettingsClicked()
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    AdbManager::instance().fetchSettings(m_currentDeviceId);
}

void UiManager::onRefreshPropertiesClicked()
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    AdbManager::instance().fetchProperties(m_currentDeviceId);
}

void UiManager::onSettingsFetched(const QVector<SettingEntry> &settings)
{
    qDebug() << "UiManager::onSettingsFetched: updating" << settings.size() << "entries";
    m_settingsModel->updateSettings(settings);
    m_settingsModel->reapplyFilter();
    recreateSettingsButtons();
}

void UiManager::onPropertiesFetched(const QVector<PropertyEntry> &properties)
{
    m_propertiesModel->updateProperties(properties);
    m_propertiesModel->reapplyFilter();
    recreatePropertiesButtons();
}

void UiManager::onSaveSettingClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    if (row < 0 || row >= m_settingsModel->rowCount()) return;

    using namespace TableConfig::SettingsColumns;
    const QString group   = m_settingsModel->data(m_settingsModel->index(row, GROUP)).toString();
    const QString setting = m_settingsModel->data(m_settingsModel->index(row, SETTING)).toString();
    const QString value   = m_settingsModel->data(m_settingsModel->index(row, VALUE)).toString();

    AdbManager::instance().saveSettingAsync(row, m_currentDeviceId, group, setting, value);
}

void UiManager::onSettingSaveResult(int /*row*/, bool success,
                                     const QString &group, const QString &setting,
                                     const QString &newValue, const QString &verifiedValue,
                                     const QString &error)
{
    if (!success) {
        QMessageBox::warning(m_mainWindow, "Failed to Set Value",
                             QString("Failed to set %1.%2:\n%3").arg(group, setting, error));
        return;
    }
    if (verifiedValue != newValue) {
        QMessageBox::warning(m_mainWindow, "Value Not Set",
                             QString("Setting %1.%2 could not be set.\n"
                                     "Expected: %3\nActual: %4\n\n"
                                     "This setting may be read-only or require special permissions.")
                                 .arg(group, setting, newValue,
                                      verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        m_ui->statusbar->showMessage(
            QString("Successfully set %1.%2 = %3").arg(group, setting, newValue), 3000);
    }
}

void UiManager::onSavePropertyClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    if (row < 0 || row >= m_propertiesModel->rowCount()) return;

    using namespace TableConfig::PropertiesColumns;
    const QString property = m_propertiesModel->data(m_propertiesModel->index(row, PROPERTY)).toString();
    const QString value    = m_propertiesModel->data(m_propertiesModel->index(row, VALUE)).toString();

    AdbManager::instance().savePropertyAsync(row, m_currentDeviceId, property, value);
}

void UiManager::onPropertySaveResult(int /*row*/, bool success,
                                      const QString &property,
                                      const QString &newValue, const QString &verifiedValue,
                                      const QString &error)
{
    if (!success) {
        QMessageBox::warning(m_mainWindow, "Failed to Set Property",
                             QString("Failed to set %1:\n%2").arg(property, error));
        return;
    }
    if (verifiedValue != newValue) {
        QMessageBox::warning(m_mainWindow, "Property Not Set",
                             QString("Property %1 could not be set.\n"
                                     "Expected: %2\nActual: %3\n\n"
                                     "This property may be read-only or require special permissions.")
                                 .arg(property, newValue,
                                      verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        m_ui->statusbar->showMessage(
            QString("Successfully set %1 = %2").arg(property, newValue), 3000);
    }
}

// ── Button factories ───────────────────────────────────────────────────────

QPushButton *UiManager::createActionButton(const QString &label,
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

void UiManager::recreateSettingsButtons()
{
    using namespace TableConfig::SettingsColumns;
    const int rowCount = m_settingsModel->rowCount();

    for (int i = 0; i < rowCount; ++i) {
        QWidget *old = m_ui->tableSettings->indexWidget(m_settingsModel->index(i, ACTION));
        if (old) { m_ui->tableSettings->setIndexWidget(m_settingsModel->index(i, ACTION), nullptr); old->deleteLater(); }
    }
    for (int i = 0; i < rowCount; ++i) {
        QPushButton *btn = createActionButton("", "Save this setting to device", 50, nullptr);
        btn->setIcon(QIcon(":/icons/download.svg"));
        m_ui->tableSettings->setIndexWidget(m_settingsModel->index(i, ACTION), btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onSaveSettingClicked(i); });
    }
}

void UiManager::recreatePropertiesButtons()
{
    using namespace TableConfig::PropertiesColumns;
    const int rowCount = m_propertiesModel->rowCount();

    for (int i = 0; i < rowCount; ++i) {
        QWidget *old = m_ui->tableProperties->indexWidget(m_propertiesModel->index(i, ACTION));
        if (old) { m_ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, ACTION), nullptr); old->deleteLater(); }
    }
    for (int i = 0; i < rowCount; ++i) {
        QPushButton *btn = createActionButton("", "Save this property to device", 50, nullptr);
        btn->setIcon(QIcon(":/icons/download.svg"));
        m_ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, ACTION), btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onSavePropertyClicked(i); });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: SDK Tab (Property Definitions)
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onSearchPropertyDefinition()
{
    const QString searchText = m_ui->txtPropertySearch->text().trimmed();
    if (searchText.isEmpty()) return;

    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions) {
        if (propDef.name.compare(searchText, Qt::CaseInsensitive) == 0) {
            m_ui->statusbar->showMessage(
                QString("Found: %1 (ID: %2, Supported: %3)")
                    .arg(propDef.name).arg(propDef.id)
                    .arg(propDef.isSupported ? "Yes" : "No"),
                3000);
            return;
        }
    }
    m_ui->statusbar->showMessage(QString("Property '%1' not found").arg(searchText), 3000);
}

void UiManager::onAddPropertyDefinition()
{
    const QString searchText = m_ui->txtPropertySearch->text().trimmed();
    qDebug() << "[onAddPropertyDefinition] called, text=" << searchText;
    if (searchText.isEmpty()) {
        return;
    }

    PropertyDefinition selectedProp;
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions) {
        if (propDef.name.compare(searchText, Qt::CaseInsensitive) == 0) {
            selectedProp = propDef;
            break;
        }
    }
    if (!selectedProp.isValid()) {
        QMessageBox::warning(m_mainWindow, "Property Not Found",
                             QString("Property '%1' not found. Please fetch property definitions first.")
                                 .arg(searchText));
        return;
    }

    const int oldRowCount = m_propertyDefinitionModel->rowCount();
    m_propertyDefinitionModel->addPropertyDefinition(selectedProp);
    const int newRowCount = m_propertyDefinitionModel->rowCount();

    if (newRowCount <= oldRowCount) {
        m_ui->statusbar->showMessage(
            QString("Property '%1' already in list").arg(selectedProp.name), 2000);
        return;
    }

    const int row = newRowCount - 1;
    using namespace TableConfig::PropertyDefColumns;

    static const QString iconBtnStyle =
        "QPushButton { padding: 0px; border: none; background: transparent; border-radius: 4px; }"
        "QPushButton:hover { background-color: rgba(255,255,255,40); }"
        "QPushButton:pressed { background-color: rgba(255,255,255,70); }";

    auto makeCenteredBtn = [&](const QString &icon, const QString &tooltip)
        -> std::pair<QWidget*, QPushButton*>
    {
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
    m_ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, SET_BUTTON), cellSet);
    connect(btnSet, &QPushButton::clicked, this, [this, row]() { onSetPropertyDefinitionClicked(row); });

    auto [cellGet, btnGet] = makeCenteredBtn(":/icons/refresh.svg", "Get property value");
    m_ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, GET_BUTTON), cellGet);
    connect(btnGet, &QPushButton::clicked, this, [this, row]() { onGetPropertyDefinitionClicked(row); });

    auto [cellRemove, btnRemove] = makeCenteredBtn(":/icons/edit-delete.svg", "Remove property from list");
    m_ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, REMOVE_BUTTON), cellRemove);
    connect(btnRemove, &QPushButton::clicked, this, [this, row]() { onRemovePropertyDefinitionClicked(row); });

    m_ui->statusbar->showMessage(QString("Added property: %1").arg(selectedProp.name), 2000);

    QTimer::singleShot(0, m_ui->txtPropertySearch, [this]() {
        m_ui->txtPropertySearch->clear();
    });
}

void UiManager::onRefreshPropertyDefinitionValues()
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    const QVector<PropertyDefinition> properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (properties.isEmpty()) {
        m_ui->statusbar->showMessage("No property definitions loaded.", 3000);
        return;
    }

    m_ui->btnFetchPropertyDefs->setEnabled(false);
    m_ui->statusbar->showMessage(
        QString("Refreshing values for %1 properties...").arg(properties.size()), 0);

    const QString deviceId = m_currentDeviceId;

    QtConcurrent::run([this, properties, deviceId]() {
        using Pair = std::pair<int, PropertyDefinition>;
        QVector<Pair> results;
        results.reserve(properties.size());

        for (int row = 0; row < properties.size(); ++row) {
            const PropertyDefinition &propDef = properties[row];
            const QString queryKey = propDef.id.isEmpty() ? propDef.name : propDef.id;

            QString output, error;
            if (!AdbManager::instance().getPropertyDefinitionValue(deviceId, queryKey, output, error))
                continue;

            PropertyDefinition updated;
            for (const QString &ln : output.split('\n', Qt::SkipEmptyParts)) {
                updated = PropertyDefinitionConverter::parseLine(ln.trimmed());
                if (updated.isValid()) break;
            }
            if (!updated.isValid()) updated = PropertyDefinitionConverter::parseLine(output);
            if (!updated.isValid()) continue;
            if (updated.name.isEmpty()) updated.name = propDef.name;

            results.append({row, updated});
        }

        QMetaObject::invokeMethod(this, [this, results]() {
            for (const auto &[row, updated] : results)
                m_propertyDefinitionModel->updatePropertyDefinition(row, updated);
            m_ui->btnFetchPropertyDefs->setEnabled(true);
            m_ui->statusbar->showMessage(
                QString("Refreshed values for %1 / %2 properties")
                    .arg(results.size())
                    .arg(m_propertyDefinitionModel->rowCount()),
                3000);
        }, Qt::QueuedConnection);
    });
}

void UiManager::onFetchPropertyDefinitions()
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    m_ui->statusbar->showMessage("Fetching property definitions...", 0);
    AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
}

void UiManager::onPropertyDefinitionsFetched(const QVector<PropertyDefinition> &defs)
{
    m_availablePropertyDefinitions = defs;
    updatePropertyNamesCompleter();

    // Build a lookup map by name so we can update current model values
    QHash<QString, QString> valueByName;
    valueByName.reserve(defs.size());
    for (const PropertyDefinition &d : defs)
        valueByName.insert(d.name, d.value);

    const QVector<PropertyDefinition> &modelDefs = m_propertyDefinitionModel->getPropertyDefinitions();
    for (int i = 0; i < modelDefs.size(); ++i) {
        auto it = valueByName.constFind(modelDefs[i].name);
        if (it != valueByName.constEnd()) {
            PropertyDefinition updated = modelDefs[i];
            updated.value = it.value();
            m_propertyDefinitionModel->updatePropertyDefinition(i, updated);
        }
    }

    m_ui->statusbar->showMessage(
        QString("Fetched %1 property definitions").arg(defs.size()), 3000);
}

void UiManager::onGetPropertyDefinitionClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) return;

    const PropertyDefinition &propDef = properties[row];
    const QString queryKey = propDef.id.isEmpty() ? propDef.name : propDef.id;
    QString output, error;

    if (!AdbManager::instance().getPropertyDefinitionValue(m_currentDeviceId, queryKey, output, error)) {
        QMessageBox::warning(m_mainWindow, "Failed to Get Value",
                             QString("Failed to get %1:\n%2").arg(propDef.name, error));
        return;
    }

    PropertyDefinition updatedProp;
    for (const QString &ln : output.split('\n', Qt::SkipEmptyParts)) {
        updatedProp = PropertyDefinitionConverter::parseLine(ln.trimmed());
        if (updatedProp.isValid()) break;
    }
    if (!updatedProp.isValid()) updatedProp = PropertyDefinitionConverter::parseLine(output);

    if (!updatedProp.isValid()) {
        QMessageBox::warning(m_mainWindow, "Parse Error",
                             QString("Failed to parse output for %1\n\nRaw output:\n%2")
                                 .arg(propDef.name, output.left(500)));
        return;
    }
    if (updatedProp.name.isEmpty()) updatedProp.name = propDef.name;
    m_propertyDefinitionModel->updatePropertyDefinition(row, updatedProp);
    m_ui->statusbar->showMessage(QString("Updated property: %1").arg(updatedProp.name), 2000);
}

void UiManager::onSetPropertyDefinitionClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) return;

    const PropertyDefinition &propDef = properties[row];
    if (propDef.readOnly) {
        QMessageBox::warning(m_mainWindow, "Read-Only Property",
                             QString("Property '%1' is marked as read-only.").arg(propDef.name));
        return;
    }

    using namespace TableConfig::PropertyDefColumns;
    QString value = m_propertyDefinitionModel->data(
                        m_propertyDefinitionModel->index(row, VALUE), Qt::DisplayRole).toString();
    value = value.replace("\"", "\\\"").replace("{", "\\{");

    QString error;
    if (AdbManager::instance().setPropertyDefinitionValue(m_currentDeviceId, propDef.id, value, error))
        m_ui->statusbar->showMessage(QString("Set %1 = %2").arg(propDef.id, value), 3000);
    else
        QMessageBox::warning(m_mainWindow, "Failed to Set Value",
                             QString("Failed to set %1:\n%2").arg(propDef.id, error));
}

void UiManager::onRemovePropertyDefinitionClicked(int row)
{
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) return;
    const PropertyDefinition prop = properties[row];
    m_propertyDefinitionModel->removePropertyDefinition(row);
    m_ui->statusbar->showMessage(QString("Removed property: %1").arg(prop.name), 2000);
}

void UiManager::onClearAllPropertyDefinitions()
{
    if (m_propertyDefinitionModel->rowCount() == 0) {
        QMessageBox::information(m_mainWindow, "Empty List", "The property list is already empty.");
        return;
    }
    if (QMessageBox::question(m_mainWindow, "Clear All Properties",
            QString("Remove all %1 properties from the list?")
                .arg(m_propertyDefinitionModel->rowCount()),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        m_propertyDefinitionModel->clear();
        m_ui->statusbar->showMessage("Cleared all properties", 2000);
    }
}

void UiManager::updatePropertyNamesCompleter()
{
    QStringList names;
    names.reserve(m_availablePropertyDefinitions.size());
    for (const PropertyDefinition &p : m_availablePropertyDefinitions)
        names.append(p.name);

    QCompleter *completer = m_ui->txtPropertySearch->completer();
    if (completer)
        qobject_cast<QStringListModel *>(completer->model())->setStringList(names);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Dumpsys Tab
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::updateDumpsysCommandText()
{
    const QString service = m_ui->txtDumpsysService->text().trimmed();
    const QString pkg     = m_ui->txtPackageFilter->text().trimmed();

    if (m_currentDeviceId.isEmpty() && service.isEmpty() && pkg.isEmpty()) {
        m_ui->txtDumpsysCommand->clear();
        return;
    }

    const QString device = m_currentDeviceId.isEmpty()
                               ? QStringLiteral("<device-id>")
                               : m_currentDeviceId;

    // Build the argument part: service and/or package
    QString args = service;
    if (!pkg.isEmpty())
        args = args.isEmpty() ? pkg : args + QLatin1Char(' ') + pkg;
    if (args.isEmpty())
        args = QStringLiteral("<package-name>");

    m_ui->txtDumpsysCommand->setText(
        QStringLiteral("adb -s ") + device + QStringLiteral(" shell dumpsys ") + args);
}

void UiManager::onRunDumpsysClicked()
{
    if (m_currentDeviceId.isEmpty()) {
        m_ui->statusbar->showMessage("No device selected", 3000);
        return;
    }
    const QString service = m_ui->txtDumpsysService->text().trimmed();
    if (service.isEmpty()) {
        m_ui->statusbar->showMessage("No service specified", 3000);
        return;
    }
    m_ui->txtDumpsysCmdResult->setPlainText("…");
    AdbManager::instance().fetchDumpsys(m_currentDeviceId, service);
}

void UiManager::onDumpsysFetched(const QString &output)
{
    m_ui->txtDumpsysCmdResult->setPlainText(output);
    const QString needle = m_ui->txtDumpsysSearch->text();
    applyDumpsysHighlights(needle);
    if (!needle.isEmpty()) {
        QTextCursor c = m_ui->txtDumpsysCmdResult->textCursor();
        c.movePosition(QTextCursor::Start);
        m_ui->txtDumpsysCmdResult->setTextCursor(c);
        m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindFlags());
    }
    m_ui->statusbar->showMessage(
        QString("Dumpsys: %1 lines").arg(output.count('\n')), 3000);
}

void UiManager::applyDumpsysHighlights(const QString &needle)
{
    QList<QTextEdit::ExtraSelection> extras;
    if (!needle.isEmpty()) {
        QTextCharFormat fmt;
        fmt.setBackground(QColor("#b8860b"));
        fmt.setForeground(QColor("#ffffff"));

        QTextDocument *doc = m_ui->txtDumpsysCmdResult->document();
        QTextCursor cur(doc);
        int count = 0;
        while (!(cur = doc->find(needle, cur)).isNull()) {
            QTextEdit::ExtraSelection sel;
            sel.format = fmt;
            sel.cursor = cur;
            extras.append(sel);
            ++count;
        }
        m_ui->statusbar->showMessage(QString("%1 match(es)").arg(count), 0);
    } else {
        m_ui->statusbar->clearMessage();
    }
    m_ui->txtDumpsysCmdResult->setExtraSelections(extras);
}

void UiManager::onDumpsysSearchChanged()
{
    const QString needle = m_ui->txtDumpsysSearch->text();
    applyDumpsysHighlights(needle);
    if (needle.isEmpty()) return;
    QTextCursor c = m_ui->txtDumpsysCmdResult->textCursor();
    c.movePosition(QTextCursor::Start);
    m_ui->txtDumpsysCmdResult->setTextCursor(c);
    m_ui->txtDumpsysCmdResult->find(needle, QTextDocument::FindFlags());
}

void UiManager::onDumpsysSearchNext()
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

void UiManager::onDumpsysSearchPrev()
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

void UiManager::onRunDumpsysCmdClicked()
{
    const QString cmdText = m_ui->txtDumpsysCommand->text().trimmed();
    if (cmdText.isEmpty()) {
        m_ui->statusbar->showMessage("No command specified", 3000);
        return;
    }
    m_ui->txtDumpsysResult->setPlainText("…");
    AdbManager::instance().runRawAdbCommand(cmdText);
}

void UiManager::onRawAdbCommandFinished(const QString &output)
{
    m_ui->txtDumpsysResult->setPlainText(output);
    m_ui->statusbar->showMessage(
        QString("Command: %1 lines").arg(output.count('\n')), 3000);
}

void UiManager::onDumpsysListFetched(const QStringList &services)
{
    m_dumpsysServices = services;

    QCompleter *completer = m_ui->txtDumpsysService->completer();
    if (completer)
        qobject_cast<QStringListModel *>(completer->model())->setStringList(services);

    m_ui->txtDumpsysService->setPlaceholderText(tr("Service name..."));
    m_ui->statusbar->showMessage(
        QString("Dumpsys: %1 services available").arg(services.size()), 3000);

    if (m_ui->txtDumpsysService->text().trimmed().isEmpty() && !services.isEmpty())
        m_ui->txtDumpsysService->setText(services.first());

    onRunDumpsysClicked();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Cradle Manager Tab
// ─────────────────────────────────────────────────────────────────────────────

// Helper: run a cradle command and update the "last command" label
static void runCradle(const QString &deviceId, const QStringList &args,
                      QLabel *lblLastCmd, QWidget *pendingWidget = nullptr)
{
    lblLastCmd->setText("adb shell cmd cradle_manager " + args.join(' '));
    if (pendingWidget) pendingWidget->setEnabled(false);
    AdbManager::instance().runCradleCommand(deviceId, args);
}

void UiManager::onCradleGetInfo()
{
    if (m_currentDeviceId.isEmpty()) { m_ui->statusbar->showMessage("No device selected", 3000); return; }
    QStringList args = { "get" };
    const QString key = m_ui->txtCradleKey->text().trimmed();
    if (!key.isEmpty()) args << key;
    runCradle(m_currentDeviceId, args, m_ui->lblCradleLastCmd, m_ui->btnCradleGet);
}

void UiManager::onCradleQueryFirmware()
{
    if (m_currentDeviceId.isEmpty()) { m_ui->statusbar->showMessage("No device selected", 3000); return; }
    QStringList args = { "query-firmware" };
    if (m_ui->radioDefaultFirmware->isChecked()) {
        args << "--default-firmware";
    } else {
        const QString path = m_ui->txtCradleFwPath->text().trimmed();
        if (path.isEmpty()) { m_ui->statusbar->showMessage("Please enter a firmware file path", 3000); return; }
        args << "--path" << path;
    }
    runCradle(m_currentDeviceId, args, m_ui->lblCradleLastCmd, m_ui->btnCradleQueryFirmware);
}

void UiManager::onCradleUpdateFirmware()
{
    if (m_currentDeviceId.isEmpty()) { m_ui->statusbar->showMessage("No device selected", 3000); return; }
    QStringList args = { "update-firmware" };
    if (m_ui->radioDefaultFirmware->isChecked()) {
        args << "--default-firmware";
    } else {
        const QString path = m_ui->txtCradleFwPath->text().trimmed();
        if (path.isEmpty()) { m_ui->statusbar->showMessage("Please enter a firmware file path", 3000); return; }
        args << "--path" << path;
    }
    QStringList types;
    if (m_ui->chkFwTypeApplication->isChecked()) types << "Application";
    if (m_ui->chkFwTypeBootloader->isChecked())  types << "Bootloader";
    if (m_ui->chkFwTypePreloader->isChecked())   types << "Preloader";
    if (m_ui->chkFwTypeWlc->isChecked())         types << "WLC";
    if (!types.isEmpty()) args << "--type" << types;
    runCradle(m_currentDeviceId, args, m_ui->lblCradleLastCmd, m_ui->btnCradleUpdateFirmware);
}

void UiManager::onCradleQuerySchedule()
{
    if (m_currentDeviceId.isEmpty()) { m_ui->statusbar->showMessage("No device selected", 3000); return; }
    QStringList days;
    if (m_ui->chkCradleMon->isChecked()) days << "1";
    if (m_ui->chkCradleTue->isChecked()) days << "2";
    if (m_ui->chkCradleWed->isChecked()) days << "3";
    if (m_ui->chkCradleThu->isChecked()) days << "4";
    if (m_ui->chkCradleFri->isChecked()) days << "5";
    if (m_ui->chkCradleSat->isChecked()) days << "6";
    if (m_ui->chkCradleSun->isChecked()) days << "7";
    if (days.isEmpty()) { m_ui->statusbar->showMessage("Please select at least one day", 3000); return; }
    QStringList args = { "query-schedule", "-d" };
    args << days;
    runCradle(m_currentDeviceId, args, m_ui->lblCradleLastCmd, m_ui->btnCradleQuerySchedule);
}

void UiManager::onCradleCommandFinished(const QString &output, const QString &error)
{
    for (QPushButton *btn : {m_ui->btnCradleGet, m_ui->btnCradleQueryFirmware,
                             m_ui->btnCradleUpdateFirmware, m_ui->btnCradleQuerySchedule})
        btn->setEnabled(true);

    if (!error.isEmpty()) {
        m_ui->txtCradleOutput->appendPlainText("[ERROR]\n" + error);
        if (!output.isEmpty())
            m_ui->txtCradleOutput->appendPlainText("[OUTPUT]\n" + output);
    } else {
        m_ui->txtCradleOutput->appendPlainText(output.isEmpty() ? "(no output)" : output);
    }

    QTextCursor c = m_ui->txtCradleOutput->textCursor();
    c.movePosition(QTextCursor::End);
    m_ui->txtCradleOutput->setTextCursor(c);
    m_ui->statusbar->showMessage("Cradle command completed", 2000);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Terminal
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onToggleTerminal(bool checked)
{
    if (checked) {
        m_ui->terminalContainer->show();
        if (!m_savedSplitterSizes.isEmpty()) {
            m_ui->splitterMain->setSizes(m_savedSplitterSizes);
        } else {
            int total = m_ui->splitterMain->height();
            int term  = total / 3;
            m_ui->splitterMain->setSizes(QList<int>() << (total - term) << term);
        }
        m_terminal->setFocus();
        if (m_terminal->getShellPID() <= 0)
            m_terminal->startShellProgram();
    } else {
        m_savedSplitterSizes = m_ui->splitterMain->sizes();
        m_ui->terminalContainer->hide();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Table Interaction
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onTableContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_ui->tableLog->indexAt(pos);
    if (!index.isValid()) return;

    const int column = index.column();
    const QString value = m_logModel->data(index, Qt::DisplayRole).toString();
    QString filterType, displayName;

    using namespace TableConfig::LogColumns;
    switch (column) {
    case PID:     filterType = "pid";     displayName = "PID";     break;
    case TID:     filterType = "tid";     displayName = "TID";     break;
    case PACKAGE: filterType = "package"; displayName = "Package"; break;
    case TAG:     filterType = "tag";     displayName = "Tag";     break;
    default:      return;
    }

    QMenu menu(m_mainWindow);
    menu.setStyleSheet(
        "QMenu { background-color: #2d2d30; color: #cccccc; border: 1px solid #3e3e42; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #0e639c; }");

    QAction *addOrAction  = menu.addAction(
        QString("Add '%1' to %2 filter (OR)").arg(value, displayName));
    QAction *addAndAction = menu.addAction(
        QString("Add '%1' to %2 filter (AND)").arg(value, displayName));

    const QAction *chosen = menu.exec(m_ui->tableLog->viewport()->mapToGlobal(pos));
    if      (chosen == addOrAction)  addToFilter(filterType, value, FilterOperator::OR);
    else if (chosen == addAndAction) addToFilter(filterType, value, FilterOperator::AND);
}

void UiManager::addToFilter(const QString &filterType, const QString &value, FilterOperator op)
{
    QLineEdit *filterField = nullptr;
    if      (filterType == "tag")     filterField = m_ui->txtTagFilter;
    else if (filterType == "package") filterField = m_ui->txtPackageFilter;
    else if (filterType == "pid")     filterField = m_ui->txtPidFilter;
    else if (filterType == "tid")     filterField = m_ui->txtPidFilter; // no separate TID field
    if (!filterField) return;

    const QString current   = filterField->text().trimmed();
    const QString separator = (op == FilterOperator::OR) ? "||" : "&&";

    if (current.isEmpty()) {
        filterField->setText(value);
    } else {
        // Avoid duplicates
        QStringList parts;
        if (current.contains("&&"))      parts = current.split("&&");
        else if (current.contains("||")) parts = current.split("||");
        else                             parts = current.split("|");

        for (const QString &part : parts) {
            if (part.trimmed() == value) {
                m_ui->statusbar->showMessage(
                    QString("'%1' is already in the filter").arg(value), 2000);
                return;
            }
        }
        filterField->setText(current + separator + value);
    }
    m_ui->statusbar->showMessage(
        QString("Added '%1' to filter (%2)").arg(value, op == FilterOperator::OR ? "OR" : "AND"),
        2000);
}

void UiManager::onLogTableDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || index.row() >= filteredLogs.size()) return;

    const LogEntry &entry = filteredLogs[index.row()];
    const int allLogsIndex = findLogInAllLogs(entry);
    if (allLogsIndex < 0) return;

    if (m_markLogModel->isMarked(allLogsIndex)) {
        m_markedRows.remove(index.row());
        m_markLogModel->removeMarkedLog(allLogsIndex);
    } else {
        m_markedRows.insert(index.row());
        m_markLogModel->addMarkedLog(entry, allLogsIndex);
    }
    m_logModel->setMarkedRows(&m_markedRows);
}

void UiManager::showCellContent(QTableView *tableView,
                                 const QAbstractItemModel *model,
                                 const QModelIndex &index)
{
    if (!m_ui->btnToggleCellContent->isChecked()) return;

    m_ui->txtCellContent->setPlainText(
        model->data(index, Qt::DisplayRole).toString());

    QTextCursor cursor = m_ui->txtCellContent->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_ui->txtCellContent->setTextCursor(cursor);

    tableView->scrollTo(index, QAbstractItemView::EnsureVisible);

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

void UiManager::onLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) { m_ui->txtCellContent->clear(); return; }
    showCellContent(m_ui->tableLog, m_logModel, index);
}

void UiManager::onMarkLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) { m_ui->txtCellContent->clear(); return; }
    showCellContent(m_ui->tableMarkLog, m_markLogModel, index);

    const int allLogsIndex = m_markLogModel->getOriginalIndex(index.row());
    if (allLogsIndex < 0 || allLogsIndex >= allLogs.size()) return;

    int filteredRow = findLogInFilteredLogs(allLogsIndex);
    if (filteredRow < 0) {
        filteredRow = findNearestVisibleLog(allLogsIndex);
        if (filteredRow < 0) {
            m_ui->statusbar->showMessage("No visible logs found with current filters", 3000);
            return;
        }
        m_pendingCenterRow = filteredRow;
        m_ui->tableLog->selectRow(filteredRow);
        m_rowResizeTimer->start();
        m_ui->statusbar->showMessage(
            "Marked log is filtered out - scrolled to nearest visible log", 3000);
        return;
    }

    m_pendingCenterRow = filteredRow;
    m_ui->tableLog->selectRow(filteredRow);
    m_rowResizeTimer->start();
}

void UiManager::onClearAllMarkedLog()
{
    m_markedRows.clear();
    m_markLogModel->clear();
    m_logModel->setMarkedRows(&m_markedRows);
}

void UiManager::onMarkLogContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_ui->tableMarkLog->indexAt(pos);
    if (!idx.isValid()) return;

    const int clickedRow     = idx.row();
    const bool alreadyAnchor = (clickedRow == m_markLogModel->anchorRow());

    QMenu menu(m_mainWindow);
    menu.setStyleSheet(
        "QMenu { background-color: #2d2d30; color: #cccccc; border: 1px solid #3e3e42; }"
        "QMenu::item:selected { background-color: #094771; }"
        "QMenu::item:disabled { color: #666666; }");

    QAction *setAnchorAction = menu.addAction(
        alreadyAnchor ? tr("\u25b6  Start time (already set)")
                      : tr("\u25b6  Set as start time (\u0394T\u00a0=\u00a00)"));
    setAnchorAction->setEnabled(!alreadyAnchor);
    menu.addSeparator();
    QAction *unmarkAction = menu.addAction(tr("\u2715  Unmark this row"));

    const QAction *chosen = menu.exec(m_ui->tableMarkLog->viewport()->mapToGlobal(pos));
    if (chosen == setAnchorAction && !alreadyAnchor) {
        m_markLogModel->setAnchorRow(clickedRow);
    } else if (chosen == unmarkAction) {
        const int allLogsIndex = m_markLogModel->getOriginalIndex(clickedRow);
        if (allLogsIndex >= 0) {
            m_markedRows.remove(findLogInFilteredLogs(allLogsIndex));
            m_markLogModel->removeMarkedLog(allLogsIndex);
            m_logModel->setMarkedRows(&m_markedRows);
        }
    }
}

void UiManager::copyTableRows(QTableView *tableView)
{
    QAbstractItemModel *model = tableView->model();
    if (!model) return;

    QItemSelectionModel *selModel = tableView->selectionModel();

    // Try full-row selection first (log table uses SelectRows)
    QModelIndexList selectedRows = selModel->selectedRows();
    if (!selectedRows.isEmpty()) {
        std::sort(selectedRows.begin(), selectedRows.end(),
                  [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });

        QList<int> visibleCols;
        for (int c = 0; c < model->columnCount(); ++c)
            if (!tableView->isColumnHidden(c)) visibleCols.append(c);

        QStringList lines;
        for (const QModelIndex &rowIdx : selectedRows) {
            QStringList cells;
            for (int col : visibleCols)
                cells << model->data(model->index(rowIdx.row(), col), Qt::DisplayRole).toString();
            lines << cells.join(QStringLiteral("\t"));
        }
        QApplication::clipboard()->setText(lines.join(QStringLiteral("\n")));
        m_ui->statusbar->showMessage(
            QString("Copied %1 row(s) to clipboard").arg(selectedRows.size()), 2000);
        return;
    }

    // Cell-level selection (settings / properties / property-definitions tables)
    QModelIndexList selected = selModel->selectedIndexes();
    if (selected.isEmpty()) {
        QModelIndex current = tableView->currentIndex();
        if (!current.isValid()) return;
        selected.append(current);
    }

    std::sort(selected.begin(), selected.end(),
              [](const QModelIndex &a, const QModelIndex &b) {
                  return a.row() != b.row() ? a.row() < b.row() : a.column() < b.column();
              });

    // Group by row, emit tab-separated cells per row
    QStringList lines;
    int prevRow = -1;
    QStringList rowCells;
    for (const QModelIndex &idx : selected) {
        if (idx.row() != prevRow) {
            if (!rowCells.isEmpty())
                lines << rowCells.join(QStringLiteral("\t"));
            rowCells.clear();
            prevRow = idx.row();
        }
        rowCells << model->data(idx, Qt::DisplayRole).toString();
    }
    if (!rowCells.isEmpty())
        lines << rowCells.join(QStringLiteral("\t"));

    QApplication::clipboard()->setText(lines.join(QStringLiteral("\n")));
    m_ui->statusbar->showMessage(
        QString("Copied %1 cell(s) to clipboard").arg(selected.size()), 2000);
}

void UiManager::enableTableCopyAction(QTableView *tableView)
{
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QAction *copyAction = new QAction(tableView);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(copyAction, &QAction::triggered, this, [this, tableView]() {
        copyTableRows(tableView);
    });
    tableView->addAction(copyAction);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: App Settings & Column Visibility
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onColumnsClicked()
{
    QDialog dialog(m_mainWindow);
    dialog.setWindowTitle("Column Visibility");
    dialog.setMinimumWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *titleLabel = new QLabel("Select columns to display:", &dialog);
    titleLabel->setStyleSheet("font-weight: bold; margin-bottom: 10px;");
    layout->addWidget(titleLabel);

    const QStringList columnNames = {"Date", "Time", "PID", "TID", "Package", "Lvl", "Tag", "Message"};
    QVector<QCheckBox *> checkboxes;
    for (int i = 0; i < columnNames.size(); ++i) {
        QCheckBox *cb = new QCheckBox(columnNames[i], &dialog);
        cb->setChecked(!m_ui->tableLog->isColumnHidden(i));
        checkboxes.append(cb);
        layout->addWidget(cb);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.setStyleSheet(
        "QDialog { background-color: #2d2d30; color: #cccccc; }"
        "QLabel { color: #cccccc; }"
        "QCheckBox { color: #cccccc; padding: 5px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator::unchecked { border: 1px solid #3e3e42; background-color: #1e1e1e; }"
        "QCheckBox::indicator::checked { border: 1px solid #007acc; background-color: #007acc; }"
        "QPushButton { background-color: #0e639c; border: none; border-radius: 3px; padding: 6px 12px; color: white; }"
        "QPushButton:hover { background-color: #1177bb; }");

    if (dialog.exec() == QDialog::Accepted) {
        for (int i = 0; i < checkboxes.size(); ++i) {
            const bool hidden = !checkboxes[i]->isChecked();
            m_ui->tableLog->setColumnHidden(i, hidden);
            m_ui->tableMarkLog->setColumnHidden(i, hidden);
        }
    }
}

void UiManager::onAutoScrollToggled(bool checked)
{
    if (checked) m_ui->tableLog->scrollToBottom();
}

void UiManager::onAppSettingsClicked()
{
    QVector<bool> colVis;
    for (int c = 0; c < TableConfig::LogColumns::TOTAL_COLUMNS; ++c)
        colVis.append(!m_ui->tableLog->isColumnHidden(c));

    QVector<bool> propDefColVis;
    for (int c = 0; c < TableConfig::PropertyDefColumns::TOTAL_COLUMNS; ++c)
        propDefColVis.append(!m_ui->tablePropertyDefinitions->isColumnHidden(c));

    SettingsDialog dlg(QApplication::font(), colVis, propDefColVis, m_mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        applyAppFont(dlg.selectedFont());
        applyColumnVisibility(dlg.columnVisibility());
        applyPropDefColumnVisibility(dlg.propDefColumnVisibility());

        const QStringList keys = dlg.keysToReset();
        if (!keys.isEmpty())
            m_filterHistoryManager->clearKeys(keys);
    }
}

void UiManager::applyColumnVisibility(const QVector<bool> &vis)
{
    using namespace TableConfig::LogColumns;
    for (int c = 0; c < vis.size(); ++c) {
        m_ui->tableLog->setColumnHidden(c, !vis[c]);
        m_ui->tableMarkLog->setColumnHidden(c, !vis[c]);
    }
    if (vis.size() > PACKAGE) m_ui->groupBox_2->setVisible(vis[PACKAGE]);
    if (vis.size() > PID)     m_ui->groupBox_3->setVisible(vis[PID]);
    if (vis.size() > TIME)    m_ui->groupBox_6->setVisible(vis[TIME]);
    if (vis.size() > TAG)     m_ui->groupBox->setVisible(vis[TAG]);
    if (vis.size() > MESSAGE) m_ui->groupBox_5->setVisible(vis[MESSAGE]);
}

void UiManager::applyPropDefColumnVisibility(const QVector<bool> &vis)
{
    using namespace TableConfig::PropertyDefColumns;
    for (int c = 0; c < vis.size() && c < TOTAL_COLUMNS; ++c)
        m_ui->tablePropertyDefinitions->setColumnHidden(c, !vis[c]);
}

void UiManager::applyAppFont(const QFont &font)
{
    QApplication::setFont(font);
    for (QWidget *w : QApplication::topLevelWidgets()) {
        w->setFont(font);
        w->setStyleSheet(w->styleSheet());
    }
    if (m_terminal) {
        QFont termFont(QStringLiteral("Monospace"),
                       font.pointSize() > 0 ? font.pointSize() : 10);
        m_terminal->setTerminalFont(termFont);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Status Bar & Performance
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::updateMemoryUsage()
{
    QFile file("/proc/self/status");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith("VmRSS:")) {
            const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2)
                memoryUsage = parts[1].toLongLong() / 1024;
            break;
        }
    }
}

void UiManager::updateStatusBar()
{
    updateMemoryUsage();
    m_ui->statusbar->showMessage(
        QString("UTF-8  Lines: %1    Mem: %2MB  \u25cf %3")
            .arg(filteredLogs.size())
            .arg(memoryUsage)
            .arg(isPaused ? "Paused" : "Running"));
}

void UiManager::resizeVisibleRows()
{
    auto *view     = m_ui->tableLog;
    auto *vp       = view->viewport();
    const int rowCount = m_logModel->rowCount();
    if (rowCount == 0 || !vp) return;

    if (rowCount <= ROW_RESIZE_THRESHOLD) {
        view->resizeRowsToContents();
    } else {
        const int first = view->rowAt(0);
        if (first < 0) return;
        int last = view->rowAt(vp->height() - 1);
        if (last < 0 || last >= rowCount) last = rowCount - 1;
        for (int r = first; r <= last; ++r)
            view->resizeRowToContents(r);
        if (m_pendingCenterRow >= 0 &&
            (m_pendingCenterRow < first || m_pendingCenterRow > last))
            view->resizeRowToContents(m_pendingCenterRow);
    }

    if (m_pendingCenterRow >= 0) {
        view->scrollTo(m_logModel->index(m_pendingCenterRow, 0),
                       QAbstractItemView::PositionAtCenter);
        m_pendingCenterRow = -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Log Navigation Helpers
// ─────────────────────────────────────────────────────────────────────────────

int UiManager::findLogInAllLogs(const LogEntry &entry) const
{
    return m_allLogsIndex.value(entry.id, -1);
}

int UiManager::findLogInFilteredLogs(int allLogsIndex) const
{
    if (allLogsIndex < 0 || allLogsIndex >= allLogs.size()) return -1;
    return m_filteredLogsIndex.value(allLogs[allLogsIndex].id, -1);
}

int UiManager::findNearestVisibleLog(int allLogsIndex) const
{
    if (allLogsIndex < 0 || allLogsIndex >= allLogs.size()) return -1;
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

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Event Filter Helpers (delegated from MainWindow::eventFilter)
// ─────────────────────────────────────────────────────────────────────────────

bool UiManager::handleEvent(QObject *obj, QEvent *event)
{
    QWidget *w = qobject_cast<QWidget*>(obj);

    // ── Device row: click to select ───────────────────────────────────────────
    if (event->type() == QEvent::MouseButtonRelease) {
        if (w && m_deviceRowMap.contains(w)) {
            selectDeviceRow(w, m_deviceRowMap[w]);
            return false;
        }
        // ── Selected device row in right panel: click to show details ─────────
        if (w && w->objectName().startsWith(QStringLiteral("devSelectedRow_"))) {
            const QString serial = w->objectName().mid(
                static_cast<int>(QString("devSelectedRow_").length()));

            // Highlight the clicked row, reset others
            QVBoxLayout *listLayout = m_ui->devDeviceListVLayout;
            for (int i = 0; i < listLayout->count(); ++i) {
                if (QWidget *row = listLayout->itemAt(i)->widget()) {
                    if (row == w)
                        row->setStyleSheet("background-color: #2a2d32; border-bottom: 1px solid #3e3e42;");
                    else
                        row->setStyleSheet("background: transparent; border-bottom: 1px solid #3e3e42;");
                }
            }

            // Find the device in connected list and show its details
            DevicesManager &dm = DevicesManager::instance();
            const QList<AdbDevice> connected = dm.connectedDevices();
            for (const AdbDevice &d : connected) {
                if (d.id == serial) {
                    DeviceInfo info;
                    info.serial = d.id;
                    info.name   = d.name;
                    info.online = d.isOnline;
                    updateDeviceDetails(info);
                    // Also highlight corresponding sidebar row
                    for (auto it = m_deviceRowMap.constBegin(); it != m_deviceRowMap.constEnd(); ++it) {
                        if (it.value().serial == serial) {
                            selectDeviceRow(it.key(), it.value());
                            break;
                        }
                    }
                    break;
                }
            }
            return false;
        }
    }

    if (handleTerminalKeyEvent(obj, event))     return true;
    if (event->type() == QEvent::Wheel)
        if (handleShiftScrollEvent(obj, static_cast<QWheelEvent*>(event))) return true;
    handleCompleterFocusEvent(obj, event);
    return false;
}

bool UiManager::handleTerminalKeyEvent(QObject *obj, QEvent *event)
{
    if (!m_terminal || event->type() != QEvent::KeyPress) return false;
    QWidget *w = qobject_cast<QWidget*>(obj);
    if (!w || w->parent() != m_terminal) return false;

    QKeyEvent *ke = static_cast<QKeyEvent*>(event);
    if (ke->modifiers() != Qt::ControlModifier) return false;

    if (ke->key() == Qt::Key_C) {
        if (!m_terminal->selectedText(true).isEmpty())
            m_terminal->copyClipboard();
        else
            m_terminal->sendText(QStringLiteral("\x03"));
        return true;
    }
    if (ke->key() == Qt::Key_V) {
        m_terminal->pasteClipboard();
        return true;
    }
    return false;
}

bool UiManager::handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent)
{
    if (!(wheelEvent->modifiers() & Qt::ShiftModifier)) return false;

    QTableView *tableView = nullptr;
    if      (obj == m_ui->tableLog->viewport())     tableView = m_ui->tableLog;
    else if (obj == m_ui->tableMarkLog->viewport()) tableView = m_ui->tableMarkLog;
    if (!tableView) return false;

    QScrollBar *hBar = tableView->horizontalScrollBar();
    if (!hBar) return false;

    const int steps = wheelEvent->angleDelta().y() / 120;
    hBar->setValue(hBar->value() - steps * hBar->singleStep());
    return true;
}

bool UiManager::handleCompleterFocusEvent(QObject *obj, QEvent *event)
{
    if (obj == m_ui->txtPropertySearch && event->type() == QEvent::FocusIn) {
        if (m_ui->txtPropertySearch->text().isEmpty()) {
            QCompleter *c = m_ui->txtPropertySearch->completer();
            if (c && c->model() && c->model()->rowCount() > 0) {
                c->setCompletionPrefix("");
                c->complete();
            }
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Property Set — Save / Load / Export / Import
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::recreatePropertyDefinitionButtons()
{
    using namespace TableConfig::PropertyDefColumns;

    static const QString iconBtnStyle =
        "QPushButton { padding: 0px; border: none; background: transparent; border-radius: 4px; }"
        "QPushButton:hover { background-color: rgba(255,255,255,40); }"
        "QPushButton:pressed { background-color: rgba(255,255,255,70); }";

    auto makeCenteredBtn = [&](const QString &icon, const QString &tooltip)
        -> std::pair<QWidget*, QPushButton*>
    {
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

    const int rowCount = m_propertyDefinitionModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        auto [cellSet, btnSet] = makeCenteredBtn(":/icons/download.svg", "Set property value");
        m_ui->tablePropertyDefinitions->setIndexWidget(
            m_propertyDefinitionModel->index(row, SET_BUTTON), cellSet);
        connect(btnSet, &QPushButton::clicked, this, [this, row]() {
            onSetPropertyDefinitionClicked(row);
        });

        auto [cellGet, btnGet] = makeCenteredBtn(":/icons/refresh.svg", "Get property value");
        m_ui->tablePropertyDefinitions->setIndexWidget(
            m_propertyDefinitionModel->index(row, GET_BUTTON), cellGet);
        connect(btnGet, &QPushButton::clicked, this, [this, row]() {
            onGetPropertyDefinitionClicked(row);
        });

        auto [cellRemove, btnRemove] = makeCenteredBtn(":/icons/edit-delete.svg", "Remove property from list");
        m_ui->tablePropertyDefinitions->setIndexWidget(
            m_propertyDefinitionModel->index(row, REMOVE_BUTTON), cellRemove);
        connect(btnRemove, &QPushButton::clicked, this, [this, row]() {
            onRemovePropertyDefinitionClicked(row);
        });
    }
}

void UiManager::onSavePropertySet()
{
    const QVector<PropertyDefinition> &currentDefs =
        m_propertyDefinitionModel->getPropertyDefinitions();

    if (currentDefs.isEmpty()) {
        QMessageBox::information(m_mainWindow,
                                 tr("Save Property Set"),
                                 tr("There are no property definitions to save."));
        return;
    }

    bool ok = false;
    const QString setName = QInputDialog::getText(
        m_mainWindow,
        tr("Save Property Set"),
        tr("Enter a name for this property set:"),
        QLineEdit::Normal,
        QString(),
        &ok).trimmed();

    if (!ok || setName.isEmpty())
        return;

    QString errorMsg;
    if (!m_propDefBackend->savePropertySet(setName, currentDefs, errorMsg)) {
        QMessageBox::critical(m_mainWindow,
                              tr("Save Failed"),
                              tr("Could not save property set:\n%1").arg(errorMsg));
        return;
    }

    QMessageBox::information(m_mainWindow,
                             tr("Save Property Set"),
                             tr("Property set \"%1\" saved successfully.").arg(setName));
}

void UiManager::onLoadPropertySet()
{
    const QStringList names = m_propDefBackend->listPropertySetNames();
    if (names.isEmpty()) {
        QMessageBox::information(m_mainWindow,
                                 tr("Load Property Set"),
                                 tr("No saved property sets found."));
        return;
    }

    // Build a dialog with a list of saved set names
    QDialog dialog(m_mainWindow);
    dialog.setWindowTitle(tr("Load Property Set"));
    dialog.setMinimumSize(360, 300);

    auto *layout   = new QVBoxLayout(&dialog);
    auto *label    = new QLabel(tr("Select a property set to load:"), &dialog);
    auto *listWidget = new QListWidget(&dialog);
    listWidget->addItems(names);
    listWidget->setCurrentRow(0);

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    // Delete button alongside OK/Cancel
    auto *btnDelete = new QPushButton(tr("Delete"), &dialog);
    btnBox->addButton(btnDelete, QDialogButtonBox::ResetRole);

    layout->addWidget(label);
    layout->addWidget(listWidget);
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(btnDelete, &QPushButton::clicked, &dialog, [&]() {
        QListWidgetItem *item = listWidget->currentItem();
        if (!item)
            return;
        const QString name = item->text();
        const auto reply = QMessageBox::question(
            &dialog,
            tr("Delete Property Set"),
            tr("Delete \"%1\"?").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
        QString err;
        if (m_propDefBackend->deletePropertySet(name, err))
            delete listWidget->takeItem(listWidget->row(item));
        else
            QMessageBox::critical(&dialog, tr("Delete Failed"), err);
    });

    // Double-click to accept
    connect(listWidget, &QListWidget::doubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QListWidgetItem *selected = listWidget->currentItem();
    if (!selected)
        return;

    const QString setName = selected->text();
    QString errorMsg;
    const QVector<PropertyDefinition> defs =
        m_propDefBackend->loadPropertySet(setName, errorMsg);

    if (!errorMsg.isEmpty()) {
        QMessageBox::critical(m_mainWindow,
                              tr("Load Failed"),
                              tr("Could not load property set:\n%1").arg(errorMsg));
        return;
    }

    m_propertyDefinitionModel->setPropertyDefinitions(defs);
    recreatePropertyDefinitionButtons();
    updatePropertyNamesCompleter();

    // Auto-fetch current values from the connected device
    if (!m_currentDeviceId.isEmpty()) {
        m_ui->statusbar->showMessage(tr("Loading current property values from device..."), 0);
        AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
    }
}

void UiManager::onExportPropertySet()
{
    const QVector<PropertyDefinition> &currentDefs =
        m_propertyDefinitionModel->getPropertyDefinitions();

    if (currentDefs.isEmpty()) {
        QMessageBox::information(m_mainWindow,
                                 tr("Export Property Set"),
                                 tr("There are no property definitions to export."));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        m_mainWindow,
        tr("Export Property Definitions"),
        QDir::homePath() + QStringLiteral("/property_definitions.json"),
        tr("JSON Files (*.json);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    QString errorMsg;
    if (!m_propDefBackend->exportToFile(filePath, currentDefs, errorMsg)) {
        QMessageBox::critical(m_mainWindow,
                              tr("Export Failed"),
                              tr("Could not export property definitions:\n%1").arg(errorMsg));
        return;
    }

    QMessageBox::information(m_mainWindow,
                             tr("Export Property Set"),
                             tr("Exported %1 property definition(s) to:\n%2")
                                 .arg(currentDefs.size())
                                 .arg(filePath));
}

void UiManager::onImportPropertySet()
{
    const QString filePath = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Import Property Definitions"),
        QDir::homePath(),
        tr("JSON Files (*.json);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    QVector<PropertyDefinition> imported;
    QString errorMsg;
    if (!m_propDefBackend->importFromFile(filePath, imported, errorMsg)) {
        QMessageBox::critical(m_mainWindow,
                              tr("Import Failed"),
                              tr("Could not import property definitions:\n%1").arg(errorMsg));
        return;
    }

    // Ask whether to replace or append
    const int existing = m_propertyDefinitionModel->rowCount();
    QMessageBox::StandardButton reply = QMessageBox::NoButton;
    if (existing > 0) {
        reply = QMessageBox::question(
            m_mainWindow,
            tr("Import Property Set"),
            tr("Replace the %1 existing definition(s) with the imported ones, "
               "or append them?")
                .arg(existing),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);

        if (reply == QMessageBox::Cancel)
            return;
    }

    if (reply == QMessageBox::Yes || existing == 0) {
        m_propertyDefinitionModel->setPropertyDefinitions(imported);
    } else {
        for (const PropertyDefinition &def : imported)
            m_propertyDefinitionModel->addPropertyDefinition(def);
    }

    recreatePropertyDefinitionButtons();
    updatePropertyNamesCompleter();

    // Auto-fetch current values from the connected device
    if (!m_currentDeviceId.isEmpty()) {
        m_ui->statusbar->showMessage(tr("Loading current property values from device..."), 0);
        AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
    }

    QMessageBox::information(m_mainWindow,
                             tr("Import Property Set"),
                             tr("Imported %1 property definition(s).").arg(imported.size()));
}
