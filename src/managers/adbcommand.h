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
    return QStringList() << "-s" << deviceId << "logcat" << "-v" << "time";
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
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "cradle_manager" << "get" << "configuration";
}

inline QStringList getCradleProperty(const QString &deviceId, const QString &propertyId)
{
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "cradle_manager" << "get" << propertyId;
}

inline QStringList setCradleProperty(const QString &deviceId, const QString &propertyId, const QString &value)
{
    return QStringList() << "-s" << deviceId << "shell" << "cmd" << "cradle_manager" << "set" << propertyId << value;
}

} // namespace AdbCommand

#endif // ADBCOMMAND_H
