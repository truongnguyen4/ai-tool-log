#include "socketserver.h"
#include "isocketdatahandler.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QDebug>

SocketServer::SocketServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_port(port)
{
    connect(m_server, &QTcpServer::newConnection,
            this,     &SocketServer::onNewConnection);
}

SocketServer::~SocketServer()
{
    stop();
}

bool SocketServer::start()
{
    if (m_server->isListening())
        return true;

    if (m_server->listen(QHostAddress::LocalHost, m_port)) {
        qDebug() << "SocketServer: listening on port" << m_port;
        return true;
    }

    qWarning() << "SocketServer: failed to listen on port" << m_port
               << "-" << m_server->errorString();
    return false;
}

void SocketServer::stop()
{
    if (m_client) {
        m_client->disconnectFromHost();
        m_client = nullptr;
    }
    m_server->close();
    m_buffer.clear();
}

void SocketServer::disconnectClient()
{
    if (m_client) {
        m_client->disconnectFromHost();
        m_client = nullptr;
    }
    m_buffer.clear();
}

quint16 SocketServer::port() const
{
    return m_port;
}

void SocketServer::registerHandler(ISocketDataHandler *handler)
{
    if (handler && !m_handlers.contains(handler))
        m_handlers.append(handler);
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void SocketServer::onNewConnection()
{
    QTcpSocket *incoming = m_server->nextPendingConnection();
    if (!incoming)
        return;

    // Drop any existing connection — we only serve the latest client.
    if (m_client) {
        m_client->disconnect(this);
        m_client->disconnectFromHost();
    }

    m_client = incoming;
    m_buffer.clear();

    connect(m_client, &QTcpSocket::readyRead,
            this,     &SocketServer::onReadyRead);
    connect(m_client, &QTcpSocket::disconnected,
            this,     &SocketServer::onClientDisconnected);

    qDebug() << "SocketServer: client connected from"
             << m_client->peerAddress().toString();
    emit clientConnected();
}

void SocketServer::onReadyRead()
{
    if (!m_client)
        return;

    m_buffer.append(m_client->readAll());

    // Dispatch every complete newline-terminated message found in the buffer.
    int newlinePos;
    while ((newlinePos = m_buffer.indexOf('\n')) != -1) {
        QByteArray message = m_buffer.left(newlinePos).trimmed();
        m_buffer.remove(0, newlinePos + 1);

        if (!message.isEmpty())
            dispatchMessage(message);
    }
}

void SocketServer::onClientDisconnected()
{
    qDebug() << "SocketServer: client disconnected";
    m_client = nullptr;
    m_buffer.clear();
    emit clientDisconnected();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SocketServer::dispatchMessage(const QByteArray &raw)
{
    // Expected envelope: type:settings,message:<payload>
    // Fall back to passing the whole raw bytes as payload with empty type
    // if the format is not recognised.
    const QString full  = QString::fromUtf8(raw);
    QString type;
    QByteArray payload  = raw;

    // Parse the type field: "type:<value>,message:<rest>"
    if (full.startsWith("type:")) {
        const int commaIdx = full.indexOf(",message:");
        if (commaIdx > 5) {
            type    = full.mid(5, commaIdx - 5).trimmed();
            payload = full.mid(commaIdx + 9).toUtf8(); // 9 == len(",message:")
        }
    }

    for (ISocketDataHandler *handler : std::as_const(m_handlers))
        handler->parseData(type, payload);
}
