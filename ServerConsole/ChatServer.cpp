#include "ChatServer.h"
#include <QHostAddress>
#include <QDebug>

ChatServer::ChatServer(QObject *parent)
    : QObject(parent), tcpServer(new QTcpServer(this)), udpSocket(new QUdpSocket(this)) {}

ChatServer::~ChatServer()
{
    tcpServer->close();
    udpSocket->close();
}

bool ChatServer::startServer(quint16 tcpPort, quint16 udpPort)
{
    // Запуск TCP-сервера
    if (!tcpServer->listen(QHostAddress::Any, tcpPort))
    {
        qCritical() << "Failed to start TCP server:" << tcpServer->errorString();
        return false;
    }
    connect(tcpServer, &QTcpServer::newConnection, this, &ChatServer::handleNewTcpConnection);
    qDebug() << "TCP server started on port" << tcpPort;

    // Запуск UDP-сервера
    if (!udpSocket->bind(QHostAddress::Any, udpPort))
    {
        qCritical() << "Failed to bind UDP socket:" << udpSocket->errorString();
        return false;
    }
    connect(udpSocket, &QUdpSocket::readyRead, this, &ChatServer::handleUdpData);
    qDebug() << "UDP server started on port" << udpPort;

    return true;
}

void ChatServer::handleNewTcpConnection()
{
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    clientsTCP.insert(clientSocket);

    connect(clientSocket, &QTcpSocket::readyRead, this, &ChatServer::handleTcpData);
    connect(clientSocket, &QTcpSocket::disconnected, this, &ChatServer::handleTcpDisconnection);

    qDebug() << "New client connected from" << clientSocket->peerAddress().toString();
}

void ChatServer::handleTcpData()
{
    QTcpSocket *senderSocket = qobject_cast<QTcpSocket *>(sender());
    if (!senderSocket)
        return;

    QByteArray data = senderSocket->readAll();
    QString message = QString::fromUtf8(data);

    qDebug() << "Message received:" << message;
    broadcastMessage(message, senderSocket);
}

void ChatServer::handleTcpDisconnection()
{
    QTcpSocket *senderSocket = qobject_cast<QTcpSocket *>(sender());
    if (!senderSocket)
        return;

    qDebug() << "Client disconnected:" << senderSocket->peerAddress().toString();
    clientsTCP.remove(senderSocket);
    senderSocket->deleteLater();
}

void ChatServer::handleUdpData()
{
    while (udpSocket->hasPendingDatagrams())
    {
        QByteArray buffer;
        QHostAddress senderAddress;
        quint16 senderPort;

        buffer.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(buffer.data(), buffer.size(), &senderAddress, &senderPort);

        QString clientId = senderAddress.toString() + ":" + QString::number(senderPort);
        if (buffer == "DISCONNECT")
        {
            clientsUDP.remove(clientId);
            qDebug() << "Client disconnected: " << clientId;
            return;
        }

        qDebug() << "Audio data received from" << senderAddress.toString() << ":" << senderPort;

        if (!clientsUDP.contains(clientId))
        {
            clientsUDP.insert(clientId, qMakePair(senderAddress, senderPort));
        }

        broadcastAudio(buffer, senderAddress, senderPort);
    }
}

void ChatServer::broadcastMessage(const QString &message, QTcpSocket *excludeSocket)
{
    QByteArray data = message.toUtf8();
    for (QTcpSocket *client : clientsTCP)
    {
        if (client != excludeSocket)
        {
            client->write(data);
        }
    }
}

// void ChatServer::broadcastAudio(const QByteArray &audioData, const QHostAddress &excludeAddress, quint16 excludePort) {
//     for (QTcpSocket *client : clientsTCP) {
//         QHostAddress clientAddress = client->peerAddress();
//         quint16 clientPort = client->peerPort();
//         qDebug() << client->peerPort() << client->peerAddress();

//         if (clientAddress != excludeAddress || clientPort != excludePort) {
//             udpSocket->writeDatagram(audioData, clientAddress, clientPort);
//         }
//     }
// }


void ChatServer::broadcastAudio(const QByteArray &audioData, const QHostAddress &excludeAddress, quint16 excludePort)
{
    for (const auto &client : clientsUDP)
    {
        if (client.first != excludeAddress || client.second != excludePort)
        {
            qDebug() << "Voice send to" <<  client.first << client.second;
            udpSocket->writeDatagram(audioData, client.first , client.second);
        }
    }
}
