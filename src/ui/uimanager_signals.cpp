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
#include "packageresolver.h"

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
/** A wait shorter than this is only the adb round trip, not worth a note in the log. */
constexpr qint64 kNoticeableWaitMs = 1500;
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
    connect(&adb, &AdbManager::propertyDefinitionsWritten, m_configurationController, &ConfigurationController::onPropertyDefinitionsWritten);
    connect(&adb, &AdbManager::settingSaveResult,          m_configurationController, &ConfigurationController::onSettingSaveResult);
    connect(&adb, &AdbManager::propertySaveResult,         m_configurationController, &ConfigurationController::onPropertySaveResult);

    // Both captures report the same way: waiting for their device, streaming
    // again, and every gap marked in the log itself.
    using CaptureKind = AdbManager::CaptureKind;
    const auto noteWaiting = [this](const QString &serial, bool reconnecting, CaptureKind kind) {
        m_captureWaitClock.start();
        updateCaptureButtons();
        if (reconnecting)
            appendCaptureNote(tr("%1 disconnected, waiting for it to come back").arg(serial), kind);
        flashStatus(tr("Waiting for %1…").arg(serial));
    };
    const auto noteStreaming = [this](const QString &serial, bool reconnected, bool newBoot,
                                      CaptureKind kind, const QString &started) {
        updateCaptureButtons();
        if (reconnected) {
            appendCaptureNote(newBoot ? tr("%1 is back after a reboot, its boot log follows").arg(serial)
                                      : tr("%1 is back, the log continues").arg(serial), kind);
        } else if (m_captureWaitClock.isValid() && m_captureWaitClock.elapsed() > kNoticeableWaitMs) {
            appendCaptureNote(tr("%1 came online, the capture started").arg(serial), kind);
        }
        flashStatus(started);
    };

    const auto capturingChanged = [this](bool capturing) {
        updateCaptureButtons();
        // The process table only needs watching while lines are arriving.
        m_packageResolver->setPolling(capturing);
    };
    connect(&adb, &AdbManager::logcatStarted, this, [capturingChanged]() { capturingChanged(true); });
    connect(&adb, &AdbManager::logcatWaitingForDevice, this,
            [noteWaiting](const QString &serial, bool reconnecting) {
        noteWaiting(serial, reconnecting, CaptureKind::Logcat);
    });
    connect(&adb, &AdbManager::logcatStreaming, this,
            [this, noteStreaming](const QString &serial, bool reconnected, bool newBoot) {
        noteStreaming(serial, reconnected, newBoot, CaptureKind::Logcat, tr("Logcat started"));
    });
    connect(&adb, &AdbManager::logcatStopped, this, [this, capturingChanged]() {
        stopCapture();
        capturingChanged(false);
        flashStatus(tr("Logcat stopped"));
    });

    connect(&adb, &AdbManager::dmesgStarted, this, [capturingChanged]() { capturingChanged(true); });
    connect(&adb, &AdbManager::dmesgWaitingForDevice, this,
            [noteWaiting](const QString &serial, bool reconnecting) {
        noteWaiting(serial, reconnecting, CaptureKind::Kernel);
    });
    connect(&adb, &AdbManager::dmesgStreaming, this,
            [this, noteStreaming](const QString &serial, bool reconnected, bool newBoot) {
        noteStreaming(serial, reconnected, newBoot, CaptureKind::Kernel,
                      tr("Kernel log started (adb shell dmesg -w)"));
    });
    connect(&adb, &AdbManager::dmesgStopped, this, [this, capturingChanged]() {
        stopCapture();
        capturingChanged(false);
        flashStatus(tr("Kernel log stopped"));
    });
    connect(&adb, &AdbManager::dmesgFailed, this, [this](const QString &reason) {
        stopCapture();
        updateCaptureButtons();
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
        m_ui->txtLogQuery, m_ui->txtStartTime, m_ui->txtEndTime,
    };
    for (QLineEdit *edit : filterEdits)
        connect(edit, &QLineEdit::returnPressed, this, &UiManager::onFilterChanged);

    // The line under the filter box flags edits not applied yet. Emptying the
    // box — which its clear button does without an Enter — applies at once:
    // showing every line is cheap, and a filter the box no longer shows would
    // only confuse.
    connect(m_ui->txtLogQuery, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.trimmed().isEmpty()
            && !m_logFilterController->criteria().query.text().trimmed().isEmpty())
            applyFilters();
        else
            updateQueryHint();
    });

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

    const auto wireLiveFilter = [this](QLineEdit *edit, QTimer *timer,
                                       void (UiManager::*apply)()) {
        connect(edit, &QLineEdit::textChanged, timer, qOverload<>(&QTimer::start));
        // Enter applies at once, without waiting out the debounce.
        connect(edit, &QLineEdit::returnPressed, this, [this, timer, apply]() {
            timer->stop();
            (this->*apply)();
        });
    };
    wireLiveFilter(m_ui->txtFilterSettings,   m_settingsFilterTimer,
                   &UiManager::onSettingsFilterChanged);
    wireLiveFilter(m_ui->txtFilterProperties, m_propertiesFilterTimer,
                   &UiManager::onPropertiesFilterChanged);

    // The row count beside each box follows every model reset: a filter, a
    // fetch and a device change alike.
    connect(m_settingsModel,   &QAbstractItemModel::modelReset,
            this, &UiManager::updateSettingsFilterStatus);
    connect(m_propertiesModel, &QAbstractItemModel::modelReset,
            this, &UiManager::updatePropertiesFilterStatus);
    updateSettingsFilterStatus();
    updatePropertiesFilterStatus();

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

    // SDK tab buttons are wired by ConfigurationController::setupSDKTab().

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

