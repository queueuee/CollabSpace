#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QSet>
#include <QHash>

class ChatServer : public QObject {
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer();

    bool startServer(quint16 tcpPort, quint16 udpPort);

private slots:
    void handleNewTcpConnection();
    void handleTcpData();
    void handleTcpDisconnection();
    void handleUdpData();

private:
    QTcpServer *tcpServer;
    QUdpSocket *udpSocket;
    QSet<QTcpSocket *> clientsTCP;
    QHash<QString, QPair<QHostAddress, quint16>> clientsUDP;

    void broadcastMessage(const QString &message, QTcpSocket *excludeSocket = nullptr);
    void broadcastAudio(const QByteArray &audioData, const QHostAddress &excludeAddress, quint16 excludePort);
};

#endif // CHATSERVER_H
