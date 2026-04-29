// UiManager: connect() wiring for top-level toolbar / forms / shortcuts.
// Defines: connectAdbManagerSignals, connectFilterSignals, connectButtonSignals,
// connectTableSignals.
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "configurationcontroller.h"
#include "cradlecontroller.h"
#include "devicesmanager.h"
#include "dumpsyscontroller.h"
#include "filemanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "settingsmodel.h"
#include "logfiltercontroller.h"

#include <QPushButton>
#include <QShortcut>
#include <QTableView>
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QRadioButton>
#include <QStatusBar>

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Signal connections
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::connectAdbManagerSignals()
{
    AdbManager &adb = AdbManager::instance();

    connect(&adb, &AdbManager::devicesChanged,           this, &UiManager::onDevicesChanged);
    connect(&adb, &AdbManager::logcatLineReceived,        this, &UiManager::onLogcatLineReceived);
    connect(&adb, &AdbManager::dmesgLineReceived,         this, &UiManager::parseDmesgLine);
    connect(&adb, &AdbManager::settingsFetched,           m_configurationController, &ConfigurationController::onSettingsFetched);
    connect(&adb, &AdbManager::propertiesFetched,         m_configurationController, &ConfigurationController::onPropertiesFetched);
    connect(&adb, &AdbManager::propertyDefinitionsFetched,m_configurationController, &ConfigurationController::onPropertyDefinitionsFetched);
    connect(&adb, &AdbManager::settingSaveResult,         m_configurationController, &ConfigurationController::onSettingSaveResult);
    connect(&adb, &AdbManager::propertySaveResult,        m_configurationController, &ConfigurationController::onPropertySaveResult);

    connect(&adb, &AdbManager::dmesgStopped, this, [this]() {
        setKernelRunningVisuals(false);
        flashStatus(tr("Kernel log stopped"));
    });
    connect(&adb, &AdbManager::dmesgFailed, this, [this](const QString &reason) {
        setKernelRunningVisuals(false);
        QMessageBox::warning(m_mainWindow, tr("Kernel Log — Root Required"), reason);
    });
    connect(&adb, &AdbManager::logcatStarted, this, [this]() {
        setLogcatRunningVisuals(true);
        flashStatus(tr("Logcat started"));
    });
    connect(&adb, &AdbManager::logcatStopped, this, [this]() {
        m_batchFlushTimer->stop();
        flushPendingLines();
        setLogcatRunningVisuals(false);
        flashStatus(tr("Logcat stopped"));
    });

    // Populate device list with whatever is already connected
    onDevicesChanged(adb.getConnectedDevices());
}

void UiManager::connectFilterSignals()
{
    // Highlight: typing updates visuals, Enter/buttons navigate matches
    connect(m_ui->txtHighlight,      &QLineEdit::textChanged,   this, &UiManager::onHighlightChanged);
    connect(m_ui->txtHighlight,      &QLineEdit::returnPressed, this, &UiManager::onHighlightNextClicked);
    connect(m_ui->btnHighlightNext,  &QPushButton::clicked,     this, &UiManager::onHighlightNextClicked);
    connect(m_ui->btnHighlightPrev,  &QPushButton::clicked,     this, &UiManager::onHighlightPrevClicked);

    // Log filters: apply on Enter (line edits) or change (radios). Looped
    // to keep "all logcat-tab filter inputs share the same handler" obvious
    // and to make adding a new field a one-line change.
    const QList<QLineEdit *> filterEdits = {
        m_ui->txtKeyword, m_ui->txtFindMessage,
        m_ui->txtStartTime, m_ui->txtEndTime,
        m_ui->txtTagFilter, m_ui->txtPackageFilter, m_ui->txtPidFilter
    };
    for (QLineEdit *edit : filterEdits)
        connect(edit, &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);

    // Log-level radio buttons
    const QList<QRadioButton *> levelRadios = {
        m_ui->radioVerbosePlus,
        m_ui->radioV, m_ui->radioD, m_ui->radioI,
        m_ui->radioW, m_ui->radioE, m_ui->radioA
    };
    for (QRadioButton *radio : levelRadios)
        connect(radio, &QRadioButton::toggled, this, &UiManager::onFilterChanged);

    // Configuration tab filters (live: text changed, not Enter)
    connect(m_ui->txtFilterSettings,       &QLineEdit::textChanged, this, &UiManager::onSettingsFilterChanged);
    connect(m_ui->txtFilterSettingsValue,  &QLineEdit::textChanged, this, &UiManager::onSettingsFilterChanged);
    connect(m_ui->txtFilterProperties,     &QLineEdit::textChanged, this, &UiManager::onPropertiesFilterChanged);
    connect(m_ui->txtFilterPropertiesValue,&QLineEdit::textChanged, this, &UiManager::onPropertiesFilterChanged);
    connect(m_ui->btnRefreshSettings,   &QPushButton::clicked, m_configurationController, &ConfigurationController::onRefreshSettingsClicked);
    connect(m_ui->btnRefreshProperties, &QPushButton::clicked, m_configurationController, &ConfigurationController::onRefreshPropertiesClicked);
}

void UiManager::connectButtonSignals()
{
    // Toolbar / main controls
    connect(m_ui->btnStart,        &QPushButton::clicked,  this, &UiManager::onStartClicked);
    connect(m_ui->btnKernel,       &QPushButton::clicked,  this, &UiManager::onKernelClicked);
    connect(m_ui->btnClear,        &QPushButton::clicked,  this, &UiManager::onClearClicked);
    connect(m_ui->btnSave,         &QPushButton::clicked,  this, &UiManager::onSaveFileClicked);
    connect(m_ui->btnAutoScroll,   &QPushButton::toggled,  this, &UiManager::onAutoScrollToggled);
    connect(m_ui->btnFitRows,      &QPushButton::clicked,  this, &UiManager::onFitRowsClicked);
    connect(m_ui->btnClearAllMarked,&QPushButton::clicked, this, &UiManager::onClearAllMarkedClicked);
    connect(m_ui->btnAppSettings,  &QPushButton::clicked,  this, &UiManager::onAppSettingsClicked);

    // btnColumns is superseded by App Settings — kept hidden, no slot wired.
    m_ui->btnColumns->setVisible(false);

    // File I/O
    connect(m_ui->txtFilePath, &QLineEdit::returnPressed, this, &UiManager::onLoadFileClicked);
    connect(m_ui->btnOpen,     &QPushButton::clicked,     this, &UiManager::onOpenFileClicked);

    // SDK
    connect(m_ui->txtPropertySearch,    &QLineEdit::returnPressed, this, [this]() {
        qDebug() << "[Completer] returnPressed";
        m_configurationController->onAddPropertyDefinition();
    });
    connect(m_ui->btnClearAllProperties,    &QPushButton::clicked, m_configurationController, &ConfigurationController::onClearAllPropertyDefinitions);
    connect(m_ui->btnFetchPropertyDefs,     &QPushButton::clicked, m_configurationController, &ConfigurationController::onRefreshPropertyDefinitionValues);
    connect(m_ui->btnSavePropertySet,       &QPushButton::clicked, m_configurationController, &ConfigurationController::onSavePropertySet);
    connect(m_ui->btnLoadPropertySet,       &QPushButton::clicked, m_configurationController, &ConfigurationController::onLoadPropertySet);
    connect(m_ui->btnExportPropertySet,     &QPushButton::clicked, m_configurationController, &ConfigurationController::onExportPropertySet);
    connect(m_ui->btnImportPropertySet,     &QPushButton::clicked, m_configurationController, &ConfigurationController::onImportPropertySet);

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
// SECTION: Toolbar visual helpers (single source of truth)
// ─────────────────────────────────────────────────────────────────────────────

namespace {
// Active-state QSS for the logcat / kernel start buttons. One literal,
// previously inlined inside a connect() lambda.
constexpr const char *kActiveToggleQss =
    "background-color: #c0392b; border: 1px solid #e74c3c; color: white;";
constexpr int kStatusFlashMs = 3000;
}

void UiManager::flashStatus(const QString &message)
{
    if (m_ui && m_ui->statusbar)
        m_ui->statusbar->showMessage(message, kStatusFlashMs);
}

void UiManager::setLogcatRunningVisuals(bool running)
{
    m_ui->btnStart->setStyleSheet(running ? QString::fromLatin1(kActiveToggleQss)
                                          : QString());
    m_ui->btnKernel->setEnabled(!running);
    m_ui->btnFitRows->setEnabled(!running);
    if (!running) onFitRowsClicked();
}

void UiManager::setKernelRunningVisuals(bool running)
{
    m_ui->btnKernel->setStyleSheet(running ? QString::fromLatin1(kActiveToggleQss)
                                           : QString());
    m_ui->btnStart->setEnabled(!running);
    m_ui->btnFitRows->setEnabled(!running);
    if (!running) onFitRowsClicked();
}
