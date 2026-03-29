#include "systempropertysockethandler.h"

#include <QDebug>

SystemPropertySocketHandler::SystemPropertySocketHandler(QObject *parent)
    : QObject(parent)
{
}

void SystemPropertySocketHandler::parseData(const QString &type, const QByteArray &message)
{
    if (type.compare("system_property", Qt::CaseInsensitive) != 0)
        return;

    const QString line = QString::fromUtf8(message).trimmed();
    if (line.isEmpty())
        return;

    // Expected payload: property:value
    // The colon is the separator.  The value may itself contain colons.
    const int sep = line.indexOf(':');
    if (sep <= 0) {
        qWarning() << "SystemPropertySocketHandler: invalid format (missing ':'):"
                   << line.left(80);
        return;
    }

    PropertyEntry entry;
    entry.property = line.left(sep).trimmed();
    entry.value    = line.mid(sep + 1);
    entry.line     = entry.property + "=" + entry.value;

    if (entry.property.isEmpty())
        return;

    qDebug() << "SystemPropertySocketHandler: parsed" << entry.property << "=" << entry.value;
    emit propertiesReceived({entry});
}
