#ifndef DEVICESMANAGER_H
#define DEVICESMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QSettings>
#include <QProcess>

#include "adbmanager.h"
#include "qsettingspresetstore.h"

class PresetStore;

// ---------------------------------------------------------------------------
// DeviceGroup — a named group that holds device serial IDs.
// ---------------------------------------------------------------------------
struct DeviceGroup {
    QString      name;
    QStringList  deviceIds;   // serial IDs belonging to this group
};

// ---------------------------------------------------------------------------
// DeviceDetails — runtime info fetched from a connected device.
// ---------------------------------------------------------------------------
struct DeviceDetails {
    QString serial;
    QString batteryLevel;   // e.g. "85%"
    QString ipAddress;      // e.g. "192.168.1.15"
    // System properties from getprop
    QString model;          // ro.product.model
    QString manufacturer;   // ro.product.manufacturer
    QString androidVersion; // ro.build.version.release
    QString sdkVersion;     // ro.build.version.sdk
    QString buildFingerprint; // ro.build.fingerprint
    QString securityPatch;  // ro.build.version.security_patch
    QString kernelVersion;  // parsed from /proc/version
    QString abi;            // ro.product.cpu.abi
    QString buildNumber;    // ro.build.display.id
    QString networkStatus;  // active network type (e.g. "WIFI", "MOBILE", "NONE")
};

// ---------------------------------------------------------------------------
// DevicesManager — owns device grouping logic and persists groups via
// QSettings.  Device discovery is delegated to AdbManager so it can be
// swapped out without touching this class.
//
// Group assignments are stored as:
//   DeviceGroups/<groupName>/devices = <comma-separated list of serial IDs>
//
// Any connected device not assigned to a named group lives in "Unknown".
// ---------------------------------------------------------------------------
class DevicesManager : public QObject
{
    Q_OBJECT

public:
    static const QString kUnknownGroup;

    static DevicesManager &instance();

    DevicesManager(const DevicesManager &) = delete;
    DevicesManager &operator=(const DevicesManager &) = delete;

    // Returns current connected devices (raw AdbDevice list).
    QList<AdbDevice> connectedDevices() const;

    // Returns all groups (including "Unknown" at the end if needed).
    QList<DeviceGroup> groups() const;

    // Returns the group name for a given device serial ID.
    QString groupForDevice(const QString &deviceId) const;

    // Moves a device to the specified group. Creates the group if needed.
    // Pass kUnknownGroup to reset a device to the default group.
    void moveDeviceToGroup(const QString &deviceId, const QString &groupName);

    // Creates a new empty group. No-op if it already exists.
    void createGroup(const QString &groupName);

    // Renames a group. No-op if oldName doesn't exist or newName already exists.
    void renameGroup(const QString &oldName, const QString &newName);

    // Deletes a group; devices in it are moved to kUnknownGroup.
    void deleteGroup(const QString &groupName);

    // Fetches battery level and IP address for a device asynchronously.
    // Emits deviceDetailsFetched when done.
    void fetchDeviceDetails(const QString &deviceId);

    // Execute an ADB command (QStringList args) asynchronously.
    void runAdbCommand(const QStringList &args);

    // Enable ADB over WiFi: runs tcpip then connect using device IP.
    void enableAdbWireless(const QString &deviceId);

    // WiFi credential storage (single SSID/password pair).
    QString savedWifiSsid() const;
    QString savedWifiPassword() const;
    void saveWifiCredentials(const QString &ssid, const QString &password);

    // Configuration preset storage. Backed by QSettingsPresetStore.
    QStringList listConfigPresets();
    bool saveConfigPreset(const QString &name, const QByteArray &jsonData);
    QByteArray loadConfigPreset(const QString &name);
    bool deleteConfigPreset(const QString &name);

    /** Underlying preset store (for use with shared PresetDialogs). */
    PresetStore &configPresetStore();

signals:
    // Emitted whenever the device list or any group assignment changes.
    void devicesOrGroupsChanged();
    // Emitted when battery/IP info for a device is ready.
    void deviceDetailsFetched(const DeviceDetails &details);

private:
    explicit DevicesManager(QObject *parent = nullptr);

    void loadGroupsFromSettings();
    void saveGroupsToSettings();
    void onAdbDevicesChanged(const QList<AdbDevice> &devices);

    // Ensures all connected device IDs that have no group are placed in
    // kUnknownGroup.
    void reconcileUnknownGroup();

    QList<AdbDevice>            m_connectedDevices;
    QMap<QString, QStringList>  m_groups;     // groupName -> list of deviceIds
    QSettings                   m_settings;
    QSettingsPresetStore        m_configPresets;
};

#endif // DEVICESMANAGER_H
