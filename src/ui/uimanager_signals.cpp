// UiManager: connect() wiring for the toolbar, filter forms and tables.
// Defines: connectAdbManagerSignals, connectFilterSignals, connectButtonSignals,
// connectTableSignals and the toolbar capture-state visuals.
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "configurationcontroller.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "propertiesmodel.h"
#include "settingsmodel.h"

#include <QComboBox>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStatusBar>
#include <QTableView>

namespace {
/** Debounce for the configuration-tab filter boxes, in milliseconds. */
constexpr int kConfigFilterDebounceMs = 180;
constexpr int kStatusFlashMs = 3000;
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Signal connections
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::connectAdbManagerSignals()
{
    AdbManager &adb = AdbManager::instance();

    connect(&adb, &AdbManager::devicesChanged, this, &UiManager::onDevicesChanged);

    // Logcat and kernel lines share one ingest queue; the installed converter
    // decides how each line is parsed.
    connect(&adb, &AdbManager::logcatLineReceived, this, &UiManager::onLogLineReceived);
    connect(&adb, &AdbManager::dmesgLineReceived,  this, &UiManager::onLogLineReceived);

    connect(&adb, &AdbManager::settingsFetched,            m_configurationController, &ConfigurationController::onSettingsFetched);
    connect(&adb, &AdbManager::propertiesFetched,          m_configurationController, &ConfigurationController::onPropertiesFetched);
    connect(&adb, &AdbManager::propertyDefinitionsFetched, m_configurationController, &ConfigurationController::onPropertyDefinitionsFetched);
    connect(&adb, &AdbManager::settingSaveResult,          m_configurationController, &ConfigurationController::onSettingSaveResult);
    connect(&adb, &AdbManager::propertySaveResult,         m_configurationController, &ConfigurationController::onPropertySaveResult);

    connect(&adb, &AdbManager::logcatStarted, this, [this]() {
        setLogcatRunningVisuals(true);
        flashStatus(tr("Logcat started"));
    });
    connect(&adb, &AdbManager::logcatStopped, this, [this]() {
        stopCapture();
        setLogcatRunningVisuals(false);
        flashStatus(tr("Logcat stopped"));
    });
    connect(&adb, &AdbManager::dmesgStopped, this, [this]() {
        stopCapture();
        setKernelRunningVisuals(false);
        flashStatus(tr("Kernel log stopped"));
    });
    connect(&adb, &AdbManager::dmesgFailed, this, [this](const QString &reason) {
        stopCapture();
        setKernelRunningVisuals(false);
        QMessageBox::warning(m_mainWindow, tr("Kernel Log — Root Required"), reason);
    });

    // Populate the device list with whatever is already connected.
    onDevicesChanged(adb.getConnectedDevices());
}

void UiManager::connectFilterSignals()
{
    // Highlight: typing repaints, Enter and the arrows navigate matches.
    connect(m_ui->txtHighlight,     &QLineEdit::textChanged,   this, &UiManager::onHighlightChanged);
    connect(m_ui->txtHighlight,     &QLineEdit::returnPressed, this, &UiManager::onHighlightNextClicked);
    connect(m_ui->btnHighlightNext, &QPushButton::clicked,     this, &UiManager::onHighlightNextClicked);
    connect(m_ui->btnHighlightPrev, &QPushButton::clicked,     this, &UiManager::onHighlightPrevClicked);

    // Log filters apply on Enter (line edits) or immediately (radios). Looping
    // keeps "every logcat filter shares one handler" obvious and makes adding
    // a field a one-line change.
    const QList<QLineEdit *> filterEdits = {
        m_ui->txtKeyword,   m_ui->txtFindMessage,
        m_ui->txtStartTime, m_ui->txtEndTime,
        m_ui->txtTagFilter, m_ui->txtPackageFilter,
        m_ui->txtPidFilter, m_ui->txtTidFilter,
    };
    for (QLineEdit *edit : filterEdits)
        connect(edit, &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);

    const QList<QRadioButton *> levelRadios = {
        m_ui->radioVerbosePlus,
        m_ui->radioV, m_ui->radioD, m_ui->radioI,
        m_ui->radioW, m_ui->radioE, m_ui->radioA,
    };
    for (QRadioButton *radio : levelRadios)
        connect(radio, &QRadioButton::toggled, this, &UiManager::onFilterChanged);

    // Configuration-tab filters are live (text changed, not Enter), so they
    // are debounced: re-filtering thousands of rows on every keystroke made
    // typing visibly stutter.
    const auto makeDebounce = [this](void (UiManager::*slot)()) {
        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(kConfigFilterDebounceMs);
        connect(timer, &QTimer::timeout, this, slot);
        return timer;
    };
    m_settingsFilterTimer   = makeDebounce(&UiManager::onSettingsFilterChanged);
    m_propertiesFilterTimer = makeDebounce(&UiManager::onPropertiesFilterChanged);

    for (QLineEdit *edit : {m_ui->txtFilterSettings, m_ui->txtFilterSettingsValue})
        connect(edit, &QLineEdit::textChanged, this,
                [this]() { m_settingsFilterTimer->start(); });
    for (QLineEdit *edit : {m_ui->txtFilterProperties, m_ui->txtFilterPropertiesValue})
        connect(edit, &QLineEdit::textChanged, this,
                [this]() { m_propertiesFilterTimer->start(); });

    connect(m_ui->btnRefreshSettings,   &QPushButton::clicked, m_configurationController, &ConfigurationController::onRefreshSettingsClicked);
    connect(m_ui->btnRefreshProperties, &QPushButton::clicked, m_configurationController, &ConfigurationController::onRefreshPropertiesClicked);
}

void UiManager::connectButtonSignals()
{
    // Toolbar / main controls
    connect(m_ui->btnStart,          &QPushButton::clicked, this, &UiManager::onStartClicked);
    connect(m_ui->btnKernel,         &QPushButton::clicked, this, &UiManager::onKernelClicked);
    connect(m_ui->btnClear,          &QPushButton::clicked, this, &UiManager::onClearClicked);
    connect(m_ui->btnSave,           &QPushButton::clicked, this, &UiManager::onSaveFileClicked);
    connect(m_ui->btnAutoScroll,     &QPushButton::toggled, this, &UiManager::onAutoScrollToggled);
    connect(m_ui->btnFitRows,        &QPushButton::clicked, this, &UiManager::onFitRowsClicked);
    connect(m_ui->btnClearAllMarked, &QPushButton::clicked, this, &UiManager::onClearAllMarkedClicked);
    connect(m_ui->btnAppSettings,    &QPushButton::clicked, this, &UiManager::onAppSettingsClicked);

    // Column visibility now lives in App Settings; the old toolbar button stays
    // hidden rather than offering a second, divergent way to do the same thing.
    m_ui->btnColumns->setVisible(false);

    // File I/O
    connect(m_ui->txtFilePath, &QLineEdit::returnPressed, this, &UiManager::onLoadFileClicked);
    connect(m_ui->btnOpen,     &QPushButton::clicked,     this, &UiManager::onOpenFileClicked);

    // SDK
    connect(m_ui->txtPropertySearch,    &QLineEdit::returnPressed, m_configurationController, &ConfigurationController::onAddPropertyDefinition);
    connect(m_ui->btnClearAllProperties,&QPushButton::clicked,     m_configurationController, &ConfigurationController::onClearAllPropertyDefinitions);
    connect(m_ui->btnFetchPropertyDefs, &QPushButton::clicked,     m_configurationController, &ConfigurationController::onRefreshPropertyDefinitionValues);
    connect(m_ui->btnSavePropertySet,   &QPushButton::clicked,     m_configurationController, &ConfigurationController::onSavePropertySet);
    connect(m_ui->btnLoadPropertySet,   &QPushButton::clicked,     m_configurationController, &ConfigurationController::onLoadPropertySet);
    connect(m_ui->btnExportPropertySet, &QPushButton::clicked,     m_configurationController, &ConfigurationController::onExportPropertySet);
    connect(m_ui->btnImportPropertySet, &QPushButton::clicked,     m_configurationController, &ConfigurationController::onImportPropertySet);

    // Device selector
    connect(m_ui->cmbDevice, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UiManager::onDeviceChanged);

    // Default file path: <app dir>/log.log
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const QString appDir = appImage.isEmpty() ? QCoreApplication::applicationDirPath()
                                              : QFileInfo(appImage).absolutePath();
    m_ui->txtFilePath->setText(appDir + QStringLiteral("/log.log"));
}

void UiManager::connectTableSignals()
{
    connectLogTableSignals(m_ui->tableLog, m_ui->tableMarkLog);
}

void UiManager::connectLogTableSignals(QTableView *logTable, QTableView *markTable)
{
    // Shared by pane A (here) and pane B (when the split is built), so the two
    // panes cannot drift apart in what they respond to.
    if (logTable) {
        logTable->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(logTable, &QTableView::customContextMenuRequested,
                this, &UiManager::onTableContextMenu);
        connect(logTable, &QTableView::doubleClicked,
                this, &UiManager::onLogTableDoubleClicked);
        enableTableCopyAction(logTable);
        logTable->viewport()->installEventFilter(m_mainWindow);
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
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Capture-state visuals
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::flashStatus(const QString &message)
{
    if (m_ui && m_ui->statusbar)
        m_ui->statusbar->showMessage(message, kStatusFlashMs);
}

void UiManager::stopCapture()
{
    // Drain whatever the device sent between the last flush and the stop, so
    // the tail of the capture is not silently dropped.
    if (m_batchFlushTimer)
        m_batchFlushTimer->stop();
    flushPendingLines();
    updateStatusBar();
}

void UiManager::setCaptureButtonState(QPushButton *active, QPushButton *other, bool running)
{
    // Styled from the theme sheet via QPushButton[state="recording"]; the
    // button keeps its normal look when idle.
    active->setProperty("state", running ? QStringLiteral("recording") : QVariant());
    active->style()->unpolish(active);
    active->style()->polish(active);

    // Only one capture source can own the log buffer at a time.
    other->setEnabled(!running);
    m_ui->btnFitRows->setEnabled(!running);
}

void UiManager::setLogcatRunningVisuals(bool running)
{
    setCaptureButtonState(m_ui->btnStart, m_ui->btnKernel, running);
}

void UiManager::setKernelRunningVisuals(bool running)
{
    setCaptureButtonState(m_ui->btnKernel, m_ui->btnStart, running);
}
