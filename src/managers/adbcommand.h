#ifndef ADBCOMMAND_H
#define ADBCOMMAND_H

#include <QStringList>


namespace AdbCommand {

inline QStringList listDevices()
{
    return QStringList() << "devices" << "-l";
}

inline QStringList getDeviceModel(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "getprop" << "ro.product.model";
}

inline QStringList startLogcat(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "logcat" << "-v" << "threadtime";
}

inline QStringList startDmesg(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "dmesg" << "-w";
}

inline QStringList clearDmesg(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "dmesg" << "-c";
}

inline QStringList clearLogcat(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "logcat" << "-c";
}

inline QStringList listSettings(const QString &deviceId, const QString &namespace_)
{
    return QStringList() << "-s" << deviceId << "shell" << "settings" << "list" << namespace_;
}

inline QStringList getSetting(const QString &deviceId, const QString &namespace_, const QString &setting)
{
    return QStringList() << "-s" << deviceId << "shell" << "settings" << "get" << namespace_ << setting;
}

inline QStringList putSetting(const QString &deviceId, const QString &namespace_, const QString &setting, const QString &value)
{
    return QStringList() << "-s" << deviceId << "shell" << "settings" << "put" << namespace_ << setting << value;
}

inline QStringList listProperties(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "getprop";
}

inline QStringList getProperty(const QString &deviceId, const QString &property)
{
    return QStringList() << "-s" << deviceId << "shell" << "getprop" << property;
}

inline QStringList setProperty(const QString &deviceId, const QString &property, const QString &value)
{
    return QStringList() << "-s" << deviceId << "shell" << "setprop" << property << value;
}

inline QStringList getPropertyDefinitions(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "configuration_manager" << "get";
}

inline QStringList getPropertyDefinition(const QString &deviceId, const QString &propertyId)
{
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "configuration_manager" << "get" << propertyId;
}

inline QStringList setPropertyDefinition(const QString &deviceId, const QString &propertyId, const QString &value)
{
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "configuration_manager" << "set" << propertyId << value;
}

inline QStringList listDumpsysServices(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "dumpsys" << "-l";
}

// cradle_manager commands: adb shell cmd cradle_manager <args...>
inline QStringList cradleCommand(const QString &deviceId, const QStringList &args)
{
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "cradle_manager" << args;
}

// Reverse port: `adb reverse tcp:<devicePort> tcp:<hostPort>`
// Routes connections from device:devicePort → host:hostPort
inline QStringList reversePort(const QString &deviceId, quint16 devicePort, quint16 hostPort)
{
    return QStringList() << "-s" << deviceId << "reverse"
                         << QString("tcp:%1").arg(devicePort)
                         << QString("tcp:%1").arg(hostPort);
}

// Battery level: `adb shell dumpsys battery`
inline QStringList getBatteryInfo(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "dumpsys" << "battery";
}

// WiFi IP address: `adb shell ip route`
inline QStringList getIpRoute(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "ip" << "route";
}

// Reboot device
inline QStringList rebootDevice(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "reboot";
}

// Reboot to sideload
inline QStringList rebootSideload(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "reboot" << "sideload";
}

// Reboot to bootloader
inline QStringList rebootBootloader(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "reboot" << "bootloader";
}

// Stay awake: settings put global stay_on_while_plugged_in <0|15>
// 15 = stay on for USB+AC+wireless; 0 = off
inline QStringList stayAwake(const QString &deviceId, bool enabled = true)
{
    return QStringList() << "-s" << deviceId << "shell" << "settings" << "put" << "global"
                         << "stay_on_while_plugged_in" << (enabled ? "15" : "0");
}

// Allow mock modem: setprop persist.radio.allow_mock_modem <true|false>
inline QStringList setMockModem(const QString &deviceId, bool enabled)
{
    return QStringList() << "-s" << deviceId << "shell" << "setprop"
                         << "persist.radio.allow_mock_modem" << (enabled ? "true" : "false");
}

// Verify ADB installs: settings put global verifier_verify_adb_installs <0|1>
inline QStringList setVerifyAdbInstalls(const QString &deviceId, bool enabled)
{
    return QStringList() << "-s" << deviceId << "shell" << "settings" << "put" << "global"
                         << "verifier_verify_adb_installs" << (enabled ? "1" : "0");
}

// System locale: setprop persist.sys.locale <locale>
inline QStringList setSystemLocale(const QString &deviceId, const QString &locale)
{
    return QStringList() << "-s" << deviceId << "shell" << "setprop"
                         << "persist.sys.locale" << locale;
}

// Time format 12/24: settings put system time_12_24 <12|24>
inline QStringList setTimeFormat(const QString &deviceId, bool use24h)
{
    return QStringList() << "-s" << deviceId << "shell" << "settings" << "put" << "system"
                         << "time_12_24" << (use24h ? "24" : "12");
}

// Kernel version: cat /proc/version
inline QStringList getKernelVersion(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "cat" << "/proc/version";
}

// WiFi status: settings get global wifi_on
inline QStringList getWifiStatus(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "settings" << "get" << "global" << "wifi_on";
}

// Volume up key event
inline QStringList volumeUp(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "input" << "keyevent" << "KEYCODE_VOLUME_UP";
}

// Volume down key event
inline QStringList volumeDown(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "input" << "keyevent" << "KEYCODE_VOLUME_DOWN";
}

// Connect to WiFi network (Android 11+)
inline QStringList connectWifi(const QString &deviceId, const QString &ssid, const QString &password)
{
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "wifi"
                         << "connect-network" << ssid << "wpa2" << password;
}

// Enable ADB over TCP/IP on device
inline QStringList tcpip(const QString &deviceId, quint16 port = 5555)
{
    return QStringList() << "-s" << deviceId << "tcpip" << QString::number(port);
}

// Connect to device over network
inline QStringList connectDevice(const QString &host, quint16 port = 5555)
{
    return QStringList() << "connect" << QString("%1:%2").arg(host).arg(port);
}

// Restart adbd with root privileges
inline QStringList root(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "root";
}

// Restart adbd without root privileges
inline QStringList unroot(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "unroot";
}

// Reboot to fastboot
inline QStringList rebootFastboot(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "reboot" << "fastboot";
}

// Power key input
inline QStringList powerKey(const QString &deviceId)
{
    return QStringList() << "-s" << deviceId << "shell" << "input" << "keyevent" << "KEYCODE_POWER";
}

} // namespace AdbCommand

#endif // ADBCOMMAND_H
