#include "ChatServer.h"
#include <QHostAddress>
#include <QDebug>

ChatServer::ChatServer(QObject *parent)
    : QObject(parent), tcpServer(new QTcpServer(this)), udpSocket(new QUdpSocket(this)) {}

ChatServer::~ChatServer() {
    tcpServer->close();
    udpSocket->close();
}

bool ChatServer::startServer(quint16 tcpPort, quint16 udpPort) {
    // Запуск TCP-сервера
    if (!tcpServer->listen(QHostAddress::Any, tcpPort)) {
        qCritical() << "Failed to start TCP server:" << tcpServer->errorString();
        return false;
    }
    connect(tcpServer, &QTcpServer::newConnection, this, &ChatServer::handleNewConnection);
    qDebug() << "TCP server started on port" << tcpPort;

    // Запуск UDP-сервера
    if (!udpSocket->bind(QHostAddress::Any, udpPort)) {
        qCritical() << "Failed to bind UDP socket:" << udpSocket->errorString();
        return false;
    }
    connect(udpSocket, &QUdpSocket::readyRead, this, &ChatServer::handleUdpData);
    qDebug() << "UDP server started on port" << udpPort;

    return true;
}

void ChatServer::handleNewConnection() {
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    clients.insert(clientSocket);

    connect(clientSocket, &QTcpSocket::readyRead, this, &ChatServer::handleTcpData);
    connect(clientSocket, &QTcpSocket::disconnected, this, &ChatServer::handleTcpDisconnection);

    qDebug() << "New client connected from" << clientSocket->peerAddress().toString();
}

void ChatServer::handleTcpData() {
    QTcpSocket *senderSocket = qobject_cast<QTcpSocket *>(sender());
    if (!senderSocket) return;

    QByteArray data = senderSocket->readAll();
    QString message = QString::fromUtf8(data);

    qDebug() << "Message received:" << message;
    broadcastMessage(message, senderSocket);
}

void ChatServer::handleTcpDisconnection() {
    QTcpSocket *senderSocket = qobject_cast<QTcpSocket *>(sender());
    if (!senderSocket) return;

    qDebug() << "Client disconnected:" << senderSocket->peerAddress().toString();
    clients.remove(senderSocket);
    senderSocket->deleteLater();
}

void ChatServer::handleUdpData() {
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        udpSocket->readDatagram(buffer.data(), buffer.size(), &senderAddress, &senderPort);
        qDebug() << "Audio data received from" << senderAddress.toString() << ":" << senderPort;

        broadcastAudio(buffer, senderAddress, senderPort);
    }
}

void ChatServer::broadcastMessage(const QString &message, QTcpSocket *excludeSocket) {
    QByteArray data = message.toUtf8();
    for (QTcpSocket *client : clients) {
        if (client != excludeSocket) {
            client->write(data);
        }
    }
}

void ChatServer::broadcastAudio(const QByteArray &audioData, const QHostAddress &excludeAddress, quint16 excludePort) {
    for (QTcpSocket *client : clients) {
        QHostAddress clientAddress = client->peerAddress();
        quint16 clientPort = client->peerPort();
        if (clientAddress != excludeAddress || clientPort != excludePort) {
            udpSocket->writeDatagram(audioData, clientAddress, clientPort);
        }
    }
}
