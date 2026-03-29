#ifndef PROPERTYDEFINITIONSOCKETHANDLER_H
#define PROPERTYDEFINITIONSOCKETHANDLER_H

#include <QObject>
#include <QString>
#include "isocketdatahandler.h"
#include "propertydefinition.h"

/**
 * PropertyDefinitionSocketHandler
 *
 * Handles socket messages of type "property_definition".
 *
 * Expected payload format: <id>:<value>
 *   1234:1
 *   5678:enabled
 *
 * Emits propertyDefinitionReceived() with the id and new value.
 */
class PropertyDefinitionSocketHandler : public QObject, public ISocketDataHandler
{
    Q_OBJECT

public:
    explicit PropertyDefinitionSocketHandler(QObject *parent = nullptr);

    void parseData(const QString &type, const QByteArray &message) override;

signals:
    void propertyDefinitionReceived(const QString &id, const QString &value);
};

#endif // PROPERTYDEFINITIONSOCKETHANDLER_H
