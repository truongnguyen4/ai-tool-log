#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <QObject>
#include <QList>

class QTcpServer;
class QTcpSocket;
class ISocketDataHandler;

/**
 * SocketServer
 *
 * A TCP server that listens on a host port for a single client connection
 * (the Android device connects via `adb reverse tcp:<devicePort> tcp:<hostPort>`).
 *
 * When a complete newline-delimited message arrives it dispatches the raw
 * bytes to every registered ISocketDataHandler.  Handlers that recognise the
 * payload parse it; others ignore it.
 *
 * Protocol: messages are newline ('\n') delimited.  Each message is a single
 * JSON object on one line.
 */
class SocketServer : public QObject
{
    Q_OBJECT

public:
    explicit SocketServer(quint16 port, QObject *parent = nullptr);
    ~SocketServer();

    bool start();
    void stop();
    void disconnectClient();  // Drop current client; server keeps listening
    quint16 port() const;

    /**
     * Register a handler that will receive every complete message.
     * Ownership stays with the caller; the handler must outlive SocketServer.
     */
    void registerHandler(ISocketDataHandler *handler);

signals:
    void clientConnected();
    void clientDisconnected();
    void errorOccurred(const QString &error);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    void dispatchMessage(const QByteArray &message);

    QTcpServer                 *m_server   = nullptr;
    QTcpSocket                 *m_client   = nullptr;
    quint16                     m_port;
    QByteArray                  m_buffer;
    QList<ISocketDataHandler *> m_handlers;
};

#endif // SOCKETSERVER_H
