
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
    clientsWebSocket[clientSocket] = clientsWebSocket.count();

    connect(clientSocket, &QWebSocket::textMessageReceived, this, &ChatServer::handleWebSocketMessage);
    connect(clientSocket, &QWebSocket::disconnected, this, &ChatServer::handleWebSocketDisconnection);

    qDebug() << "New WebSocket client connected from" << clientSocket->peerAddress().toString();
}

QJsonObject ChatServer::messageToJson (const QString &userName_,
                                      const QString &content_,
                                      const QString &timestamp_)
{
    QJsonObject message;
    message["user_name"] = userName_;
    message["content"] = content_;
    message["timestamp"] = timestamp_;

    return message;
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
    QSet<int> user_ids_to_send;

    QSqlQuery query;
    QString queryStr;
    QJsonObject messageJsonToSend;
    QJsonObject messageJson = jsonDoc.object();
    auto request_type = messageJson.value(REQUEST).toString();

    if (request_type == MESSAGE)
    {
        auto message_type = messageJson.value(TYPE).toString();
        int channel_id = messageJson.value("channel_id").toInt();
        int sender_id = messageJson.value("user_id").toInt();
        QString content = messageJson.value("content").toString();
        int server_id = -1;

        query.clear();
        query.prepare(R"(
            SELECT server_id FROM channels
            WHERE id = :channel_id
        )");
        query.bindValue(":channel_id", channel_id);
        query.exec();
        if (query.next())
            server_id = query.value(0).toInt();

        query.clear();
        query.prepare(R"(
            SELECT u.id AS user_id
            FROM Users u
            JOIN Server_User_relation sur ON u.id = sur.user_id
            JOIN Channels c ON sur.server_id = c.server_id
            WHERE c.id = :channel_id
        )");
        query.bindValue(":channel_id", channel_id);

        if (query.exec())
        {
            while (query.next())
                user_ids_to_send.insert(query.value("user_id").toInt());
        }
        if (message_type == TEXT)
        {
            QSqlQuery query;
            query.prepare("SELECT login FROM Users WHERE id = :sender_id");
            query.bindValue(":sender_id", sender_id);

            messageJsonToSend = {
                {REQUEST, MESSAGE},
                {TYPE, TEXT},
                {"server_id", server_id},
                {"channel_id", channel_id}
            };

            QJsonObject msg;
            if (query.exec() && query.next())
            {
                QString userName = query.value(0).toString();

                QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);


                QSqlQuery insertQuery;
                insertQuery.prepare("INSERT INTO Message (author_id, channel_id, type, content, created_at) "
                                    "VALUES (:author_id, :channel_id, :type, :content, :created_at) "
                                    "RETURNING id");
                insertQuery.bindValue(":author_id", sender_id);
                insertQuery.bindValue(":channel_id", channel_id);
                insertQuery.bindValue(":type", message_type);
                insertQuery.bindValue(":content", content);
                insertQuery.bindValue(":created_at", timestamp);

                if (insertQuery.exec() && insertQuery.next()){
                    int msg_id= insertQuery.value(0).toInt();
                    msg[QString::number(msg_id)] = messageToJson(userName, content, timestamp);
                }
                else
                    qWarning() << "Failed to insert message:" << insertQuery.lastError();
            }
            else
                qWarning() << "User not found for ID:" << sender_id;

            messageJsonToSend["message"] = msg;
        }
        else
        {
            qWarning() << "Invalid message type:" << message_type;
        }
    }
    else if(request_type == LOGIN_USER)
    {
        QString login = messageJson.value("login").toString();
        QString passwordHash = messageJson.value("passwordHash").toString();
        int status = messageJson.value("status").toInt();
        int user_id = -1;

        QSqlQuery query;
        query.prepare("SELECT id FROM Users WHERE login = :login AND password_hash = :password_hash");
        query.bindValue(":login", login);
        query.bindValue(":password_hash", passwordHash);

        if (query.exec() && query.next())
        {
            user_id = query.value(0).toInt();
            clientsWebSocket[senderSocket] = user_id;

            user_ids_to_send.insert(user_id);
            messageJsonToSend = generateResponse(true, LOGIN_USER, {{"user_id", user_id}});

            query.clear();
            query.prepare("UPDATE users SET status = :status WHERE id = :user_id;");
            query.bindValue(":status", UserState::Online);
            query.bindValue(":user_id", user_id);
            query.exec();
        }
        else
        {
            messageJsonToSend = generateResponse(false, LOGIN_USER, {{ERROR, "Invalid login or password"}});
        }

        broadcastMessage(messageJsonToSend, user_ids_to_send);
        user_ids_to_send.clear();

        query.clear();
        query.prepare(R"(SELECT DISTINCT
                            CASE
                                WHEN c.user1_id = :user_id THEN c.user2_id
                                ELSE c.user1_id
                            END AS related_user_id,
                            c.id AS compadres_id,
                            NULL::INT AS server_id
                         FROM Compadres c
                         WHERE c.user1_id = :user_id OR c.user2_id = :user_id

                         UNION

                         SELECT DISTINCT
                            sur2.user_id AS related_user_id,
                            NULL::INT AS compadres_id,
                            sur1.server_id
                         FROM Server_User_relation sur1
                         JOIN Server_User_relation sur2 ON sur1.server_id = sur2.server_id
                         WHERE sur1.user_id = :user_id AND sur2.user_id != :user_id;)");
        query.bindValue(":user_id", user_id);
        query.exec();
        while(query.next())
        {
            user_ids_to_send.insert(query.value("related_user_id").toInt());
        }

        QJsonObject response{
            {"user_id", user_id},
            {"user_login", login},
            {"status", UserState::Online}
        };
        messageJsonToSend = generateResponse(true, UPDATE_USER, response);

        broadcastMessage(messageJsonToSend, user_ids_to_send);

        return;
    }
    else if(request_type == LOGOUT_USER)
    {
        int user_id = messageJson.value("user_id").toInt();
        QString login = messageJson.value("user_login").toString();

        QSqlQuery query;
        query.prepare("UPDATE users SET status = :status WHERE id = :user_id;");
        query.bindValue(":status", UserState::Offline);
        query.bindValue(":user_id", user_id);
        query.exec();

        query.clear();
        query.prepare(R"(SELECT DISTINCT
                            CASE
                                WHEN c.user1_id = :user_id THEN c.user2_id
                                ELSE c.user1_id
                            END AS related_user_id,
                            c.id AS compadres_id,
                            NULL::INT AS server_id
                         FROM Compadres c
                         WHERE c.user1_id = :user_id OR c.user2_id = :user_id

                         UNION

                         SELECT DISTINCT
                            sur2.user_id AS related_user_id,
                            NULL::INT AS compadres_id,
                            sur1.server_id
                         FROM Server_User_relation sur1
                         JOIN Server_User_relation sur2 ON sur1.server_id = sur2.server_id
                         WHERE sur1.user_id = :user_id AND sur2.user_id != :user_id;)");
        query.bindValue(":user_id", user_id);
        query.exec();
        while(query.next())
        {
            user_ids_to_send.insert(query.value("related_user_id").toInt());
        }

        QJsonObject response{
            {"user_id", user_id},
            {"user_login", login},
            {"status", UserState::Offline}
        };
        messageJsonToSend = generateResponse(true, UPDATE_USER, response);

        broadcastMessage(messageJsonToSend, user_ids_to_send);
        return;
    }
    else if(request_type == CREATE_SERVER)
    {
        QString name = messageJson.value("name").toString();
        int owner_id = messageJson.value("owner_id").toInt();
        QString desc = messageJson.value("description").toString();
        int num_of_voice = messageJson.value("num_of_voice").toInt();
        int num_of_text = messageJson.value("num_of_text").toInt();

        user_ids_to_send.insert(owner_id);

        bool is_open = messageJson.value("is_open").toBool();
        QString iconImg = messageJson.value("iconImg").toString();

        QSqlQuery query;
        query.prepare("INSERT INTO Servers (owner_id, name, description, is_open, icon_img) "
                      "VALUES (:owner_id, :name, :description, :is_open, :icon_img) "
                      "RETURNING id");

        query.bindValue(":owner_id", owner_id);
        query.bindValue(":name", name);
        query.bindValue(":description", desc);
        query.bindValue(":is_open", is_open);
        query.bindValue(":icon_img", iconImg);

        if (query.exec() && query.next())
        {
            int server_id = query.value(0).toInt();

            QJsonArray channels_info;

            for (int i = 0; i < num_of_voice; i++)
            {
                QString channel_name = QString("Voice Channel %1").arg(i + 1);
                insertChannel(server_id, owner_id, channel_name, true, channels_info);
            }
            for (int i = 0; i < num_of_text; i++) {
                QString channel_name = QString("Text Channel %1").arg(i + 1);
                insertChannel(server_id, owner_id, channel_name, false, channels_info);
            }

            QJsonObject response_params{
                {"server_id", server_id},
                {"server_name", name},
                {"server_desc", desc},
                {"is_open", is_open},
                {"server_img", iconImg},
                {"channels", channels_info}
            };

            messageJsonToSend = {
                {REQUEST, CREATE_SERVER},
                {INFO, response_params}
            };
        }
        else
        {
            qDebug() << "Error inserting server:" << query.lastError().text();
            messageJsonToSend = generateResponse(false, {{ERROR, "Error inserting Server or Channels"}});
        }
    }
    else if (request_type == JOIN_SERVER)
    {
        int user_id = messageJson.value("user_id").toInt();
        int invite_id = messageJson.value("invite_id").toInt();
        user_ids_to_send.insert(user_id);

        QSqlQuery query;
        query.prepare("SELECT uses_left FROM invite_to_server "
                      "WHERE id = :invite_id;");
        query.bindValue(":invite_id", invite_id);
        query.exec();

        QJsonObject response_params;
        while(query.next())
        {
            if (query.value("uses_left").toInt())
            {
                // Вычесть 1 и что-нибудь сделать
            }
        }
        query.clear();

        query.prepare(
            "INSERT INTO Server_User_relation (user_id, server_id, role_id) "
            "SELECT "
            "    ?, r.server_id, i.roles_id "
            "FROM Invite_to_server i "
            "JOIN Roles r ON i.roles_id = r.id "
            "WHERE i.id = ?;"
            );

        query.addBindValue(user_id);
        query.addBindValue(invite_id);
        if(query.exec())
            messageJsonToSend = generateResponse(true, JOIN_SERVER, response_params);
        else
        {
            response_params["error_msg"] = query.lastError().text();
            messageJsonToSend = generateResponse(false, JOIN_SERVER, response_params);
        }
    }
    else if(request_type == GET_OPEN_SERVERS_LIST)
    {
        int user_id = messageJson.value("user_id").toInt();
        user_ids_to_send.insert(user_id);
        QSqlQuery query;
        query.prepare("SELECT id, name, description, icon_img, invite_default_id FROM servers "
                      "WHERE is_open = true;");
        query.exec();

        QJsonObject response_params;
        while (query.next())
        {
            QString serverId = QString::number(query.value("id").toInt());
            QJsonObject serverInfo{
                {"server_name", query.value("name").toString()},
                {"server_desc", query.value("description").toString()},
                {"server_img", query.value("icon_img").toString()},
                {"invite_id", query.value("invite_default_id").toInt()},
            };
            response_params.insert(serverId, serverInfo);
        }
        messageJsonToSend = generateResponse(true, GET_OPEN_SERVERS_LIST, response_params);
    }
    else if(request_type == GET_USER_SERVERS_LIST)
    {
        int user_id = messageJson.value("user_id").toInt();
        user_ids_to_send.insert(user_id);

        QSqlQuery query;
        query.prepare("SELECT "
                      "Servers.id AS server_id, "
                      "Servers.name AS server_name, "
                      "Servers.description AS server_desc, "
                      "Servers.is_open AS is_open, "
                      "Servers.icon_img AS server_img, "
                      "Roles.name AS role_name, "
                      "Roles.is_highest_permission AS is_highest_perm, "
                      "Roles.user_permissions AS user_perm, "
                      "Roles.admin_permissions AS admin_perm "
                      "FROM Server_User_relation "
                      "JOIN Servers ON Servers.id = Server_User_relation.server_id "
                      "JOIN Roles ON Roles.id = Server_User_relation.role_id "
                      "WHERE Server_User_relation.user_id = :user_id");

        query.bindValue(":user_id", user_id);
        query.exec();

        QJsonObject response_params;
        while (query.next())
        {
            QString serverId = QString::number(query.value("server_id").toInt());
            QJsonObject serverInfo{
                {"server_name", query.value("server_name").toString()},
                {"server_desc", query.value("server_desc").toString()},
                {"is_open", query.value("is_open").toBool()},
                {"server_img", query.value("server_img").toString()},
                {"role_name", query.value("role_name").toString()},
                {"is_highest_perm", query.value("is_highest_perm").toBool()},
                {"user_perm", query.value("user_perm").toString()},
                {"admin_perm", query.value("admin_perm").toString()}
            };

            QSqlQuery channelQuery;
            channelQuery.prepare("SELECT id, is_voice, name "
                                 "FROM Channels "
                                 "WHERE server_id = :server_id");

            channelQuery.bindValue(":server_id", query.value("server_id").toInt());
            channelQuery.exec();

            QJsonArray channelsArray;
            while (channelQuery.next()) {
                QJsonObject channelInfo{
                    {"id", channelQuery.value("id").toInt()},
                    {"is_voice", channelQuery.value("is_voice").toBool()},
                    {"name", channelQuery.value("name").toString()}
                };
                channelsArray.append(channelInfo);
            }
            serverInfo.insert("channels", channelsArray);

            response_params.insert(serverId, serverInfo);
        }
        messageJsonToSend = generateResponse(true, GET_USER_SERVERS_LIST, response_params);

    }
    else if(request_type == GET_MESSAGES_LIST)
    {
        int server_id = -1;
        int channel_id = messageJson.value("channel_id").toInt();
        int num = messageJson.value("num_of_msgs").toInt();
        int user_id = messageJson.value("user_id").toInt();

        user_ids_to_send.insert(user_id);
        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            SELECT server_id FROM channels
            WHERE id = :channel_id
        )");
        query.bindValue(":channel_id", channel_id);
        query.exec();
        if (query.next())
            server_id = query.value(0).toInt();

        query.clear();

        messageJsonToSend = {
                             {REQUEST, MESSAGE},
                             {TYPE, TEXT},
                             {"server_id", server_id},
                             {"channel_id", channel_id},
                             };

        QString sql = R"(SELECT
                            m.id AS message_id,
                            m.type AS type,
                            u.login AS sender_name,
                            m.content AS content,
                            m.created_at AS created_at,
                            m.is_edited AS is_edited
                        FROM Message m
                        JOIN Users u ON m.author_id = u.id
                        WHERE m.channel_id = :channel_id
                        ORDER BY m.created_at DESC
                        LIMIT :N;
                        )";
        sql.replace(":N", QString::number(num));
        query.prepare(sql);
        query.bindValue(":channel_id", channel_id);
        query.exec();


        QJsonObject msgs;
        while (query.next())
        {
            QJsonObject msg;
            int message_id= query.value("message_id").toInt();
            QString type = query.value("type").toString();
            QString authorName = query.value("sender_name").toString();
            QString content = query.value("content").toString();
            QString timestamp = query.value("created_at").toString();
            bool is_edited = query.value("is_edited").toBool();

            msg = messageToJson(authorName, content, timestamp);
            msg["type"] = type;
            msg["is_edited"] = is_edited;

            msgs[QString::number(message_id)] = msg;
        }
        messageJsonToSend["message"] = msgs;
    }
    else if(request_type == DELETE_SERVER)
    {
        int server_id = messageJson.value("server_id").toInt();
        int user_id = messageJson.value("user_id").toInt();
        user_ids_to_send.insert(user_id);
        QJsonObject response_params;
        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            DELETE FROM servers
            WHERE id = :server_id
        )");
        query.bindValue(":server_id", server_id);
        query.exec();

        response_params["server_id"] = server_id;
        messageJsonToSend = generateResponse(true, DELETE_SERVER, response_params);
    }
    else if(request_type == LEAVE_SERVER)
    {
        int server_id = messageJson.value("server_id").toInt();
        int user_id = messageJson.value("user_id").toInt();

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            SELECT user_id FROM server_user_relation
            WHERE server_id = :server_id;
        )");
        query.bindValue(":server_id", server_id);
        query.exec();

        QJsonObject response_params;
        while(query.next())
        {
            user_ids_to_send.insert(query.value("user_id").toInt());
        }

        query.clear();
        query.prepare(R"(
            DELETE FROM server_user_relation
            WHERE user_id = :user_id AND server_id = :server_id
        )");
        query.bindValue(":user_id", user_id);
        query.bindValue(":server_id", server_id);
        query.exec();


        response_params["server_id"] = server_id;
        response_params["user_id"] = user_id;
        response_params["user_status"] = -1;

        messageJsonToSend = generateResponse(true, USER_LEAVE_SERVER_INFO, response_params);
    }
    else if(request_type == GET_SERVER_PARTICIPANTS_LIST)
    {
        int server_id = messageJson.value("server_id").toInt();
        int user_id = messageJson.value("user_id").toInt();

        user_ids_to_send.insert(user_id);

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            SELECT s_u_r.user_id AS user_id, u.status AS user_status, u.login AS login FROM server_user_relation s_u_r
            JOIN users u ON u.id = s_u_r.user_id
            WHERE s_u_r.server_id = :server_id;
        )");
        query.bindValue(":server_id", server_id);
        query.exec();

        QJsonObject response_params;
        while(query.next())
        {
            QJsonObject user_info{
                {"user_status", query.value("user_status").toInt()},
                {"user_login", query.value("login").toString()}
            };
            response_params.insert(query.value("user_id").toString(), user_info);
        }
        QJsonObject response{
            {"server_id", server_id},
            {"users_list", response_params}
        };

        messageJsonToSend = generateResponse(true, GET_SERVER_PARTICIPANTS_LIST, response);
    }
    else if(request_type == GET_FRIEND_REQUESTS)
    {
        int user_id = messageJson.value("user_id").toInt();

        user_ids_to_send.insert(user_id);

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            SELECT user1_id AS user_id, users.login AS user_login FROM compadres
            JOIN users ON users.id = compadres.user1_id
            WHERE user2_id = :user_id AND compadres.status = :status
        )");
        query.bindValue(":user_id", user_id);
        query.bindValue(":status", FriendShipState::Waiting);
        query.exec();

        QJsonObject response_params;
        while(query.next())
        {
            QJsonObject user_info{
                {"user_login", query.value("user_login").toString()}
            };
            response_params.insert(query.value("user_id").toString(), user_info);
        }
        QJsonObject response{
            {"friend_requests", response_params}
        };

        messageJsonToSend = generateResponse(true, GET_FRIEND_REQUESTS, response);
    }
    else if(request_type == GET_FRIEND_LIST)
    {
        int user_id = messageJson.value("user_id").toInt();

        user_ids_to_send.insert(user_id);

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            SELECT u.id AS user_id, u.login AS user_login, u.status AS user_status
            FROM users u
            JOIN compadres c
                ON (
                    (c.user1_id = :user_id AND c.user2_id = u.id) OR
                    (c.user2_id = :user_id AND c.user1_id = u.id)
                )
            WHERE c.status = :status;
        )");
        query.bindValue(":user_id", user_id);
        query.bindValue(":status", FriendShipState::Accepted);
        query.exec();

        QJsonObject response_params;
        while(query.next())
        {
            QJsonObject user_info{
                {"user_login", query.value("user_login").toString()},
                {"user_status", query.value("user_status").toInt()}
            };
            response_params.insert(query.value("user_id").toString(), user_info);
        }
        QJsonObject response{
            {"friend_list", response_params}
        };

        messageJsonToSend = generateResponse(true, GET_FRIEND_LIST, response);
    }
    else if(request_type == CREATE_FRIENDSHIP)
    {
        int sender_id = messageJson.value("sender_id").toInt();
        int server_id = messageJson.value("server_id").toInt();
        int target_id = messageJson.value("target_id").toInt();

        user_ids_to_send.insert(target_id);

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            INSERT INTO compadres (user1_id, user2_id, status)
            VALUES (:sender_id, :target_id, :status);
        )");
        query.bindValue(":sender_id", sender_id);
        query.bindValue(":target_id", target_id);
        query.bindValue(":status", FriendShipState::Waiting);
        query.exec();

        query.clear();
        query.prepare(R"(
            SELECT login FROM users
            WHERE id = :sender_id;
                         )");
        query.bindValue(":sender_id", sender_id);
        query.exec();
        query.next();

        QJsonObject response_params
            {
                {"user_id", sender_id},
                {"user_login", query.value("login").toString()}
            };

        messageJsonToSend = generateResponse(true, CREATE_FRIENDSHIP, response_params);
    }
    else if(request_type == ACCEPT_FRIENDSHIP)
    {
        int user2_id = messageJson.value("accepter_id").toInt();
        int user1_id = messageJson.value("sender_id").toInt();

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            UPDATE compadres
            SET status = :status
            WHERE user1_id = :user1_id AND user2_id = :user2_id;
        )");
        query.bindValue(":user1_id", user1_id);
        query.bindValue(":user2_id", user2_id);
        query.bindValue(":status", FriendShipState::Accepted);
        query.exec();

        query.clear();
        query.prepare(R"(
            SELECT id, login, status FROM users
            WHERE id = :user1_id OR id = :user2_id;
                         )");
        query.bindValue(":user1_id", user1_id);
        query.bindValue(":user2_id", user2_id);
        query.exec();
        query.next();

        // Ответ создателю запроса
        {
            user_ids_to_send.insert(user1_id);
            QJsonObject response_params
                {
                    {"user_id", user2_id},
                    {"user_login", query.value("login").toString()},
                    {"friendship_status", FriendShipState::Accepted},
                    {"user_status", query.value("status").toInt()}
                };

            messageJsonToSend = generateResponse(true, ACCEPT_FRIENDSHIP, response_params);
            broadcastMessage(messageJsonToSend, user_ids_to_send);
        }
        query.next();

        // Ответ принимающему запрос
        {
            user_ids_to_send.clear();
            user_ids_to_send.insert(user2_id);
            QJsonObject response_params
                {
                    {"user_id", user1_id},
                    {"user_login", query.value("login").toString()},
                    {"friendship_status", FriendShipState::Accepted},
                    {"user_status", query.value("status").toInt()}
                };

            messageJsonToSend = generateResponse(true, ACCEPT_FRIENDSHIP, response_params);
            broadcastMessage(messageJsonToSend, user_ids_to_send);
        }
        return;
    }

    broadcastMessage(messageJsonToSend, user_ids_to_send);
}

bool ChatServer::insertChannel(int server_id, int owner_id, const QString& name, bool is_voice, QJsonArray& channels_info)
{
    QSqlQuery channelQuery;
    channelQuery.prepare("INSERT INTO Channels (server_id, owner_id, name, is_voice) "
                         "VALUES (:server_id, :owner_id, :name, :is_voice) RETURNING id");
    channelQuery.bindValue(":server_id", server_id);
    channelQuery.bindValue(":owner_id", owner_id);
    channelQuery.bindValue(":name", name);
    channelQuery.bindValue(":is_voice", is_voice);

    if (channelQuery.exec() && channelQuery.next())
    {
        QJsonObject channel_info{
            {"channel_id", channelQuery.value(0).toInt()},
            {"name", name},
            {"is_voice", is_voice}
        };
        channels_info.append(channel_info);
        return true;
    }
    else
    {
        qDebug() << "Error inserting channel:" << channelQuery.lastError().text();
        return false;
    }
}


QJsonObject ChatServer::generateResponse(bool is_positive_, const QJsonObject &response_params_)
{
    QJsonObject response{
        {REQUEST, ANSWER},
        {TYPE, is_positive_ ? OK : NO},
        {INFO, response_params_}
    };

    return response;
}

QJsonObject ChatServer::generateResponse(bool is_positive_, const QString requestType_, const QJsonObject &response_params_)
{
    QJsonObject response{
        {REQUEST, requestType_},
        {TYPE, is_positive_ ? OK : NO},
        {INFO, response_params_}
    };

    return response;
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

void ChatServer::broadcastMessage(const QJsonObject &message, QSet<int> user_ids_to_send_)
{
    QByteArray data = QJsonDocument(message).toJson();
    for (auto it = clientsWebSocket.begin(); it != clientsWebSocket.end(); it++)
    {
        QWebSocket *client = it.key();
        int userId = it.value();

        if (user_ids_to_send_.contains(userId))
        {
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
