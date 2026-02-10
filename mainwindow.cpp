#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "threadtimelogconverter.h"
#include "brieflogconverter.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include <QClipboard>
#include <QApplication>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QRegularExpression>
#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QKeyEvent>
#include <QLineEdit>
#include <QBrush>
#include <QColor>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QCompleter>
#include <QStringListModel>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_logModel(new LogModel(this))
    , m_markLogModel(new MarkLogModel(this))
    , m_settingsModel(new SettingsModel(this))
    , m_propertiesModel(new PropertiesModel(this))
    , m_propertyDefinitionModel(new PropertyDefinitionModel(this))
    , m_currentDeviceId("")
    , isPaused(true)
    , memoryUsage(42)
    , m_logConverter(new ThreadtimeLogConverter())
{
    ui->setupUi(this);
    
    // Setup main log table view with model
    ui->tableLog->setModel(m_logModel);
    ui->tableLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableLog->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableLog->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableLog->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableLog->setAlternatingRowColors(true);
    
    // Set marked rows pointer to model for highlighting
    m_logModel->setMarkedRows(&m_markedRows);
    
    // Set column widths for main log table
    ui->tableLog->setColumnWidth(0, 100); // Date
    ui->tableLog->setColumnWidth(1, 120); // Time
    ui->tableLog->setColumnWidth(2, 60);  // PID
    ui->tableLog->setColumnWidth(3, 60);  // TID
    ui->tableLog->setColumnWidth(4, 200); // Package
    ui->tableLog->setColumnWidth(5, 35);  // Lvl
    ui->tableLog->setColumnWidth(6, 150); // Tag
    
    // Setup mark log table view with model
    ui->tableMarkLog->setModel(m_markLogModel);
    ui->tableMarkLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableMarkLog->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableMarkLog->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableMarkLog->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableMarkLog->setAlternatingRowColors(true);
    
    // Set column widths for mark log table
    ui->tableMarkLog->setColumnWidth(0, 100); // Date
    ui->tableMarkLog->setColumnWidth(1, 120); // Time
    ui->tableMarkLog->setColumnWidth(2, 60);  // PID
    ui->tableMarkLog->setColumnWidth(3, 60);  // TID
    ui->tableMarkLog->setColumnWidth(4, 200); // Package
    ui->tableMarkLog->setColumnWidth(5, 35);  // Lvl
    ui->tableMarkLog->setColumnWidth(6, 150); // Tag
    
    // Setup splitters
    ui->splitter->setSizes(QList<int>() << 250 << 1150);
    ui->splitterLogTables->setSizes(QList<int>() << 600 << 200);
    
    // Enable context menu for main log table
    ui->tableLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableLog, &QTableView::customContextMenuRequested, this, &MainWindow::onTableContextMenu);
    
    // Connect double-click for marking logs
    connect(ui->tableLog, &QTableView::doubleClicked, this, &MainWindow::onLogTableDoubleClicked);
    
    // Connect click on mark log table for scrolling to original
    connect(ui->tableMarkLog, &QTableView::clicked, this, &MainWindow::onMarkLogTableClicked);
    
    // Connect to AdbManager
    AdbManager &adbManager = AdbManager::instance();
    connect(&adbManager, &AdbManager::devicesChanged, this, &MainWindow::onDevicesChanged);
    connect(&adbManager, &AdbManager::logcatLineReceived, this, &MainWindow::onLogcatLineReceived);
    connect(&adbManager, &AdbManager::settingsFetched, this, &MainWindow::onSettingsFetched);
    connect(&adbManager, &AdbManager::propertiesFetched, this, &MainWindow::onPropertiesFetched);
    connect(&adbManager, &AdbManager::propertyDefinitionsFetched, this, &MainWindow::onPropertyDefinitionsFetched);
    
    // Initialize with current devices
    onDevicesChanged(adbManager.getConnectedDevices());
    
    setupConnections();
    
    // Install event filters for filter history navigation
    ui->txtTagFilter->installEventFilter(this);
    ui->txtPidFilter->installEventFilter(this);
    ui->txtPackageFilter->installEventFilter(this);
    ui->txtFindMessage->installEventFilter(this);
    ui->txtPropertySearch->installEventFilter(this);
    
    // Initialize tab widget - start with ADB Logcat tab
    ui->tabWidget->setCurrentIndex(0);
    
    setupConfigurationTables();
    setupSDKTab();
    applyFilters();
    updateStatusBar();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // Filter connections - apply filter only when Enter is pressed
    connect(ui->txtFindMessage, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtStartTime, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtEndTime, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtTagFilter, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtPackageFilter, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    connect(ui->txtPidFilter, &QLineEdit::returnPressed, this, &MainWindow::onFilterChanged);
    
    // Configuration filter connections
    connect(ui->txtFilterSettings, &QLineEdit::textChanged, this, &MainWindow::onSettingsFilterChanged);
    connect(ui->txtFilterProperties, &QLineEdit::textChanged, this, &MainWindow::onPropertiesFilterChanged);
    connect(ui->btnRefreshSettings, &QPushButton::clicked, this, &MainWindow::onRefreshSettingsClicked);
    connect(ui->btnRefreshProperties, &QPushButton::clicked, this, &MainWindow::onRefreshPropertiesClicked);
    
    // SDK tab connections
    connect(ui->txtPropertySearch, &QLineEdit::returnPressed, this, &MainWindow::onSearchPropertyDefinition);
    connect(ui->btnAddProperty, &QPushButton::clicked, this, &MainWindow::onAddPropertyDefinition);
    connect(ui->btnClearAllProperties, &QPushButton::clicked, this, &MainWindow::onClearAllPropertyDefinitions);
    connect(ui->btnFetchPropertyDefs, &QPushButton::clicked, this, &MainWindow::onFetchPropertyDefinitions);
    
    connect(ui->radioVerbosePlus, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioV, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioD, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioI, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioW, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioE, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    connect(ui->radioA, &QRadioButton::toggled, this, &MainWindow::onFilterChanged);
    
    // Button connections
    connect(ui->btnResume, &QPushButton::clicked, this, &MainWindow::onResumeClicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->btnCopy, &QPushButton::clicked, this, &MainWindow::onCopyClicked);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::onSaveFileClicked);
    connect(ui->btnColumns, &QPushButton::clicked, this, &MainWindow::onColumnsClicked);
    connect(ui->btnAutoScroll, &QPushButton::toggled, this, &MainWindow::onAutoScrollToggled);
    connect(ui->btnSettings, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(ui->cmbDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDeviceChanged);
    
    // File path input - load file when Enter is pressed
    connect(ui->txtFilePath, &QLineEdit::returnPressed, this, &MainWindow::onLoadFileClicked);
    connect(ui->btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
}

void MainWindow::addSampleLogs()
{
    // Add sample log entries based on the screenshot
    allLogs.append({"2026-02-10", "06:58:01.159", "4523", "3898", "com.android.systemui", "V", "CameraService", "Request requires android.permission.MANAGE_USERS"});
    allLogs.append({"2026-02-10", "06:58:10.759", "1224", "1954", "com.android.phone", "I", "CameraService", "Displayed com.android.settings/.Settings: +350ms"});
    allLogs.append({"2026-02-10", "06:58:18.759", "5955", "4469", "com.android.systemui", "I", "CameraService", "Displayed com.android.settings/.Settings: +350ms"});
    allLogs.append({"2026-02-10", "06:58:23.559", "5410", "5334", "com.instagram.android", "D", "CameraService", "Attempting to reconnect to WiFi..."});
    allLogs.append({"2026-02-10", "06:58:28.359", "2642", "4272", "com.instagram.android", "I", "CameraService", "killing 1293:com.google.android.gms/u0a12 (adj 900): empty #17 [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "06:58:37.159", "3462", "4051", "com.instagram.android", "D", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] } [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "06:58:49.959", "5034", "1688", "com.example.app", "D", "CameraService", "killing 1293:com.google.android.gms/u0a12 (adj 900): empty #17"});
    allLogs.append({"2026-02-10", "06:59:09.159", "4301", "3805", "com.spotify.music", "I", "CameraService", "Displayed com.android.settings/.Settings: +350ms"});
    allLogs.append({"2026-02-10", "06:59:21.959", "3781", "5915", "com.google.android.gms", "I", "CameraService", "Attempting to reconnect to WiFi..."});
    allLogs.append({"2026-02-10", "06:59:53.959", "5861", "2692", "com.google.android.gms", "I", "CameraService", "killing 1293:com.google.android.gms/u0a12 (adj 900): empty #17 [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "06:59:54.359", "5017", "1340", "com.android.phone", "I", "CameraService", "Connected to process 14502 [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "06:59:41.159", "1400", "2642", "com.example.app", "D", "CameraService", "Connected to process 14502 [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "06:59:42.759", "1624", "3436", "com.spotify.music", "D", "CameraService", "Heap transition to process 4520 [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "06:59:53.959", "5861", "2692", "com.google.android.gms", "I", "CameraService", "Attempting to reconnect to WiFi..."});
    allLogs.append({"2026-02-10", "06:59:54.359", "5017", "1340", "com.android.phone", "I", "CameraService", "Connected to process 14502 [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "07:00:04.759", "4414", "2872", "com.spotify.music", "V", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] }"});
    allLogs.append({"2026-02-10", "07:00:27.559", "2284", "5068", "android.process.media", "D", "CameraService", "BufferedQueueProducer: [com.example.app/MainActivity] disconnect: api=1"});
    allLogs.append({"2026-02-10", "07:00:32.359", "1134", "2855", "com.spotify.music", "I", "CameraService", "Connected to process 14502"});
    allLogs.append({"2026-02-10", "07:00:38.759", "5914", "2969", "com.android.phone", "D", "CameraService", "Connected to process 14502"});
    allLogs.append({"2026-02-10", "07:00:42.759", "4892", "1548", "com.android.systemui", "I", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] } [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "07:00:45.159", "3100", "2199", "com.whatsapp", "I", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] } [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "07:00:53.959", "5051", "5698", "android.process.media", "D", "CameraService", "BufferedQueueProducer: [com.example.app/MainActivity] disconnect: api=1 [EXTRA_DATA]"});
    allLogs.append({"2026-02-10", "07:01:06.759", "1233", "5843", "android.process.media", "I", "CameraService", "Access denied finding property \"ro.serialno\" [EXTRA_DATA]"});
    
    // Add more entries to reach 489 total
    for (int i = 0; i < 466; i++) {
        QString packages[] = {"com.android.systemui", "com.google.android.gms", "com.spotify.music", "com.whatsapp", "com.example.app"};
        QString levels[] = {"V", "D", "I", "W", "E"};
        QString tags[] = {"CameraService", "ActivityManager", "WindowManager", "PackageManager"};
        
        LogEntry entry;
        entry.date = "2026-02-10";
        entry.time = QString("07:%1:%2.%3").arg(i/60, 2, 10, QChar('0')).arg(i%60, 2, 10, QChar('0')).arg(rand()%1000, 3, 10, QChar('0'));
        entry.pid = QString::number(1000 + rand() % 9000);
        entry.tid = QString::number(1000 + rand() % 9000);
        entry.package = packages[rand() % 5];
        entry.level = levels[rand() % 5];
        entry.tag = tags[rand() % 4];
        entry.message = "Sample log message for testing " + QString::number(i);
        
        allLogs.append(entry);
    }
}

void MainWindow::setupConfigurationTables()
{
    // Setup Settings table with model
    ui->tableSettings->setModel(m_settingsModel);
    ui->tableSettings->horizontalHeader()->setStretchLastSection(false);
    ui->tableSettings->setColumnWidth(0, 50);   // LINE
    ui->tableSettings->setColumnWidth(1, 120);  // GROUP
    ui->tableSettings->setColumnWidth(2, 250);  // SETTING
    ui->tableSettings->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // VALUE - stretch to max
    ui->tableSettings->setColumnWidth(4, 40);   // Download button
    
    // Setup VALUE column delegate for editing with margin 1
    ValueDelegate *settingsDelegate = new ValueDelegate(this);
    ui->tableSettings->setItemDelegateForColumn(3, settingsDelegate);
    
    // Setup Properties table with model
    ui->tableProperties->setModel(m_propertiesModel);
    ui->tableProperties->horizontalHeader()->setStretchLastSection(false);
    ui->tableProperties->setColumnWidth(0, 50);   // LINE
    ui->tableProperties->setColumnWidth(1, 300);  // PROPERTY
    ui->tableProperties->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch); // VALUE - stretch to max
    ui->tableProperties->setColumnWidth(3, 40);   // Download button
    
    // Setup VALUE column delegate for editing with margin 1
    ValueDelegate *propertiesDelegate = new ValueDelegate(this);
    ui->tableProperties->setItemDelegateForColumn(2, propertiesDelegate);
    
    // Setup splitters for configuration tab
    ui->splitterConfig->setSizes(QList<int>() << 250 << 1150);
    ui->splitterConfigTables->setSizes(QList<int>() << 575 << 575);
}

void MainWindow::setupSDKTab()
{
    // Setup PropertyDefinition table with model
    ui->tablePropertyDefinitions->setModel(m_propertyDefinitionModel);
    ui->tablePropertyDefinitions->horizontalHeader()->setStretchLastSection(false);
    
    // Set column widths
    ui->tablePropertyDefinitions->setColumnWidth(0, 200);  // Name
    ui->tablePropertyDefinitions->setColumnWidth(1, 80);   // ID
    ui->tablePropertyDefinitions->setColumnWidth(2, 80);   // Supported
    ui->tablePropertyDefinitions->setColumnWidth(3, 80);   // Loaded
    ui->tablePropertyDefinitions->setColumnWidth(4, 100);  // Need Reboot
    ui->tablePropertyDefinitions->setColumnWidth(5, 80);   // Volatile
    ui->tablePropertyDefinitions->setColumnWidth(6, 90);   // Read Only
    ui->tablePropertyDefinitions->setColumnWidth(7, 80);   // Optional
    ui->tablePropertyDefinitions->setColumnWidth(8, 100);  // Persistence
    ui->tablePropertyDefinitions->setColumnWidth(9, 120);  // eManager
    ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Stretch); // VALUE - stretch to max
    ui->tablePropertyDefinitions->setColumnWidth(11, 50);  // Set button
    ui->tablePropertyDefinitions->setColumnWidth(12, 50);  // Get button
    ui->tablePropertyDefinitions->setColumnWidth(13, 60);  // Remove button
    
    // Setup VALUE column delegate for editing with margin 1
    ValueDelegate *valueDelegate = new ValueDelegate(this);
    ui->tablePropertyDefinitions->setItemDelegateForColumn(10, valueDelegate);
    
    // Setup completer for property search (will be populated when property definitions are fetched)
    QCompleter *completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    ui->txtPropertySearch->setCompleter(completer);
}

void MainWindow::recreateSettingsButtons()
{
    // Clear existing buttons to prevent memory leaks and crashes
    const QVector<SettingEntry> &settings = m_settingsModel->getSettings();
    
    // Remove all existing index widgets
    for (int i = 0; i < m_settingsModel->rowCount(); i++) {
        QWidget *oldWidget = ui->tableSettings->indexWidget(m_settingsModel->index(i, 4));
        if (oldWidget) {
            ui->tableSettings->setIndexWidget(m_settingsModel->index(i, 4), nullptr);
            oldWidget->deleteLater();
        }
    }
    
    // Add save buttons for each setting
    for (int i = 0; i < settings.size(); i++) {
        QPushButton *btnSave = new QPushButton("💾");
        btnSave->setMaximumSize(30, 30);
        btnSave->setStyleSheet("QPushButton { font-size: 14px; }");
        btnSave->setToolTip("Save this setting to device");
        ui->tableSettings->setIndexWidget(m_settingsModel->index(i, 4), btnSave);
        
        // Connect button to save slot
        connect(btnSave, &QPushButton::clicked, this, [this, i]() {
            onSaveSettingClicked(i);
        });
    }
}

void MainWindow::recreatePropertiesButtons()
{
    // Clear existing buttons to prevent memory leaks and crashes
    const QVector<PropertyEntry> &properties = m_propertiesModel->getProperties();
    
    // Remove all existing index widgets
    for (int i = 0; i < m_propertiesModel->rowCount(); i++) {
        QWidget *oldWidget = ui->tableProperties->indexWidget(m_propertiesModel->index(i, 3));
        if (oldWidget) {
            ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, 3), nullptr);
            oldWidget->deleteLater();
        }
    }
    
    // Add save buttons for each property
    for (int i = 0; i < properties.size(); i++) {
        QPushButton *btnSave = new QPushButton("💾");
        btnSave->setMaximumSize(30, 30);
        btnSave->setStyleSheet("QPushButton { font-size: 14px; }");
        btnSave->setToolTip("Save this property to device");
        ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, 3), btnSave);
        
        // Connect button to save slot
        connect(btnSave, &QPushButton::clicked, this, [this, i]() {
            onSavePropertyClicked(i);
        });
    }
}

void MainWindow::recreatePropertyDefinitionButtons()
{
    // Recreate buttons for all rows since row indices may have changed after removal
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    
    for (int row = 0; row < properties.size(); row++) {
        // Add Set button
        QPushButton *btnSet = new QPushButton("Set");
        btnSet->setMaximumSize(45, 25);
        btnSet->setStyleSheet("QPushButton { font-size: 12px; padding: 2px; }");
        btnSet->setToolTip("Set property value");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, 11), btnSet);
        
        connect(btnSet, &QPushButton::clicked, this, [this, row]() {
            onSetPropertyDefinitionClicked(row);
        });
        
        // Add Get button
        QPushButton *btnGet = new QPushButton("Get");
        btnGet->setMaximumSize(45, 25);
        btnGet->setStyleSheet("QPushButton { font-size: 12px; padding: 2px; }");
        btnGet->setToolTip("Get property value");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, 12), btnGet);
        
        connect(btnGet, &QPushButton::clicked, this, [this, row]() {
            onGetPropertyDefinitionClicked(row);
        });
        
        // Add Remove button
        QPushButton *btnRemove = new QPushButton("❌");
        btnRemove->setMaximumSize(50, 25);
        btnRemove->setStyleSheet("QPushButton { font-size: 12px; padding: 2px; }");
        btnRemove->setToolTip("Remove property from list");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, 13), btnRemove);
        
        connect(btnRemove, &QPushButton::clicked, this, [this, row]() {
            onRemovePropertyDefinitionClicked(row);
        });
    }
}

void MainWindow::onFilterChanged()
{
    applyFilters();
}

void MainWindow::onSettingsFilterChanged()
{
    QString filterText = ui->txtFilterSettings->text();
    m_settingsModel->applyFilter(filterText);
}

void MainWindow::onPropertiesFilterChanged()
{
    QString filterText = ui->txtFilterProperties->text();
    m_propertiesModel->applyFilter(filterText);
}

void MainWindow::onRefreshSettingsClicked()
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    
    ui->txtFilterSettings->clear();
    m_settingsModel->clearFilter();
    
    // Fetch settings from device via ADB
    AdbManager::instance().fetchSettings(m_currentDeviceId);
}

void MainWindow::onRefreshPropertiesClicked()
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    
    ui->txtFilterProperties->clear();
    m_propertiesModel->clearFilter();
    
    // Fetch properties from device via ADB
    AdbManager::instance().fetchProperties(m_currentDeviceId);
}

void MainWindow::onSettingsFetched(const QVector<SettingEntry> &settings)
{
    m_settingsModel->setSettings(settings);
    recreateSettingsButtons();
}

void MainWindow::onPropertiesFetched(const QVector<PropertyEntry> &properties)
{
    m_propertiesModel->setProperties(properties);
    recreatePropertiesButtons();
}

void MainWindow::onSaveSettingClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    
    // Get the setting entry from model
    const QVector<SettingEntry> &settings = m_settingsModel->getSettings();
    if (row < 0 || row >= settings.size()) {
        return;
    }
    
    const SettingEntry &entry = settings[row];
    QString newValue = m_settingsModel->data(m_settingsModel->index(row, 3), Qt::DisplayRole).toString();
    
    // Set the value via ADB
    QString error;
    bool success = AdbManager::instance().setSetting(m_currentDeviceId, entry.group, entry.setting, newValue, error);
    
    if (!success) {
        QMessageBox::warning(this, "Failed to Set Value", 
            QString("Failed to set %1.%2:\n%3").arg(entry.group, entry.setting, error));
        return;
    }
    
    // Verify the value was actually set
    QString verifiedValue = AdbManager::instance().verifySetting(m_currentDeviceId, entry.group, entry.setting);
    
    if (verifiedValue != newValue) {
        QMessageBox::warning(this, "Value Not Set", 
            QString("Setting %1.%2 could not be set.\nExpected: %3\nActual: %4\n\nThis setting may be read-only or require special permissions.")
                .arg(entry.group, entry.setting, newValue, verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        ui->statusbar->showMessage(QString("Successfully set %1.%2 = %3").arg(entry.group, entry.setting, newValue), 3000);
    }
}

void MainWindow::onSavePropertyClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    
    // Get the property entry from model
    const QVector<PropertyEntry> &properties = m_propertiesModel->getProperties();
    if (row < 0 || row >= properties.size()) {
        return;
    }
    
    const PropertyEntry &entry = properties[row];
    QString newValue = m_propertiesModel->data(m_propertiesModel->index(row, 2), Qt::DisplayRole).toString();
    
    // Set the value via ADB
    QString error;
    bool success = AdbManager::instance().setProperty(m_currentDeviceId, entry.property, newValue, error);
    
    if (!success) {
        QMessageBox::warning(this, "Failed to Set Property", 
            QString("Failed to set %1:\n%2").arg(entry.property, error));
        return;
    }
    
    // Verify the value was actually set
    QString verifiedValue = AdbManager::instance().verifyProperty(m_currentDeviceId, entry.property);
    
    if (verifiedValue != newValue) {
        QMessageBox::warning(this, "Property Not Set", 
            QString("Property %1 could not be set.\nExpected: %2\nActual: %3\n\nThis property may be read-only or require special permissions.")
                .arg(entry.property, newValue, verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        ui->statusbar->showMessage(QString("Successfully set %1 = %2").arg(entry.property, newValue), 3000);
    }
}

void MainWindow::applyFilters()
{
    filteredLogs.clear();
    
    for (const auto &entry : allLogs) {
        if (passesFilter(entry)) {
            filteredLogs.append(entry);
        }
    }
    
    // Update model with filtered data
    m_logModel->setLogs(filteredLogs);
    
    updateFilterCount();
    updateStatusBar();
}

bool MainWindow::passesFilter(const LogEntry &entry)
{
    FilterCriteria criteria = buildFilterCriteria();
    return m_logFilter.passesFilter(entry, criteria);
}

FilterCriteria MainWindow::buildFilterCriteria() const
{
    FilterCriteria criteria;
    
    // Message filter
    criteria.messageFilter = ui->txtFindMessage->text();
    if (criteria.messageFilter.contains("&&")) {
        criteria.messageOperator = FilterOperator::AND;
    } else {
        criteria.messageOperator = FilterOperator::OR;
    }
    
    // Time range filter
    criteria.startTime = ui->txtStartTime->text();
    criteria.endTime = ui->txtEndTime->text();
    
    // Tag filter
    criteria.tagFilter = ui->txtTagFilter->text();
    if (criteria.tagFilter.contains("&&")) {
        criteria.tagOperator = FilterOperator::AND;
    } else {
        criteria.tagOperator = FilterOperator::OR;
    }
    
    // Package filter
    criteria.packageFilter = ui->txtPackageFilter->text();
    if (criteria.packageFilter.contains("&&")) {
        criteria.packageOperator = FilterOperator::AND;
    } else {
        criteria.packageOperator = FilterOperator::OR;
    }
    
    // PID filter
    criteria.pidFilter = ui->txtPidFilter->text();
    if (criteria.pidFilter.contains("&&")) {
        criteria.pidOperator = FilterOperator::AND;
    } else {
        criteria.pidOperator = FilterOperator::OR;
    }
    
    // TID filter is not in UI yet, but structure supports it
    criteria.tidFilter = "";
    criteria.tidOperator = FilterOperator::OR;
    
    // Level filter
    if (ui->radioVerbosePlus->isChecked()) criteria.minLevel = "V";
    else if (ui->radioV->isChecked()) criteria.minLevel = "V";
    else if (ui->radioD->isChecked()) criteria.minLevel = "D";
    else if (ui->radioI->isChecked()) criteria.minLevel = "I";
    else if (ui->radioW->isChecked()) criteria.minLevel = "W";
    else if (ui->radioE->isChecked()) criteria.minLevel = "E";
    else if (ui->radioA->isChecked()) criteria.minLevel = "A";
    
    return criteria;
}



void MainWindow::updateFilterCount()
{
    ui->lblFilterCount->setText(QString("Showing: %1 / %2")
                                    .arg(filteredLogs.size())
                                    .arg(allLogs.size()));
}

void MainWindow::updateStatusBar()
{
    QString status = QString("UTF-8  Lines: %1    Mem: %2MB  ● %3")
                        .arg(filteredLogs.size())
                        .arg(memoryUsage)
                        .arg(isPaused ? "Paused" : "Running");
    ui->statusbar->showMessage(status);
}

void MainWindow::onResumeClicked()
{
    isPaused = !isPaused;
    ui->btnResume->setText(isPaused ? "▶ RESUME" : "⏸ PAUSE");
    updateStatusBar();
}

void MainWindow::onClearClicked()
{
    allLogs.clear();
    filteredLogs.clear();
    m_logModel->clear();
    updateFilterCount();
    updateStatusBar();
}

void MainWindow::onCopyClicked()
{
    QModelIndexList selectedIndexes = ui->tableLog->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty())
        return;
    
    QStringList selectedText;
    
    for (const QModelIndex &index : selectedIndexes) {
        QStringList rowData;
        for (int col = 0; col < m_logModel->columnCount(); ++col) {
            QModelIndex cellIndex = m_logModel->index(index.row(), col);
            rowData << m_logModel->data(cellIndex, Qt::DisplayRole).toString();
        }
        selectedText.append(rowData.join("\t"));
    }
    
    QApplication::clipboard()->setText(selectedText.join("\n"));
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
    
    // Create checkboxes for each column
    QVector<QCheckBox*> checkboxes;
    QStringList columnNames = {"Date", "Time", "PID", "TID", "Package", "Lvl", "Tag", "Message"};
    
    for (int i = 0; i < columnNames.size(); ++i) {
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
        "QPushButton:hover { background-color: #1177bb; }"
    );
    
    // Show dialog and apply changes
    if (dialog.exec() == QDialog::Accepted) {
        for (int i = 0; i < checkboxes.size(); ++i) {
            ui->tableLog->setColumnHidden(i, !checkboxes[i]->isChecked());
        }
    }
}

void MainWindow::onAutoScrollToggled(bool checked)
{
    if (checked) {
        ui->tableLog->scrollToBottom();
    }
}

void MainWindow::onSettingsClicked()
{
    // TODO: Implement settings dialog
}

void MainWindow::onDeviceChanged(int index)
{
    // UI placeholder - device selection changed
    QString deviceName = ui->cmbDevice->itemText(index);
    QString deviceId = ui->cmbDevice->itemData(index).toString();
    
    // Update both local and AdbManager's current device
    m_currentDeviceId = deviceId;
    AdbManager::instance().setCurrentDeviceId(deviceId);
    
    if (!deviceId.isEmpty()) {
        ui->statusbar->showMessage(QString("Selected device: %1").arg(deviceName), 2000);
    }
}

void MainWindow::onTableContextMenu(const QPoint &pos)
{
    QModelIndex index = ui->tableLog->indexAt(pos);
    if (!index.isValid()) return;
    
    int column = index.column();
    QString value = m_logModel->data(index, Qt::DisplayRole).toString();
    QString filterType;
    QString displayName;
    
    // Determine which filter based on column
    // Columns: 0=Date, 1=Time, 2=PID, 3=TID, 4=Package, 5=Lvl, 6=Tag, 7=Message
    switch (column) {
        case 2: // PID
            filterType = "pid";
            displayName = "PID";
            break;
        case 3: // TID
            filterType = "tid";
            displayName = "TID";
            break;
        case 4: // Package
            filterType = "package";
            displayName = "Package";
            break;
        case 6: // Tag
            filterType = "tag";
            displayName = "Tag";
            break;
        default:
            return; // Not a filterable column
    }
    
    // Create context menu
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2d2d30; color: #cccccc; border: 1px solid #3e3e42; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #0e639c; }"
    );
    
    QString orText = QString("Add '%1' to %2 filter (OR ||)").arg(value).arg(displayName);
    QString andText = QString("Add '%1' to %2 filter (AND &&)").arg(value).arg(displayName);
    QAction *addFilterOrAction = menu.addAction(orText);
    QAction *addFilterAndAction = menu.addAction(andText);
    
    QAction *selected = menu.exec(ui->tableLog->viewport()->mapToGlobal(pos));
    if (selected == addFilterOrAction) {
        addToFilter(filterType, value, FilterOperator::OR);
    } else if (selected == addFilterAndAction) {
        addToFilter(filterType, value, FilterOperator::AND);
    }
}

void MainWindow::addToFilter(const QString &filterType, const QString &value, FilterOperator op)
{
    QLineEdit *filterField = nullptr;
    
    if (filterType == "tag") {
        filterField = ui->txtTagFilter;
    } else if (filterType == "package") {
        filterField = ui->txtPackageFilter;
    } else if (filterType == "pid") {
        filterField = ui->txtPidFilter;
    } else if (filterType == "tid") {
        // Note: There's no TID filter in the current UI, but we can add it to PID filter
        // or you could add a TID filter field to the UI
        filterField = ui->txtPidFilter;
    }
    
    if (!filterField) return;
    
    QString currentFilter = filterField->text().trimmed();
    QString newFilter;
    QString separator = (op == FilterOperator::OR) ? "||" : "&&";
    
    if (currentFilter.isEmpty()) {
        newFilter = value;
    } else {
        // Check if value already exists in filter
        QStringList filterParts;
        if (currentFilter.contains("&&")) {
            filterParts = currentFilter.split("&&");
        } else if (currentFilter.contains("||")) {
            filterParts = currentFilter.split("||");
        } else {
            filterParts = currentFilter.split("|");
        }
        
        bool valueExists = false;
        for (const QString &part : filterParts) {
            if (part.trimmed() == value) {
                valueExists = true;
                break;
            }
        }
        
        if (!valueExists) {
            newFilter = currentFilter + separator + value;
        } else {
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
    
    if (devices.isEmpty()) {
        ui->cmbDevice->addItem("No devices found", "");
        ui->lblDeviceStatus->setStyleSheet("color: #f87171; font-size: 16px;"); // Red
        m_currentDeviceId = "";
        AdbManager::instance().setCurrentDeviceId("");
    } else {
        for (const AdbDevice &device : devices) {
            ui->cmbDevice->addItem(device.name, device.id);
        }
        ui->lblDeviceStatus->setStyleSheet("color: #34d399; font-size: 16px;"); // Green
        
        // Restore previous selection if still available
        int index = ui->cmbDevice->findData(currentDeviceId);
        if (index >= 0) {
            ui->cmbDevice->setCurrentIndex(index);
        } else {
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
    parseAndAddLogLine(line);
}

void MainWindow::parseAndAddLogLine(const QString &line)
{
    // Use the configured log converter to parse the line
    LogEntry entry = m_logConverter->convert(line);
    
    // Check if parsing was successful
    if (!entry.isValid()) {
        return;
    }
    
    allLogs.append(entry);
    
    // Apply filters and update display if it passes
    if (passesFilter(entry)) {
        filteredLogs.append(entry);
        
        // Add to model
        m_logModel->addLog(entry);
        
        // Auto-scroll to bottom if enabled
        if (ui->btnAutoScroll->isChecked()) {
            ui->tableLog->scrollToBottom();
        }
        
        updateFilterCount();
    }
}

void MainWindow::onLoadFileClicked()
{
    QString filePath = ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
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
        "Log Files (*.log *.txt);;All Files (*.*)"
    );
    
    if (!filePath.isEmpty()) {
        ui->txtFilePath->setText(filePath);
        loadLogsFromFile(filePath);
    }
}

void MainWindow::onSaveFileClicked()
{
    QString filePath = ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
        ui->statusbar->showMessage("Please enter a file path", 3000);
        return;
    }
    
    // Add .log extension if not present
    if (!filePath.endsWith(".log") && !filePath.endsWith(".txt")) {
        filePath += ".log";
    }
    
    QString errorMsg;
    bool success = m_fileManager.saveToFile(filePath, allLogs, errorMsg);
    
    if (success) {
        ui->statusbar->showMessage(QString("Saved %1 log entries to %2")
                                       .arg(allLogs.size())
                                       .arg(filePath), 3000);
    } else {
        ui->statusbar->showMessage(QString("Failed to save: %1").arg(errorMsg), 5000);
    }
}

void MainWindow::loadLogsFromFile(const QString &filePath)
{
    ui->statusbar->showMessage("Loading log file...", 0);
    
    // Prepare converters for auto-detection
    QVector<LogConverterPtr> converters;
    converters.append(LogConverterPtr(new ThreadtimeLogConverter()));
    converters.append(LogConverterPtr(new BriefLogConverter()));
    
    QString errorMsg;
    LogConverterPtr usedConverter;
    QVector<LogEntry> logs = m_fileManager.readFromFileAuto(filePath, converters, usedConverter, errorMsg);
    
    qDebug() << "Loading file:" << filePath;
    qDebug() << "Loaded entries:" << logs.size();
    qDebug() << "Error message:" << errorMsg;
    qDebug() << "Line count:" << m_fileManager.getLastLineCount();
    qDebug() << "Parsed count:" << m_fileManager.getLastParsedCount();
    
    if (logs.isEmpty() && !errorMsg.isEmpty()) {
        ui->statusbar->showMessage(QString("Failed to load file: %1").arg(errorMsg), 5000);
        return;
    }
    
    if (logs.isEmpty()) {
        ui->statusbar->showMessage("No valid log entries found in file", 5000);
        return;
    }
    
    // Clear existing logs and load new ones
    allLogs.clear();
    allLogs = logs;
    
    // Update converter to match the detected format
    if (usedConverter) {
        m_logConverter = usedConverter;
    }
    
    // Clear filters to show all loaded data
    ui->txtFindMessage->clear();
    ui->txtStartTime->clear();
    ui->txtEndTime->clear();
    ui->txtTagFilter->clear();
    ui->txtPackageFilter->clear();
    ui->txtPidFilter->clear();
    
    // Apply filters and display
    applyFilters();
    updateFilterCount();
    updateStatusBar();
    
    // Show success message
    ui->statusbar->showMessage(
        QString("Loaded %1 log entries from %2 (Format: %3, Parsed: %4/%5)")
            .arg(logs.size())
            .arg(filePath)
            .arg(usedConverter ? usedConverter->name() : "Unknown")
            .arg(m_fileManager.getLastParsedCount())
            .arg(m_fileManager.getLastLineCount()),
        5000
    );
}

void MainWindow::onLogTableDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || index.row() >= filteredLogs.size()) {
        return;
    }
    
    int row = index.row();
    const LogEntry &entry = filteredLogs[row];
    
    // Toggle mark state
    if (m_markedRows.contains(row)) {
        // Unmark
        m_markedRows.remove(row);
        m_markLogModel->removeMarkedLog(row);
    } else {
        // Mark
        m_markedRows.insert(row);
        m_markLogModel->addMarkedLog(entry, row);
    }
    
    // Notify model to update highlighting
    m_logModel->setMarkedRows(&m_markedRows);
}

void MainWindow::onMarkLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    // Get the original index in the main log table
    int originalRow = m_markLogModel->getOriginalIndex(index.row());
    if (originalRow < 0 || originalRow >= filteredLogs.size()) {
        return;
    }
    
    // Scroll to the row and position it in the middle of the viewport
    QModelIndex targetIndex = m_logModel->index(originalRow, 0);
    ui->tableLog->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
    
    // Select the row
    ui->tableLog->selectRow(originalRow);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Handle property search box - show all properties on focus/click
    if (obj == ui->txtPropertySearch && event->type() == QEvent::FocusIn) {
        QCompleter *completer = ui->txtPropertySearch->completer();
        if (completer && completer->model() && completer->model()->rowCount() > 0) {
            completer->complete();
        }
    }
    
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(obj);
        
        if (lineEdit) {
            // Handle UP key - navigate to previous filter in history
            if (keyEvent->key() == Qt::Key_Up) {
                navigateHistory(lineEdit, true);
                return true; // Event handled
            }
            // Handle DOWN key - navigate to next filter in history
            else if (keyEvent->key() == Qt::Key_Down) {
                navigateHistory(lineEdit, false);
                return true; // Event handled
            }
            // Handle Enter/Return key - save current text to history
            else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                saveToHistory(lineEdit);
                // Let the event propagate for normal QLineEdit handling (e.g., returnPressed signal)
                return QMainWindow::eventFilter(obj, event);
            }
        }
    }
    else if (event->type() == QEvent::FocusOut) {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(obj);
        if (lineEdit && !lineEdit->text().isEmpty()) {
            // Save to history when focus is lost and there's text
            saveToHistory(lineEdit);
        }
    }
    
    // Pass the event to the base class
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::saveToHistory(QLineEdit *lineEdit)
{
    if (!lineEdit) return;
    
    QString text = lineEdit->text().trimmed();
    
    // Don't save empty strings
    if (text.isEmpty()) {
        return;
    }
    
    QStringList &history = m_filterHistory[lineEdit];
    
    // Don't add duplicate if it's the same as the most recent entry
    if (!history.isEmpty() && history.last() == text) {
        return;
    }
    
    // Remove if it exists elsewhere in history (move to end)
    history.removeAll(text);
    
    // Add to end of history
    history.append(text);
    
    // Limit history size
    if (history.size() > MAX_HISTORY_SIZE) {
        history.removeFirst();
    }
    
    // Reset history index
    m_historyIndex[lineEdit] = history.size();
    m_currentText[lineEdit].clear();
}

void MainWindow::navigateHistory(QLineEdit *lineEdit, bool up)
{
    if (!lineEdit) return;
    
    QStringList &history = m_filterHistory[lineEdit];
    
    // If history is empty, do nothing
    if (history.isEmpty()) {
        return;
    }
    
    // Initialize history index if not set
    if (!m_historyIndex.contains(lineEdit)) {
        m_historyIndex[lineEdit] = history.size();
        m_currentText[lineEdit] = lineEdit->text();
    }
    
    int &index = m_historyIndex[lineEdit];
    
    // Store current text if we're at the end of history
    if (index == history.size()) {
        m_currentText[lineEdit] = lineEdit->text();
    }
    
    if (up) {
        // Navigate backward (older entries)
        if (index > 0) {
            index--;
            lineEdit->setText(history[index]);
        }
    } else {
        // Navigate forward (newer entries)
        if (index < history.size() - 1) {
            index++;
            lineEdit->setText(history[index]);
        } else if (index == history.size() - 1) {
            // Move beyond last history item - restore current text
            index++;
            lineEdit->setText(m_currentText[lineEdit]);
        }
    }
}

void MainWindow::onSearchPropertyDefinition()
{
    // Search is handled by QCompleter automatically
    // This slot is triggered when user presses Enter
    QString searchText = ui->txtPropertySearch->text().trimmed();
    
    if (searchText.isEmpty()) {
        return;
    }
    
    // Try to find the property in available definitions
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions) {
        if (propDef.name.compare(searchText, Qt::CaseInsensitive) == 0) {
            // Show property information in status bar
            ui->statusbar->showMessage(
                QString("Found: %1 (ID: %2, Supported: %3)")
                    .arg(propDef.name)
                    .arg(propDef.id)
                    .arg(propDef.isSupported ? "Yes" : "No"),
                3000
            );
            return;
        }
    }
    
    ui->statusbar->showMessage(QString("Property '%1' not found").arg(searchText), 3000);
}

void MainWindow::onAddPropertyDefinition()
{
    QString searchText = ui->txtPropertySearch->text().trimmed();
    
    if (searchText.isEmpty()) {
        QMessageBox::warning(this, "No Property Selected", "Please enter or select a property name.");
        return;
    }
    
    // Find the property in available definitions
    PropertyDefinition selectedProp;
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions) {
        if (propDef.name.compare(searchText, Qt::CaseInsensitive) == 0) {
            selectedProp = propDef;
            break;
        }
    }
    
    if (!selectedProp.isValid()) {
        QMessageBox::warning(this, "Property Not Found", 
            QString("Property '%1' not found in available definitions.\nPlease fetch property definitions first.").arg(searchText));
        return;
    }
    
    // Add to model
    int oldRowCount = m_propertyDefinitionModel->rowCount();
    m_propertyDefinitionModel->addPropertyDefinition(selectedProp);
    int newRowCount = m_propertyDefinitionModel->rowCount();
    
    if (newRowCount > oldRowCount) {
        // Property was added (not a duplicate)
        int row = newRowCount - 1;
        
        // Add Set button
        QPushButton *btnSet = new QPushButton("Set");
        btnSet->setMaximumSize(45, 25);
        btnSet->setStyleSheet("QPushButton { font-size: 12px; padding: 2px; }");
        btnSet->setToolTip("Set property value");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, 11), btnSet);
        
        connect(btnSet, &QPushButton::clicked, this, [this, row]() {
            onSetPropertyDefinitionClicked(row);
        });
        
        // Add Get button
        QPushButton *btnGet = new QPushButton("Get");
        btnGet->setMaximumSize(45, 25);
        btnGet->setStyleSheet("QPushButton { font-size: 12px; padding: 2px; }");
        btnGet->setToolTip("Get property value");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, 12), btnGet);
        
        connect(btnGet, &QPushButton::clicked, this, [this, row]() {
            onGetPropertyDefinitionClicked(row);
        });
        
        // Add Remove button
        QPushButton *btnRemove = new QPushButton("❌");
        btnRemove->setMaximumSize(50, 25);
        btnRemove->setStyleSheet("QPushButton { font-size: 12px; padding: 2px; }");
        btnRemove->setToolTip("Remove property from list");
        ui->tablePropertyDefinitions->setIndexWidget(m_propertyDefinitionModel->index(row, 13), btnRemove);
        
        connect(btnRemove, &QPushButton::clicked, this, [this, row]() {
            onRemovePropertyDefinitionClicked(row);
        });
        
        ui->statusbar->showMessage(QString("Added property: %1").arg(selectedProp.name), 2000);
        ui->txtPropertySearch->clear();
    } else {
        ui->statusbar->showMessage(QString("Property '%1' already in list").arg(selectedProp.name), 2000);
    }
}

void MainWindow::onFetchPropertyDefinitions()
{
    if (m_currentDeviceId.isEmpty()) {
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
        3000
    );
}

void MainWindow::onGetPropertyDefinitionClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) {
        return;
    }
    
    const PropertyDefinition &propDef = properties[row];
    
    QString value;
    QString error;
    bool success = AdbManager::instance().getPropertyDefinitionValue(m_currentDeviceId, propDef.id, value, error);
    
    if (success) {
        // Update the VALUE column in the model
        m_propertyDefinitionModel->setData(m_propertyDefinitionModel->index(row, 10), value, Qt::EditRole);
        
        QMessageBox::information(this, "Property Value", 
            QString("Property: %1\nValue: %2").arg(propDef.name, value));
    } else {
        QMessageBox::warning(this, "Failed to Get Value", 
            QString("Failed to get %1:\n%2").arg(propDef.name, error));
    }
}

void MainWindow::onSetPropertyDefinitionClicked(int row)
{
    if (m_currentDeviceId.isEmpty()) {
        QMessageBox::warning(this, "No Device", "Please select a device first.");
        return;
    }
    
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) {
        return;
    }
    
    const PropertyDefinition &propDef = properties[row];
    
    // Check if property is read-only
    if (propDef.read_only) {
        QMessageBox::warning(this, "Read-Only Property", 
            QString("Property '%1' is marked as read-only and cannot be modified.").arg(propDef.name));
        return;
    }
    
    // Get current value from the VALUE column (column 10)
    QString currentValue = m_propertyDefinitionModel->data(m_propertyDefinitionModel->index(row, 10), Qt::DisplayRole).toString();
    
    // Prompt user for new value
    bool ok;
    QString newValue = QInputDialog::getText(this, "Set Property Value",
        QString("Enter new value for '%1' (ID: %2):").arg(propDef.name, propDef.id),
        QLineEdit::Normal, currentValue, &ok);
    
    if (!ok || newValue.isEmpty()) {
        return;
    }
    
    QString error;
    bool success = AdbManager::instance().setPropertyDefinitionValue(m_currentDeviceId, propDef.id, newValue, error);
    
    if (success) {
        // Update the VALUE column in the model
        m_propertyDefinitionModel->setData(m_propertyDefinitionModel->index(row, 10), newValue, Qt::EditRole);
        
        QString message = QString("Successfully set %1 = %2").arg(propDef.name, newValue);
        
        if (propDef.need_reboot) {
            message += "\n\nNote: This property requires a device reboot to take effect.";
        }
        
        QMessageBox::information(this, "Property Set", message);
        ui->statusbar->showMessage(QString("Set %1 = %2").arg(propDef.name, newValue), 3000);
    } else {
        QMessageBox::warning(this, "Failed to Set Value", 
            QString("Failed to set %1:\n%2").arg(propDef.name, error));
    }
}

void MainWindow::onRemovePropertyDefinitionClicked(int row)
{
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) {
        return;
    }
    
    const PropertyDefinition &propDef = properties[row];
    
    // Confirm removal
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Remove Property", 
        QString("Remove property '%1' from the list?").arg(propDef.name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_propertyDefinitionModel->removePropertyDefinition(row);
        
        // Recreate all buttons for remaining rows since row indices changed
        recreatePropertyDefinitionButtons();
        
        ui->statusbar->showMessage(QString("Removed property: %1").arg(propDef.name), 2000);
    }
}

void MainWindow::onClearAllPropertyDefinitions()
{
    if (m_propertyDefinitionModel->rowCount() == 0) {
        QMessageBox::information(this, "Empty List", "The property list is already empty.");
        return;
    }
    
    // Confirm clear all
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Clear All Properties", 
        QString("Remove all %1 properties from the list?").arg(m_propertyDefinitionModel->rowCount()),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_propertyDefinitionModel->clear();
        ui->statusbar->showMessage("Cleared all properties", 2000);
    }
}

void MainWindow::updatePropertyNamesCompleter()
{
    // Extract property names from available definitions
    QStringList propertyNames;
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions) {
        propertyNames.append(propDef.name);
    }
    
    QCompleter *completer = ui->txtPropertySearch->completer();
    if (completer) {
        if (propertyNames.isEmpty()) {
            // If list is empty, set an empty model so completer shows nothing
            completer->setModel(new QStringListModel(QStringList(), completer));
        } else {
            // Set the model with property names
            QStringListModel *model = new QStringListModel(propertyNames, completer);
            completer->setModel(model);
        }
    }
}

void MainWindow::createSampleSettings()
{
    QVector<SettingEntry> settings;
    settings.append({"1", "Global", "phenotype_boot_count", "9"});
    settings.append({"2", "Global", "phenotype_flags", "classifier_consta..."});
    settings.append({"3", "Global", "_boot_Phenotype_flags", ""});
    settings.append({"4", "Global", "activity_starts_logging_enabled", "1"});
    settings.append({"5", "Global", "adb_enabled", "1"});
    settings.append({"6", "Global", "adb_wifi_enabled", "0"});
    settings.append({"7", "Global", "add_users_when_locked", "0"});
    settings.append({"8", "Global", "airplane_mode_on", "0"});
    settings.append({"9", "Global", "airplane_mode_radios", "cell,bluetooth,wi..."});
    settings.append({"10", "Global", "airplane_mode_toggleable_radios", "bluetooth,wifi"});
    settings.append({"11", "Global", "alarm_manager_constants", ""});
    settings.append({"12", "Global", "alarm_manager_dummy_flags", "null"});
    settings.append({"13", "Global", "alt_bypass_wifi_requirement_time_...", "0"});
    settings.append({"14", "Global", "always_on_display_constants", "null"});
    settings.append({"15", "Global", "ambient_enabled", "1"});
}

void MainWindow::createSampleProperties()
{
    QVector<PropertyEntry> properties;
    properties.append({"1", "DEVICE_PROVISIONED", "1"});
    properties.append({"2", "aaudio.hw_burst_min_usec", "2000"});
    properties.append({"3", "aaudio.mmap_exclusive_policy", "2"});
    properties.append({"4", "aaudio.mmap_policy", "2"});
    properties.append({"5", "af.fast_track_multiplier", "1"});
    properties.append({"6", "apex.all.ready", "true"});
    properties.append({"7", "apexd.status", "ready"});
    properties.append({"8", "arm64.memtag.process.system_server", "off"});
    properties.append({"9", "audio.deep_buffer.media", "true"});
    properties.append({"10", "audio.offload.min.duration.secs", "30"});
    properties.append({"11", "audio.offload.video", "true"});
    properties.append({"12", "audio.sys.mute.latency.factor", "2"});
    properties.append({"13", "audio.sys.noisy.broadcast.delay", "500"});
    properties.append({"14", "audio.sys.offload.pstimeout.secs", "3"});
    properties.append({"15", "audio.sys.routing.latency", "0"});
}

void MainWindow::createSamplePropertyDefinitions()
{
    QVector<PropertyDefinition> samplePropertyDefs;
    
    PropertyDefinition prop1;
    prop1.id = "1";
    prop1.name = "persist.sys.usb.config";
    prop1.isSupported = true;
    prop1.isLoaded = true;
    prop1.need_reboot = false;
    prop1.volatile_ = false;
    prop1.read_only = false;
    prop1.optional = false;
    prop1.persistence = "persist";
    prop1.eManager = "SystemManager";
    prop1.value = "";
    samplePropertyDefs.append(prop1);
    
    PropertyDefinition prop2;
    prop2.id = "2";
    prop2.name = "ro.build.version.sdk";
    prop2.isSupported = true;
    prop2.isLoaded = true;
    prop2.need_reboot = false;
    prop2.volatile_ = false;
    prop2.read_only = true;
    prop2.optional = false;
    prop2.persistence = "readonly";
    prop2.eManager = "BuildManager";
    prop2.value = "";
    samplePropertyDefs.append(prop2);
    
    PropertyDefinition prop3;
    prop3.id = "3";
    prop3.name = "debug.hwui.renderer";
    prop3.isSupported = true;
    prop3.isLoaded = false;
    prop3.need_reboot = true;
    prop3.volatile_ = false;
    prop3.read_only = false;
    prop3.optional = true;
    prop3.persistence = "persist";
    prop3.eManager = "GraphicsManager";
    prop3.value = "";
    samplePropertyDefs.append(prop3);
    
    PropertyDefinition prop4;
    prop4.id = "4";
    prop4.name = "sys.boot_completed";
    prop4.isSupported = true;
    prop4.isLoaded = true;
    prop4.need_reboot = false;
    prop4.volatile_ = true;
    prop4.read_only = false;
    prop4.optional = false;
    prop4.persistence = "runtime";
    prop4.eManager = "SystemManager";
    prop4.value = "";
    samplePropertyDefs.append(prop4);
    
    PropertyDefinition prop5;
    prop5.id = "5";
    prop5.name = "persist.sys.timezone";
    prop5.isSupported = true;
    prop5.isLoaded = true;
    prop5.need_reboot = false;
    prop5.volatile_ = false;
    prop5.read_only = false;
    prop5.optional = false;
    prop5.persistence = "persist";
    prop5.eManager = "TimeManager";
    prop5.value = "";
    samplePropertyDefs.append(prop5);
    
    // Store sample property definitions for auto-complete (table starts empty)
    m_availablePropertyDefinitions = samplePropertyDefs;
    QStringList propertyNames;
    for (const PropertyDefinition &propDef : samplePropertyDefs) {
        propertyNames.append(propDef.name);
    }
    // QStringListModel *completerModel = new QStringListModel(propertyNames, completer);
    // completer->setModel(completerModel);
}
