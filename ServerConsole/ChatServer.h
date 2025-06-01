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
#include <QJsonArray>


#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

enum UserState
{
    InGame = 0,
    Online = 1,
    Offline = 2
};

enum FriendShipState
{
    NotFriends = -1,
    Waiting = 0,
    Accepted = 1,
    Blocked = 2,
    Chat = 3
};

struct User
{
    QString name;
    int id;
};

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
    QMap<QWebSocket *, int> clientsWebSocket;
    QUdpSocket *udpSocket;
    QHash<QString, QPair<QHostAddress, quint16>> clientsUDP;
    QMap<int, QList<User>> voiceChannels__;

    void parseJson(QJsonObject &messageJson_);
    void broadcastMessage(const QJsonObject &message, QSet<int> user_ids_to_send_);
    void broadcastAudio(const QByteArray &audioData, const QHostAddress &excludeAddress, quint16 excludePort);
    QJsonObject generateResponse(bool is_positive, const QJsonObject &response_params);
    QJsonObject generateResponse(bool is_positive_, const QString requestType_, const QJsonObject &response_params_);
    bool insertChannel(int server_id, int owner_id, const QString& name, bool is_voice, QJsonArray& channels_info);
    bool insertMessage(int sender_id, QString &userName, int channel_id, const QString& message_type, QString& content, QString &created_at, QJsonObject &msgHandler);
    QJsonObject messageToJson(const QString &userName_,
                              const QString &content_,
                              const QString &timestamp_);
};

#endif // CHATSERVER_H
