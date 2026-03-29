#include "propertydefinitionsockethandler.h"

#include <QDebug>

PropertyDefinitionSocketHandler::PropertyDefinitionSocketHandler(QObject *parent)
    : QObject(parent)
{
}

void PropertyDefinitionSocketHandler::parseData(const QString &type, const QByteArray &message)
{
    if (type.compare("property_definition", Qt::CaseInsensitive) != 0)
        return;

    const QString line = QString::fromUtf8(message).trimmed();
    if (line.isEmpty())
        return;

    // Expected payload: id:value
    // The colon is the separator.  The value may itself contain colons.
    const int sep = line.indexOf(':');
    if (sep <= 0) {
        qWarning() << "PropertyDefinitionSocketHandler: invalid format (missing ':'):"
                   << line.left(80);
        return;
    }

    const QString id    = line.left(sep).trimmed();
    const QString value = line.mid(sep + 1);

    if (id.isEmpty())
        return;

    qDebug() << "PropertyDefinitionSocketHandler: parsed id=" << id << "value=" << value;
    emit propertyDefinitionReceived(id, value);
}
