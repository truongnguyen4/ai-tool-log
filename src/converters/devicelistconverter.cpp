#include "devicelistconverter.h"

#include <QRegularExpression>
#include <QStringList>

QList<AdbDevice> DeviceListConverter::convert(const QString &output)
{
    QList<AdbDevice> devices;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    static const QRegularExpression kWhitespace(QStringLiteral("\\s+"));
    static const QRegularExpression kModel(QStringLiteral("model:([^\\s]+)"));

    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("List of devices"))
            || line.trimmed().isEmpty())
            continue;

        const QStringList parts = line.split(kWhitespace, Qt::SkipEmptyParts);
        if (parts.size() < 2)
            continue;

        AdbDevice device;
        device.id       = parts[0];
        device.isOnline = (parts[1] == QLatin1String("device"));

        const QRegularExpressionMatch m = kModel.match(line);
        const QString friendly = m.hasMatch()
            ? m.captured(1).replace('_', ' ')
            : device.id;
        device.name = QString("%1 (%2)").arg(friendly, device.id);

        if (device.isOnline)
            devices.append(device);
    }

    return devices;
}
