// UiManager: the device selector and the logcat capture controls.
//
// The selector lists every device seen, online or not. A chosen device that
// goes away — rebooting, unplugged — stays chosen, so Logcat can wait for it
// and read its log the moment it is back.
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "colorscheme.h"
#include "configurationcontroller.h"
#include "dumpsyscontroller.h"
#include "packageresolver.h"
#include "tooltips.h"
#include "components/components.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>

#include <algorithm>

namespace {

constexpr auto kKnownDevicesKey   = "Devices/known";
constexpr auto kSelectedDeviceKey = "Devices/selected";
constexpr auto kFollowRebootsKey  = "Logcat/followReboots";
constexpr int  kMaxKnownDevices   = 12;
/** Item role naming the action of the selector's non-device entries. */
constexpr int  kDeviceActionRole  = Qt::UserRole + 1;
constexpr auto kActionAdd         = "add";
constexpr auto kActionForget      = "forget";
constexpr int  kStatusDotSize     = 10;

/** The model part of AdbDevice::name, "Joya Smart (65c361ce)"; empty when there is none. */
QString modelName(const AdbDevice &device)
{
    const QString suffix = QStringLiteral(" (%1)").arg(device.id);
    const QString model = device.name.endsWith(suffix) ? device.name.chopped(suffix.size())
                                                       : device.name;
    return model == device.id ? QString() : model;
}

/** A filled dot for an online device, a ring for one that is away. */
QIcon statusIcon(const QColor &color, bool filled)
{
    constexpr qreal kScale = 2.0;   // drawn at twice the size for high-DPI screens
    QPixmap pixmap(int(kStatusDotSize * kScale), int(kStatusDotSize * kScale));
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF dot(3, 3, pixmap.width() - 6, pixmap.height() - 6);
    if (filled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
    } else {
        painter.setPen(QPen(color, 2.5));
        painter.setBrush(Qt::NoBrush);
    }
    painter.drawEllipse(dot);
    painter.end();
    pixmap.setDevicePixelRatio(kScale);
    return QIcon(pixmap);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Remembered devices
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::loadKnownDevices()
{
    const QSettings settings;
    m_knownDevices.clear();
    const QStringList entries = settings.value(QLatin1String(kKnownDevicesKey)).toStringList();
    for (const QString &entry : entries) {
        const QString serial = entry.section(QLatin1Char('\t'), 0, 0).trimmed();
        if (!serial.isEmpty())
            m_knownDevices.append({serial, entry.section(QLatin1Char('\t'), 1)});
    }
    m_selectedSerial = settings.value(QLatin1String(kSelectedDeviceKey)).toString();
}

void UiManager::saveKnownDevices() const
{
    QStringList entries;
    for (const KnownDevice &device : m_knownDevices)
        entries << device.serial + QLatin1Char('\t') + device.model;
    QSettings().setValue(QLatin1String(kKnownDevicesKey), entries);
}

void UiManager::rememberDevices(const QList<AdbDevice> &devices)
{
    bool changed = false;
    for (const AdbDevice &device : devices) {
        const QString model = modelName(device);
        const auto known = std::find_if(m_knownDevices.begin(), m_knownDevices.end(),
                                        [&](const KnownDevice &k) { return k.serial == device.id; });
        if (known == m_knownDevices.end()) {
            m_knownDevices.prepend({device.id, model});
            changed = true;
        } else if (!model.isEmpty() && known->model != model) {
            known->model = model;
            changed = true;
        }
    }

    // Past the limit, forget the longest-known devices that are away.
    while (m_knownDevices.size() > kMaxKnownDevices) {
        const auto oldestAway = std::find_if(
            m_knownDevices.rbegin(), m_knownDevices.rend(), [this](const KnownDevice &k) {
                return !m_onlineSerials.contains(k.serial) && k.serial != m_selectedSerial;
            });
        if (oldestAway == m_knownDevices.rend())
            break;
        m_knownDevices.erase(std::next(oldestAway).base());
        changed = true;
    }

    if (changed)
        saveKnownDevices();
}

void UiManager::addDeviceBySerial()
{
    bool ok = false;
    const QString serial = QInputDialog::getText(
        m_mainWindow, tr("Add Device"),
        tr("Serial number, as `adb devices` lists it.\n"
           "A network device (host:port) must be connected with `adb connect` first."),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || serial.isEmpty())
        return;
    if (serial.contains(QRegularExpression(QStringLiteral("\\s")))) {
        flashStatus(tr("A serial number has no spaces"));
        return;
    }

    const bool known = std::any_of(m_knownDevices.cbegin(), m_knownDevices.cend(),
                                   [&](const KnownDevice &k) { return k.serial == serial; });
    if (!known) {
        m_knownDevices.prepend({serial, QString()});
        saveKnownDevices();
    }
    m_selectedSerial = serial;
    m_deviceChoicePinned = true;
    QSettings().setValue(QLatin1String(kSelectedDeviceKey), serial);
    rebuildDeviceSelector();
    setActiveDevice(m_onlineSerials.contains(serial) ? serial : QString());
}

void UiManager::forgetOfflineDevices()
{
    const AdbManager &adb = AdbManager::instance();
    // The device a running capture waits for is not forgotten.
    const QString capturing = adb.isLogcatRunning() ? adb.logcatSerial() : QString();
    m_knownDevices.erase(
        std::remove_if(m_knownDevices.begin(), m_knownDevices.end(),
                       [&](const KnownDevice &k) {
                           return !m_onlineSerials.contains(k.serial) && k.serial != capturing;
                       }),
        m_knownDevices.end());
    saveKnownDevices();

    if (!m_onlineSerials.contains(m_selectedSerial) && m_selectedSerial != capturing) {
        m_selectedSerial = m_knownDevices.isEmpty() ? QString() : m_knownDevices.first().serial;
        m_deviceChoicePinned = false;
    }
    rebuildDeviceSelector();
    setActiveDevice(m_onlineSerials.contains(m_selectedSerial) ? m_selectedSerial : QString());
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Selector
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::rebuildDeviceSelector()
{
    QComboBox *combo = m_ui->cmbDevice;
    const QSignalBlocker blocker(combo);
    const ColorScheme &colors = ColorScheme::instance();
    combo->clear();

    const auto addDevice = [&](const KnownDevice &device, bool online) {
        QString text = device.model.isEmpty()
                           ? device.serial
                           : QStringLiteral("%1  ·  %2").arg(device.model, device.serial);
        if (!online)
            text += tr("  —  offline");
        combo->addItem(statusIcon(online ? colors.success() : colors.mutedText(), online),
                       text, device.serial);
        combo->setItemData(combo->count() - 1,
                           online ? tr("Connected")
                                  : tr("Not connected. Logcat can wait for it to come online."),
                           Qt::ToolTipRole);
    };

    // Connected devices first, then the ones that are away.
    bool anyAway = false;
    for (const KnownDevice &device : std::as_const(m_knownDevices)) {
        if (m_onlineSerials.contains(device.serial))
            addDevice(device, true);
    }
    for (const KnownDevice &device : std::as_const(m_knownDevices)) {
        if (!m_onlineSerials.contains(device.serial)) {
            addDevice(device, false);
            anyAway = true;
        }
    }
    if (combo->count() == 0)
        combo->addItem(tr("No devices found"), QString());

    combo->insertSeparator(combo->count());
    combo->addItem(tr("Add device by serial…"));
    combo->setItemData(combo->count() - 1, QLatin1String(kActionAdd), kDeviceActionRole);
    if (anyAway) {
        combo->addItem(tr("Forget offline devices"));
        combo->setItemData(combo->count() - 1, QLatin1String(kActionForget), kDeviceActionRole);
    }

    const int index = m_selectedSerial.isEmpty() ? -1 : combo->findData(m_selectedSerial);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void UiManager::onDevicesChanged(const QList<AdbDevice> &devices)
{
    m_onlineSerials.clear();
    for (const AdbDevice &device : devices)
        m_onlineSerials.insert(device.id);
    rememberDevices(devices);

    // A device the user picked stays picked while it is away. Otherwise the
    // selection follows what is plugged in.
    if (!m_onlineSerials.contains(m_selectedSerial) && !m_deviceChoicePinned) {
        if (!devices.isEmpty())
            m_selectedSerial = devices.first().id;
        else if (m_selectedSerial.isEmpty() && !m_knownDevices.isEmpty())
            m_selectedSerial = m_knownDevices.first().serial;
    }

    rebuildDeviceSelector();
    setActiveDevice(m_onlineSerials.contains(m_selectedSerial) ? m_selectedSerial : QString());
}

void UiManager::onDeviceChanged(int index)
{
    QComboBox *combo = m_ui->cmbDevice;
    const QString action = combo->itemData(index, kDeviceActionRole).toString();
    if (!action.isEmpty()) {
        {
            // An action is not a selection: put the device back at once.
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(qMax(0, combo->findData(m_selectedSerial)));
        }
        // Run it once the popup has closed.
        QTimer::singleShot(0, this, [this, action]() {
            if (action == QLatin1String(kActionAdd))
                addDeviceBySerial();
            else
                forgetOfflineDevices();
        });
        return;
    }

    const QString serial = combo->itemData(index).toString();
    if (serial.isEmpty())
        return;
    m_selectedSerial = serial;
    m_deviceChoicePinned = true;
    QSettings().setValue(QLatin1String(kSelectedDeviceKey), serial);

    const bool online = m_onlineSerials.contains(serial);
    setActiveDevice(online ? serial : QString());
    if (!online)
        flashStatus(tr("%1 is not connected. Press Logcat to wait for it.").arg(serial));
}

void UiManager::setActiveDevice(const QString &deviceId)
{
    setDeviceStatusConnected(!deviceId.isEmpty());
    if (deviceId == m_currentDeviceId)
        return;

    // Monitors poll the device they were started on.
    if (m_configurationController)
        m_configurationController->stopAllMonitors();
    m_currentDeviceId = deviceId;
    AdbManager::instance().setCurrentDeviceId(deviceId);
    m_packageResolver->setDevice(deviceId);

    if (deviceId.isEmpty()) {
        // Drop device-specific data so no stale values are shown as current.
        m_settingsModel->setSettings({});
        m_propertiesModel->setProperties({});
        m_configurationController->clearPropertyDefinitions();
        m_ui->txtDumpsysCmdResult->clear();
        if (m_dumpsysController)
            m_dumpsysController->clearServices();
        m_ui->txtDumpsysService->clear();
        m_ui->txtDumpsysService->setPlaceholderText(tr("Service name (e.g. activity)"));
        if (m_dumpsysController)
            m_dumpsysController->refreshCommandText();
        return;
    }

    m_ui->statusbar->showMessage(tr("Selected device: %1").arg(deviceId), 2000);
    AdbManager &adb = AdbManager::instance();
    if (m_dumpsysController)
        m_dumpsysController->clearServices();
    const QString previousService = m_ui->txtDumpsysService->text().trimmed();
    m_ui->txtDumpsysService->setPlaceholderText(tr("Loading services..."));
    adb.fetchDumpsysList(deviceId);

    // Reload the configuration tables so they show this device's values.
    adb.fetchSettings(deviceId);
    adb.fetchProperties(deviceId);
    m_configurationController->refreshPropertyDefinitions();

    // An empty service field gets the service last used on this device.
    if (previousService.isEmpty() && m_dumpsysController)
        m_dumpsysController->restoreLastService(deviceId);
    const QString service = m_ui->txtDumpsysService->text().trimmed();
    if (!service.isEmpty()) {
        m_ui->txtDumpsysCmdResult->setPlainText(QStringLiteral("..."));
        adb.fetchDumpsys(deviceId, service);
    }
    if (m_dumpsysController)
        m_dumpsysController->refreshCommandText();
}

void UiManager::setDeviceStatusConnected(bool connected)
{
    QLabel *label = m_ui->lblDeviceStatus;
    if (!label)
        return;
    // Styled from the theme sheet via QLabel#lblDeviceStatus[state=...].
    label->setProperty("state", connected ? QStringLiteral("connected")
                                          : QStringLiteral("disconnected"));
    label->setToolTip(connected                   ? tr("Connected")
                      : m_selectedSerial.isEmpty() ? tr("No device")
                                                   : tr("%1 is not connected").arg(m_selectedSerial));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Logcat capture controls
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::setupCaptureControls()
{
    using namespace UiComponents;

    m_btnFollowReboots = Button::make(tr("↻ Follow reboots"), ButtonVariant::Secondary,
                                      m_ui->btnStart->parentWidget(), ButtonSize::Medium);
    m_btnFollowReboots->setObjectName(QStringLiteral("btnFollowReboots"));
    m_btnFollowReboots->setCheckable(true);
    m_btnFollowReboots->setToolTip(tr(Tooltips::btnFollowReboots));
    m_btnFollowReboots->setChecked(QSettings().value(QLatin1String(kFollowRebootsKey), true).toBool());
    if (QHBoxLayout *toolbar = m_ui->horizontalLayout_3) {
        const int index = toolbar->indexOf(m_ui->btnStart);
        toolbar->insertWidget(index + 1, m_btnFollowReboots);
    }
    connect(m_btnFollowReboots, &QPushButton::toggled, this, [](bool on) {
        QSettings().setValue(QLatin1String(kFollowRebootsKey), on);
        AdbManager::instance().setCaptureFollowReboots(on);   // the running captures too
    });

    // The status dots are painted, so they follow a theme change by hand.
    connect(&ColorScheme::instance(), &ColorScheme::modeChanged,
            this, &UiManager::rebuildDeviceSelector);

    updateCaptureButtons();
}

void UiManager::updateCaptureButtons()
{
    const AdbManager &adb = AdbManager::instance();
    const bool logcatRunning = adb.isLogcatRunning();
    const bool kernelRunning = adb.isDmesgRunning();

    const auto apply = [](QPushButton *button, const QString &idleText, const QString &idleTip,
                          bool running, bool waiting, const QString &serial) {
        button->setText(waiting ? tr("Waiting…") : idleText);
        if (waiting)
            button->setToolTip(tr("Waiting for %1. The capture starts the moment it is online.\n"
                                  "Click to stop waiting.").arg(serial));
        else if (running)
            button->setToolTip(tr("Capturing from %1.\nClick to stop.").arg(serial));
        else
            button->setToolTip(idleTip);

        // Styled from the theme sheet via QPushButton#btnX[state=...].
        const QVariant state = waiting ? QVariant(QStringLiteral("waiting"))
                               : running ? QVariant(QStringLiteral("recording"))
                                         : QVariant();
        if (button->property("state") == state)
            return;
        button->setProperty("state", state);
        button->style()->unpolish(button);
        button->style()->polish(button);
    };

    apply(m_ui->btnStart, tr("Logcat"), tr(Tooltips::btnStart),
          logcatRunning, adb.isLogcatWaiting(), adb.logcatSerial());
    apply(m_ui->btnKernel, tr("Kernel"), tr(Tooltips::btnKernel),
          kernelRunning, adb.isDmesgWaiting(), adb.dmesgSerial());

    // Both captures fill the same buffer, so only one runs at a time.
    m_ui->btnStart->setEnabled(!kernelRunning);
    m_ui->btnKernel->setEnabled(!logcatRunning);
    m_ui->btnFitRows->setEnabled(!logcatRunning && !kernelRunning);
}

void UiManager::appendCaptureNote(const QString &text, AdbManager::CaptureKind kind)
{
    // A line in the log's own format, so it lands in the table like any other,
    // under the tag ToolLogPro.
    if (kind == AdbManager::CaptureKind::Kernel) {
        // The kernel counts seconds since boot; carry on from the last line.
        const QString seconds = AdbManager::instance().captureStamp(kind);
        onLogLineReceived(QStringLiteral("[%1] [ToolLogPro] ━━━ %2 ━━━")
                              .arg(seconds.isEmpty() ? QStringLiteral("0.000000") : seconds, text));
        return;
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("MM-dd HH:mm:ss.zzz"));
    onLogLineReceived(QStringLiteral("%1     0     0 I ToolLogPro: ━━━ %2 ━━━").arg(stamp, text));
}
