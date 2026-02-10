#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "threadtimelogconverter.h"
#include "brieflogconverter.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_logModel(new LogModel(this))
    , m_markLogModel(new MarkLogModel(this))
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
    ui->tableLog->setColumnWidth(0, 120); // Time
    ui->tableLog->setColumnWidth(1, 60);  // PID
    ui->tableLog->setColumnWidth(2, 60);  // TID
    ui->tableLog->setColumnWidth(3, 200); // Package
    ui->tableLog->setColumnWidth(4, 35);  // Lvl
    ui->tableLog->setColumnWidth(5, 150); // Tag
    
    // Setup mark log table view with model
    ui->tableMarkLog->setModel(m_markLogModel);
    ui->tableMarkLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableMarkLog->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableMarkLog->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableMarkLog->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableMarkLog->setAlternatingRowColors(true);
    
    // Set column widths for mark log table
    ui->tableMarkLog->setColumnWidth(0, 120); // Time
    ui->tableMarkLog->setColumnWidth(1, 60);  // PID
    ui->tableMarkLog->setColumnWidth(2, 60);  // TID
    ui->tableMarkLog->setColumnWidth(3, 200); // Package
    ui->tableMarkLog->setColumnWidth(4, 35);  // Lvl
    ui->tableMarkLog->setColumnWidth(5, 150); // Tag
    
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
    
    // Initialize with current devices
    onDevicesChanged(adbManager.getConnectedDevices());
    
    setupConnections();
    addSampleLogs();
    applyFilters();
    updateStatusBar();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // Filter connections
    connect(ui->txtFindMessage, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->txtStartTime, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->txtEndTime, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->txtTagFilter, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->txtPackageFilter, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->txtPidFilter, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    
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
    connect(ui->btnAdbLogcat, &QPushButton::clicked, this, &MainWindow::onAdbLogcatClicked);
    connect(ui->btnConfiguration, &QPushButton::clicked, this, &MainWindow::onConfigurationClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(ui->cmbDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDeviceChanged);
    
    // File path input - load file when Enter is pressed
    connect(ui->txtFilePath, &QLineEdit::returnPressed, this, &MainWindow::onLoadFileClicked);
}

void MainWindow::addSampleLogs()
{
    // Add sample log entries based on the screenshot
    allLogs.append({"06:58:01.159", "4523", "3898", "com.android.systemui", "V", "CameraService", "Request requires android.permission.MANAGE_USERS"});
    allLogs.append({"06:58:10.759", "1224", "1954", "com.android.phone", "I", "CameraService", "Displayed com.android.settings/.Settings: +350ms"});
    allLogs.append({"06:58:18.759", "5955", "4469", "com.android.systemui", "I", "CameraService", "Displayed com.android.settings/.Settings: +350ms"});
    allLogs.append({"06:58:23.559", "5410", "5334", "com.instagram.android", "D", "CameraService", "Attempting to reconnect to WiFi..."});
    allLogs.append({"06:58:28.359", "2642", "4272", "com.instagram.android", "I", "CameraService", "killing 1293:com.google.android.gms/u0a12 (adj 900): empty #17 [EXTRA_DATA]"});
    allLogs.append({"06:58:37.159", "3462", "4051", "com.instagram.android", "D", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] } [EXTRA_DATA]"});
    allLogs.append({"06:58:49.959", "5034", "1688", "com.example.app", "D", "CameraService", "killing 1293:com.google.android.gms/u0a12 (adj 900): empty #17"});
    allLogs.append({"06:59:09.159", "4301", "3805", "com.spotify.music", "I", "CameraService", "Displayed com.android.settings/.Settings: +350ms"});
    allLogs.append({"06:59:21.959", "3781", "5915", "com.google.android.gms", "I", "CameraService", "Attempting to reconnect to WiFi..."});
    allLogs.append({"06:59:53.959", "5861", "2692", "com.google.android.gms", "I", "CameraService", "killing 1293:com.google.android.gms/u0a12 (adj 900): empty #17 [EXTRA_DATA]"});
    allLogs.append({"06:59:54.359", "5017", "1340", "com.android.phone", "I", "CameraService", "Connected to process 14502 [EXTRA_DATA]"});
    allLogs.append({"06:59:41.159", "1400", "2642", "com.example.app", "D", "CameraService", "Connected to process 14502 [EXTRA_DATA]"});
    allLogs.append({"06:59:42.759", "1624", "3436", "com.spotify.music", "D", "CameraService", "Heap transition to process 4520 [EXTRA_DATA]"});
    allLogs.append({"06:59:53.959", "5861", "2692", "com.google.android.gms", "I", "CameraService", "Attempting to reconnect to WiFi..."});
    allLogs.append({"06:59:54.359", "5017", "1340", "com.android.phone", "I", "CameraService", "Connected to process 14502 [EXTRA_DATA]"});
    allLogs.append({"07:00:04.759", "4414", "2872", "com.spotify.music", "V", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] }"});
    allLogs.append({"07:00:27.559", "2284", "5068", "android.process.media", "D", "CameraService", "BufferedQueueProducer: [com.example.app/MainActivity] disconnect: api=1"});
    allLogs.append({"07:00:32.359", "1134", "2855", "com.spotify.music", "I", "CameraService", "Connected to process 14502"});
    allLogs.append({"07:00:38.759", "5914", "2969", "com.android.phone", "D", "CameraService", "Connected to process 14502"});
    allLogs.append({"07:00:42.759", "4892", "1548", "com.android.systemui", "I", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] } [EXTRA_DATA]"});
    allLogs.append({"07:00:45.159", "3100", "2199", "com.whatsapp", "I", "CameraService", "Starting activity: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] } [EXTRA_DATA]"});
    allLogs.append({"07:00:53.959", "5051", "5698", "android.process.media", "D", "CameraService", "BufferedQueueProducer: [com.example.app/MainActivity] disconnect: api=1 [EXTRA_DATA]"});
    allLogs.append({"07:01:06.759", "1233", "5843", "android.process.media", "I", "CameraService", "Access denied finding property \"ro.serialno\" [EXTRA_DATA]"});
    
    // Add more entries to reach 489 total
    for (int i = 0; i < 466; i++) {
        QString packages[] = {"com.android.systemui", "com.google.android.gms", "com.spotify.music", "com.whatsapp", "com.example.app"};
        QString levels[] = {"V", "D", "I", "W", "E"};
        QString tags[] = {"CameraService", "ActivityManager", "WindowManager", "PackageManager"};
        
        LogEntry entry;
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

void MainWindow::onFilterChanged()
{
    applyFilters();
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
    QStringList columnNames = {"Time", "PID", "TID", "Package", "Lvl", "Tag", "Message"};
    
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

void MainWindow::onAdbLogcatClicked()
{
    AdbManager &adbManager = AdbManager::instance();
    
    if (adbManager.isLogcatRunning()) {
        adbManager.stopLogcat();
        ui->btnAdbLogcat->setText("ADB Logcat");
        ui->statusbar->showMessage("Logcat stopped", 2000);
    } else {
        QString deviceId = ui->cmbDevice->currentData().toString();
        if (deviceId.isEmpty()) {
            ui->statusbar->showMessage("No device selected", 2000);
            return;
        }
        
        if (adbManager.startLogcat(deviceId)) {
            ui->btnAdbLogcat->setText("Stop Logcat");
            ui->statusbar->showMessage("Logcat started", 2000);
            // Clear existing logs when starting new logcat
            allLogs.clear();
            filteredLogs.clear();
            m_logModel->clear();
        } else {
            ui->statusbar->showMessage("Failed to start logcat", 2000);
        }
    }
}

void MainWindow::onConfigurationClicked()
{
    // TODO: Implement configuration dialog
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
    // Columns: 0=Time, 1=PID, 2=TID, 3=Package, 4=Lvl, 5=Tag, 6=Message
    switch (column) {
        case 1: // PID
            filterType = "pid";
            displayName = "PID";
            break;
        case 2: // TID
            filterType = "tid";
            displayName = "TID";
            break;
        case 3: // Package
            filterType = "package";
            displayName = "Package";
            break;
        case 5: // Tag
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
        ui->btnAdbLogcat->setEnabled(false);
    } else {
        for (const AdbDevice &device : devices) {
            ui->cmbDevice->addItem(device.name, device.id);
        }
        ui->lblDeviceStatus->setStyleSheet("color: #34d399; font-size: 16px;"); // Green
        ui->btnAdbLogcat->setEnabled(true);
        
        // Restore previous selection if still available
        int index = ui->cmbDevice->findData(currentDeviceId);
        if (index >= 0) {
            ui->cmbDevice->setCurrentIndex(index);
        }
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

