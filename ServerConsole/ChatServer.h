#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QUdpSocket>
#include <QSet>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

class ChatServer : public QObject {
    Q_OBJECT

public:
    explicit ChatServer(QSqlDatabase *db_ = nullptr, QObject *parent = nullptr);
    ~ChatServer();

    bool startServer(quint16 wsPort, quint16 udpPort);

private slots:
    void handleNewWebSocketConnection();
    void handleWebSocketMessage(const QString &message);
    void handleWebSocketDisconnection();
    void handleUdpData();
signals:
    void textMessageReceived(const QString &userName,
                             const QString &content,
                             const QString &timestamp);
private:
    QSqlDatabase *db__;
    QWebSocketServer *webSocketServer;
    QSet<QWebSocket *> clientsWebSocket;
    QUdpSocket *udpSocket;
    QHash<QString, QPair<QHostAddress, quint16>> clientsUDP;

    void parseJson(QJsonObject &messageJson_);
    void broadcastMessage(const QJsonObject &message, QWebSocket *excludeSocket = nullptr);
    void broadcastAudio(const QByteArray &audioData, const QHostAddress &excludeAddress, quint16 excludePort);
};

#endif // CHATSERVER_H
