#include "devicesmanager.h"
#include "adbmanager.h"
#include "adbcommand.h"
#include <QDebug>
#include <QRegularExpression>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

const QString DevicesManager::kUnknownGroup = QStringLiteral("Unknown");

// ---------------------------------------------------------------------------
DevicesManager::DevicesManager(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("ToolLogPro"), QStringLiteral("DeviceGroups"))
{
    loadGroupsFromSettings();

    connect(&AdbManager::instance(), &AdbManager::devicesChanged,
            this, &DevicesManager::onAdbDevicesChanged);

    // Populate from current device list immediately.
    m_connectedDevices = AdbManager::instance().getConnectedDevices();
    reconcileUnknownGroup();
}

// ---------------------------------------------------------------------------
DevicesManager &DevicesManager::instance()
{
    static DevicesManager inst;
    return inst;
}

// ---------------------------------------------------------------------------
QList<AdbDevice> DevicesManager::connectedDevices() const
{
    return m_connectedDevices;
}

// ---------------------------------------------------------------------------
QList<DeviceGroup> DevicesManager::groups() const
{
    QList<DeviceGroup> result;

    // Named groups first (show even if empty so users can drop devices into them).
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        if (it.key() == kUnknownGroup) continue;
        // Collect only currently connected device IDs for this group.
        QStringList present;
        for (const AdbDevice &d : m_connectedDevices)
            if (it.value().contains(d.id))
                present << d.id;
        result.append({ it.key(), present });
    }

    // Unknown group last — devices not assigned to any named group.
    QStringList unknownIds;
    for (const AdbDevice &d : m_connectedDevices) {
        bool assigned = false;
        for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
            if (it.key() == kUnknownGroup) continue;
            if (it.value().contains(d.id)) { assigned = true; break; }
        }
        if (!assigned) unknownIds << d.id;
    }
    if (!unknownIds.isEmpty() || result.isEmpty())
        result.append({ kUnknownGroup, unknownIds });

    return result;
}

// ---------------------------------------------------------------------------
QString DevicesManager::groupForDevice(const QString &deviceId) const
{
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        if (it.key() == kUnknownGroup) continue;
        if (it.value().contains(deviceId))
            return it.key();
    }
    return kUnknownGroup;
}

// ---------------------------------------------------------------------------
void DevicesManager::moveDeviceToGroup(const QString &deviceId, const QString &groupName)
{
    // Remove from all named groups first.
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it)
        it.value().removeAll(deviceId);

    if (groupName != kUnknownGroup) {
        if (!m_groups.contains(groupName))
            m_groups.insert(groupName, {});
        m_groups[groupName].append(deviceId);
    }

    saveGroupsToSettings();
    emit devicesOrGroupsChanged();
}

// ---------------------------------------------------------------------------
void DevicesManager::createGroup(const QString &groupName)
{
    if (groupName.isEmpty() || groupName == kUnknownGroup) return;
    if (m_groups.contains(groupName)) return;

    m_groups.insert(groupName, {});
    saveGroupsToSettings();
    emit devicesOrGroupsChanged();
}

// ---------------------------------------------------------------------------
void DevicesManager::renameGroup(const QString &oldName, const QString &newName)
{
    if (oldName == kUnknownGroup || newName.isEmpty() || newName == kUnknownGroup) return;
    if (!m_groups.contains(oldName) || m_groups.contains(newName)) return;

    QStringList devices = m_groups.take(oldName);
    m_groups.insert(newName, devices);
    saveGroupsToSettings();
    emit devicesOrGroupsChanged();
}

// ---------------------------------------------------------------------------
void DevicesManager::deleteGroup(const QString &groupName)
{
    if (groupName == kUnknownGroup || !m_groups.contains(groupName)) return;
    m_groups.remove(groupName);
    saveGroupsToSettings();
    emit devicesOrGroupsChanged();
}

// ---------------------------------------------------------------------------
void DevicesManager::onAdbDevicesChanged(const QList<AdbDevice> &devices)
{
    m_connectedDevices = devices;
    reconcileUnknownGroup();
    emit devicesOrGroupsChanged();
}

// ---------------------------------------------------------------------------
void DevicesManager::reconcileUnknownGroup()
{
    // Prune stale device IDs from named groups (disconnected devices stay in
    // their groups so re-connecting puts them back automatically).
    // Nothing to prune — we keep the assignment even when offline.
    // Just emit so the UI can reflect the current connection state.
}

// ---------------------------------------------------------------------------
void DevicesManager::fetchDeviceDetails(const QString &deviceId)
{
    const QString adbPath = AdbManager::instance().getAdbPath();

    QtConcurrent::run([this, deviceId, adbPath]() {
        DeviceDetails details;
        details.serial = deviceId;

        // Helper: run a single adb command and return trimmed stdout.
        auto runAdb = [&](const QStringList &args) -> QString {
            QProcess proc;
            proc.start(adbPath, args);
            if (!proc.waitForFinished(3000)) return {};
            return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        };

        // --- Battery level ---
        {
            const QString out = runAdb(AdbCommand::getBatteryInfo(deviceId));
            QRegularExpression re(QStringLiteral("level:\\s*(\\d+)"));
            auto m = re.match(out);
            if (m.hasMatch())
                details.batteryLevel = m.captured(1) + QStringLiteral("%");
        }

        // --- IP address ---
        {
            const QString out = runAdb(AdbCommand::getIpRoute(deviceId));
            QRegularExpression re(QStringLiteral("src\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+)"));
            auto m = re.match(out);
            if (m.hasMatch())
                details.ipAddress = m.captured(1);
        }

        // --- System properties via getprop ---
        auto getProp = [&](const QString &prop) -> QString {
            return runAdb(AdbCommand::getProperty(deviceId, prop));
        };

        details.model           = getProp(QStringLiteral("ro.product.model"));
        details.manufacturer    = getProp(QStringLiteral("ro.product.manufacturer"));
        details.androidVersion  = getProp(QStringLiteral("ro.build.version.release"));
        details.sdkVersion      = getProp(QStringLiteral("ro.build.version.sdk"));
        details.buildFingerprint = getProp(QStringLiteral("ro.build.fingerprint"));
        details.securityPatch   = getProp(QStringLiteral("ro.build.version.security_patch"));
        details.abi             = getProp(QStringLiteral("ro.product.cpu.abi"));
        details.buildNumber     = getProp(QStringLiteral("ro.build.display.id"));

        // --- Kernel version ---
        {
            const QString out = runAdb(AdbCommand::getKernelVersion(deviceId));
            if (!out.isEmpty()) {
                // "Linux version 5.10.157-..." -- take first two words after "Linux version"
                QRegularExpression re(QStringLiteral("Linux version\\s+(\\S+)"));
                auto m = re.match(out);
                details.kernelVersion = m.hasMatch() ? m.captured(1) : out.left(60);
            }
        }

        // --- Network status (WiFi) ---
        {
            const QString out = runAdb(AdbCommand::getWifiStatus(deviceId));
            if (out.trimmed() == QStringLiteral("1"))
                details.networkStatus = QStringLiteral("Connected");
            else
                details.networkStatus = QStringLiteral("Disconnected");
        }

        // Fill empty fields with N/A
        auto fallback = [](QString &s) { if (s.isEmpty()) s = QStringLiteral("N/A"); };
        fallback(details.batteryLevel);
        fallback(details.ipAddress);
        fallback(details.model);
        fallback(details.manufacturer);
        fallback(details.androidVersion);
        fallback(details.sdkVersion);
        fallback(details.buildFingerprint);
        fallback(details.securityPatch);
        fallback(details.kernelVersion);
        fallback(details.abi);
        fallback(details.buildNumber);
        fallback(details.networkStatus);

        emit deviceDetailsFetched(details);
    });
}

// ---------------------------------------------------------------------------
void DevicesManager::loadGroupsFromSettings()
{
    m_groups.clear();
    const QStringList groupNames = m_settings.childGroups();
    for (const QString &name : groupNames) {
        m_settings.beginGroup(name);
        const QStringList ids = m_settings.value(QStringLiteral("devices"))
                                    .toString()
                                    .split(QLatin1Char(','), Qt::SkipEmptyParts);
        m_settings.endGroup();
        m_groups.insert(name, ids);
    }
}

// ---------------------------------------------------------------------------
void DevicesManager::saveGroupsToSettings()
{
    m_settings.clear();
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        if (it.key() == kUnknownGroup) continue;
        m_settings.beginGroup(it.key());
        m_settings.setValue(QStringLiteral("devices"), it.value().join(QLatin1Char(',')));
        m_settings.endGroup();
    }
    m_settings.sync();
}

// ---------------------------------------------------------------------------
void DevicesManager::runAdbCommand(const QStringList &args)
{
    const QString adbPath = AdbManager::instance().getAdbPath();
    QtConcurrent::run([adbPath, args]() {
        QProcess proc;
        proc.start(adbPath, args);
        proc.waitForFinished(10000);
    });
}

// ---------------------------------------------------------------------------
void DevicesManager::enableAdbWireless(const QString &deviceId)
{
    const QString adbPath = AdbManager::instance().getAdbPath();

    QtConcurrent::run([this, adbPath, deviceId]() {
        auto runAdb = [&](const QStringList &args) -> QString {
            QProcess proc;
            proc.start(adbPath, args);
            if (!proc.waitForFinished(5000)) return {};
            return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        };

        // 1) Get device IP address first
        const QString ipOut = runAdb(AdbCommand::getIpRoute(deviceId));
        QRegularExpression re(QStringLiteral("src\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+)"));
        auto m = re.match(ipOut);
        if (!m.hasMatch()) {
            qWarning() << "enableAdbWireless: could not determine device IP";
            return;
        }
        const QString ip = m.captured(1);

        // 2) Switch device to TCP/IP mode
        runAdb(AdbCommand::tcpip(deviceId));

        // 3) Wait for device to restart adbd
        QThread::sleep(2);

        // 4) Connect over network
        const QString result = runAdb(AdbCommand::connectDevice(ip));
        qDebug() << "enableAdbWireless:" << result;
    });
}

// ---------------------------------------------------------------------------
QString DevicesManager::savedWifiSsid() const
{
    return m_settings.value(QStringLiteral("wifi/ssid")).toString();
}

// ---------------------------------------------------------------------------
QString DevicesManager::savedWifiPassword() const
{
    return m_settings.value(QStringLiteral("wifi/password")).toString();
}

// ---------------------------------------------------------------------------
void DevicesManager::saveWifiCredentials(const QString &ssid, const QString &password)
{
    m_settings.setValue(QStringLiteral("wifi/ssid"), ssid);
    m_settings.setValue(QStringLiteral("wifi/password"), password);
    m_settings.sync();
}

// ---------------------------------------------------------------------------
QStringList DevicesManager::listConfigPresets()
{
    QStringList names;
    const int count = m_settings.beginReadArray(QStringLiteral("ConfigPresets"));
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        names << m_settings.value(QStringLiteral("name")).toString();
    }
    m_settings.endArray();
    return names;
}

// ---------------------------------------------------------------------------
bool DevicesManager::saveConfigPreset(const QString &name, const QByteArray &jsonData)
{
    // Load existing presets
    QList<QPair<QString, QByteArray>> presets;
    const int count = m_settings.beginReadArray(QStringLiteral("ConfigPresets"));
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        const QString n = m_settings.value(QStringLiteral("name")).toString();
        const QByteArray d = m_settings.value(QStringLiteral("data")).toByteArray();
        if (n != name) // skip if overwriting
            presets.append({n, d});
    }
    m_settings.endArray();

    // Append new/updated preset
    presets.append({name, jsonData});

    // Write back
    m_settings.beginWriteArray(QStringLiteral("ConfigPresets"), presets.size());
    for (int i = 0; i < presets.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue(QStringLiteral("name"), presets[i].first);
        m_settings.setValue(QStringLiteral("data"), presets[i].second);
    }
    m_settings.endArray();
    m_settings.sync();
    return true;
}

// ---------------------------------------------------------------------------
QByteArray DevicesManager::loadConfigPreset(const QString &name)
{
    const int count = m_settings.beginReadArray(QStringLiteral("ConfigPresets"));
    QByteArray result;
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        if (m_settings.value(QStringLiteral("name")).toString() == name) {
            result = m_settings.value(QStringLiteral("data")).toByteArray();
            break;
        }
    }
    m_settings.endArray();
    return result;
}

// ---------------------------------------------------------------------------
bool DevicesManager::deleteConfigPreset(const QString &name)
{
    QList<QPair<QString, QByteArray>> presets;
    const int count = m_settings.beginReadArray(QStringLiteral("ConfigPresets"));
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        const QString n = m_settings.value(QStringLiteral("name")).toString();
        const QByteArray d = m_settings.value(QStringLiteral("data")).toByteArray();
        if (n != name)
            presets.append({n, d});
    }
    m_settings.endArray();

    m_settings.beginWriteArray(QStringLiteral("ConfigPresets"), presets.size());
    for (int i = 0; i < presets.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue(QStringLiteral("name"), presets[i].first);
        m_settings.setValue(QStringLiteral("data"), presets[i].second);
    }
    m_settings.endArray();
    m_settings.sync();
    return true;
}
