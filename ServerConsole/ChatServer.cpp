
#include "ChatServer.h"
#include "../possible_requests.h"

#include <QHostAddress>
#include <QDebug>
#include <QDateTime>

ChatServer::ChatServer(QSqlDatabase *db_, QObject *parent)
    : QObject(parent),
    webSocketServer(new QWebSocketServer(QStringLiteral("Chat Server"), QWebSocketServer::NonSecureMode, this)),
    udpSocket(new QUdpSocket(this)),
    db__(db_) {}

ChatServer::~ChatServer() {
    webSocketServer->close();
    qDeleteAll(clientsWebSocket.begin(), clientsWebSocket.end());
    udpSocket->close();
}

bool ChatServer::startServer(quint16 wsPort, quint16 udpPort) {
    // Запуск WebSocket-сервера
    if (!webSocketServer->listen(QHostAddress::Any, wsPort)) {
        qCritical() << "Failed to start WebSocket server:" << webSocketServer->errorString();
        return false;
    }
    connect(webSocketServer, &QWebSocketServer::newConnection, this, &ChatServer::handleNewWebSocketConnection);
    qDebug() << "WebSocket server started on port" << wsPort;

    // Запуск UDP-сервера
    if (!udpSocket->bind(QHostAddress::Any, udpPort)) {
        qCritical() << "Failed to bind UDP socket:" << udpSocket->errorString();
        return false;
    }
    connect(udpSocket, &QUdpSocket::readyRead, this, &ChatServer::handleUdpData);
    qDebug() << "UDP server started on port" << udpPort;

    return true;
}

void ChatServer::handleNewWebSocketConnection() {
    QWebSocket *clientSocket = webSocketServer->nextPendingConnection();
    clientsWebSocket.insert(clientSocket);

    connect(clientSocket, &QWebSocket::textMessageReceived, this, &ChatServer::handleWebSocketMessage);
    connect(clientSocket, &QWebSocket::disconnected, this, &ChatServer::handleWebSocketDisconnection);

    qDebug() << "New WebSocket client connected from" << clientSocket->peerAddress().toString();
}
void printJson(const QJsonDocument &doc)
{
    // Проверяем, является ли JSON объектом или массивом
    if (doc.isObject()) {
        // Выводим JSON объект в строковом формате
        qDebug() << "JSON Object:" << doc.object();
    }
    else {
        qDebug() << "Invalid JSON format.";
    }
}

void ChatServer::handleWebSocketMessage(const QString &message)
{
    QWebSocket *senderSocket = qobject_cast<QWebSocket *>(sender());
    if (!senderSocket)
        return;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(message.toUtf8());
    if (!jsonDoc.isObject()) {
        qWarning() << "Invalid message format";
        return;
    }
    QSqlQuery query;
    QString queryStr;
    QJsonObject messageJsonToSend;
    QJsonObject messageJson = jsonDoc.object();
    printJson(jsonDoc);
    auto request_type = messageJson.value(REQUEST).toString();
    if (request_type == MESSAGE)
    {
        auto message_type = messageJson.value(TYPE).toString();
        if (message_type == TEXT)
        {
            QString userName = messageJson.value("userName").toString();
            QString content = messageJson.value("content").toString();
            QString timestamp = messageJson.value("timestamp").toString();
            QString time = QDateTime::fromString(timestamp, Qt::ISODate).toString("HH:mm");

            messageJsonToSend = {
                {REQUEST, MESSAGE},
                {TYPE, TEXT},
                {"userName", userName},
                {"content", content},
                {"timestamp", time}
            };
        }
        else
        {
            qWarning() << "Invalid message type:"<< message_type;
            return;
        }
    }
    else if(request_type == LOGIN_USER)
    {
        QString login = messageJson.value("login").toString();
        QString passwordHash = messageJson.value("passwordHash").toString();
        int status = messageJson.value("status").toInt();

        queryStr += "SELECT * FROM Users";
        queryStr += " WHERE login = '" + login + "'";
        queryStr += " AND password_hash = '" + passwordHash + "'";
        query.exec(queryStr);
        if (query.next())
        {
            messageJsonToSend = {
                {REQUEST, ANSWER},
                {TYPE, OK},
            };
        }
        else
        {
            messageJsonToSend = {
                {REQUEST, ANSWER},
                {TYPE, NO},
            };
        }
    }
    else if(request_type == CREATE_SERVER)
    {
        QString name = messageJson.value("name").toString();
        int owner_id = messageJson.value("owner_id").toInt();
        QString desc = messageJson.value("description").toString();
        bool is_open = messageJson.value("is_open").toBool();
        QString iconImg = messageJson.value("iconImg").toString();

        queryStr += "INSERT INTO Servers (owner_id, name, is_open, icon_img)";
        queryStr += " VALUES '" + name + "', '" + owner_id + "', '" + desc +"', ";
        queryStr += is_open + ", '" + iconImg + "');";
        query.exec(queryStr);

        if (query.next())
        {
            messageJsonToSend = {
                                 {REQUEST, ANSWER},
                                 {TYPE, OK},
                                 };
        }
        else
        {
            messageJsonToSend = {
                                 {REQUEST, ANSWER},
                                 {TYPE, NO},
                                 };
        }
    }

    broadcastMessage(messageJsonToSend, senderSocket);
}

void ChatServer::handleWebSocketDisconnection()
{
    QWebSocket *senderSocket = qobject_cast<QWebSocket *>(sender());
    if (!senderSocket)
        return;

    qDebug() << "WebSocket client disconnected:" << senderSocket->peerAddress().toString();
    clientsWebSocket.remove(senderSocket);
    senderSocket->deleteLater();
}

void ChatServer::handleUdpData() {
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray buffer;
        QHostAddress senderAddress;
        quint16 senderPort;

        buffer.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(buffer.data(), buffer.size(), &senderAddress, &senderPort);

        QString clientId = senderAddress.toString() + ":" + QString::number(senderPort);
        if (buffer == "DISCONNECT") {
            clientsUDP.remove(clientId);
            qDebug() << "Client disconnected: " << clientId;
            return;
        }

        qDebug() << "Audio data received from" << senderAddress.toString() << ":" << senderPort;

        if (!clientsUDP.contains(clientId)) {
            clientsUDP.insert(clientId, qMakePair(senderAddress, senderPort));
        }

        broadcastAudio(buffer, senderAddress, senderPort);
    }
}

void ChatServer::broadcastMessage(const QJsonObject &message, QWebSocket *excludeSocket) {   
    QByteArray data = QJsonDocument(message).toJson();
    if(message.value(REQUEST) == ANSWER)
    {
        excludeSocket->sendTextMessage(QString::fromUtf8(data));

        return;
    }
    for (QWebSocket *client : clientsWebSocket) {
        if (client != excludeSocket) {
            client->sendTextMessage(QString::fromUtf8(data));
        }
    }
}

void ChatServer::broadcastAudio(const QByteArray &audioData, const QHostAddress &excludeAddress, quint16 excludePort) {
    for (const auto &client : clientsUDP) {
        if (client.first != excludeAddress || client.second != excludePort) {
            qDebug() << "Voice send to" << client.first << client.second;
            udpSocket->writeDatagram(audioData, client.first, client.second);
        }
    }
}
