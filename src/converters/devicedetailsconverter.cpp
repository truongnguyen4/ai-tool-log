#include "devicedetailsconverter.h"

#include <QRegularExpression>

QString DeviceDetailsConverter::convertBatteryLevel(const QString &dumpsysBatteryOutput)
{
    static const QRegularExpression re(QStringLiteral("level:\\s*(\\d+)"));
    const QRegularExpressionMatch m = re.match(dumpsysBatteryOutput);
    return m.hasMatch() ? m.captured(1) + QStringLiteral("%") : QString();
}

QString DeviceDetailsConverter::convertIpFromRoute(const QString &ipRouteOutput)
{
    static const QRegularExpression re(QStringLiteral("src\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+)"));
    const QRegularExpressionMatch m = re.match(ipRouteOutput);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString DeviceDetailsConverter::convertKernelVersion(const QString &procVersionOutput)
{
    if (procVersionOutput.isEmpty())
        return QString();
    static const QRegularExpression re(QStringLiteral("Linux version\\s+(\\S+)"));
    const QRegularExpressionMatch m = re.match(procVersionOutput);
    return m.hasMatch() ? m.captured(1) : procVersionOutput.left(60);
}

QString DeviceDetailsConverter::convertWifiStatus(const QString &wifiStatusOutput)
{
    return wifiStatusOutput.trimmed() == QLatin1String("1")
        ? QStringLiteral("Connected")
        : QStringLiteral("Disconnected");
}
