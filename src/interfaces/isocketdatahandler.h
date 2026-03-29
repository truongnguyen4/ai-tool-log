#ifndef ISOCKETDATAHANDLER_H
#define ISOCKETDATAHANDLER_H

#include <QByteArray>
#include <QString>

/**
 * Interface for socket data handlers.
 *
 * SocketServer parses each newline-delimited message into a type and payload,
 * then forwards both to every registered handler.  Handlers that recognise
 * the type act on the payload; others ignore it.
 *
 * Wire format:  type:settings,message:airplane_mode_on:1
 */
class ISocketDataHandler
{
public:
    virtual ~ISocketDataHandler() = default;
    virtual void parseData(const QString &type, const QByteArray &message) = 0;
};

#endif // ISOCKETDATAHANDLER_H
