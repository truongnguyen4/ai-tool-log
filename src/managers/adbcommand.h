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

} // namespace AdbCommand

#endif // ADBCOMMAND_H
