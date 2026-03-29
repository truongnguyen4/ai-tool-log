#ifndef SYSTEMPROPERTYSOCKETHANDLER_H
#define SYSTEMPROPERTYSOCKETHANDLER_H

#include <QObject>
#include <QVector>
#include "isocketdatahandler.h"
#include "propertyentry.h"

/**
 * SystemPropertySocketHandler
 *
 * Handles socket messages of type "system_property".
 *
 * Expected payload format (one entry per message):
 *   ro.build.version.release:13
 *   persist.sys.locale:en-US
 *
 * Emits propertiesReceived() on successful parse.
 */
class SystemPropertySocketHandler : public QObject, public ISocketDataHandler
{
    Q_OBJECT

public:
    explicit SystemPropertySocketHandler(QObject *parent = nullptr);

    void parseData(const QString &type, const QByteArray &message) override;

signals:
    void propertiesReceived(const QVector<PropertyEntry> &properties);
};

#endif // SYSTEMPROPERTYSOCKETHANDLER_H
