
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

QJsonObject ChatServer::messageToJson(const QString &userName_,
                                      const QJsonObject &content_,
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
        int server_id = -1;
        if (channel_id <= 0)
        {
            query.clear();
            query.prepare(R"(
                SELECT channels.id
                FROM compadres
                JOIN channels ON compadres_id = compadres.id
                WHERE (user1_id = 1 AND user2_id = 2) OR
                      (user1_id = 2 AND user2_id = 1)
            )");
            int target_id = messageJson["target_id"].toInt();

            query.bindValue(":sender_id", sender_id);
            query.bindValue(":target_id", target_id);
            query.exec();
            if (query.next())
                channel_id = query.value(0).toInt();
        }

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
        if (server_id > 0)
            query.prepare(R"(
                SELECT u.id AS user_id
                FROM Users u
                JOIN Server_User_relation sur ON u.id = sur.user_id
                JOIN Channels c ON sur.server_id = c.server_id
                WHERE c.id = :channel_id
            )");
        else
            query.prepare(R"(
                SELECT user1_id AS user_id FROM compadres
                JOIN channels ON compadres.id = channels.compadres_id
                WHERE channels.id = :channel_id

                UNION

                SELECT user2_id AS user_id FROM compadres
                JOIN channels ON compadres.id = channels.compadres_id
                WHERE channels.id = :channel_id;
            )");
        query.bindValue(":channel_id", channel_id);

        if (query.exec())
        {
            while (query.next())
            {
                user_ids_to_send.insert(query.value("user_id").toInt());
            }
        }
        QSqlQuery query;
        query.prepare("SELECT login FROM Users WHERE id = :sender_id");
        query.bindValue(":sender_id", sender_id);

        messageJsonToSend = {
            {REQUEST, MESSAGE},
            {"server_id", server_id},
            {"channel_id", channel_id}
        };

        QJsonObject msg;
        if (query.exec() && query.next())
        {
            QString userName = query.value(0).toString();
            QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
            if (message_type == TEXT)
            {
                QString content = messageJson.value("content").toString();
                insertTextMessage(sender_id, userName, channel_id, message_type, content, timestamp, msg);
            }
            else if (message_type == INVITE)
            {
                msg.insert(TYPE, INVITE);
                // qDebug() << sender_id << userName << channel_id << message_type << messageJson["invite_id"].toInt() << messageJson["serverName"].toString();
                QString serverName = messageJson["serverName"].toString();
                insertInviteMessage(sender_id, userName, channel_id, message_type,
                                    messageJson["invite_id"].toInt(), serverName, timestamp, msg);
            }
        }
        else
            qWarning() << "User not found for ID:" << sender_id;

        messageJsonToSend["message"] = msg;
        qDebug() << messageJsonToSend;
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
            if (query.value("uses_left").toString().isEmpty())
            {
                //return;
                // Вычесть 1 и что-нибудь сделать
            }
            else
            {
                int uses_left = query.value("uses_left").toInt() - 1;
                query.clear();
                query.prepare("UPDATE invite_to_server"
                              "SET uses_left = :uses_left "
                              "WHERE id = :invite_id;");
                query.bindValue(":uses_left", uses_left);
                query.bindValue(":invite_id", invite_id);
                query.exec();
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
            int message_id = query.value("message_id").toInt();
            QString type = query.value("type").toString();
            QString authorName = query.value("sender_name").toString();

            QJsonObject contentObj;
            QVariant contentVariant = query.value("content");
            if (!contentVariant.isNull()) {
                QString jsonStr = contentVariant.toString();
                QJsonParseError parseError;
                QJsonDocument contentDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
                if (parseError.error == QJsonParseError::NoError && contentDoc.isObject()) {
                    contentObj = contentDoc.object();
                } else {
                    qWarning() << "Ошибка парсинга поля content:" << parseError.errorString();
                    qWarning() << "Исходная строка content:" << jsonStr;
                }
            }

            QString timestamp = query.value("created_at").toString();
            bool is_edited = query.value("is_edited").toBool();

            msg = messageToJson(authorName, contentObj, timestamp);
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
            SELECT
                s_u_r.user_id AS user_id,
                u.status AS user_status,
                u.login AS login,
                COALESCE(c1.status, c2.status) AS friend_status
            FROM server_user_relation s_u_r
            JOIN users u ON u.id = s_u_r.user_id
            LEFT JOIN compadres c1
                ON c1.user1_id = :user_id AND c1.user2_id = s_u_r.user_id
            LEFT JOIN compadres c2
                ON c2.user2_id = :user_id AND c2.user1_id = s_u_r.user_id
            WHERE s_u_r.server_id = :server_id;
        )");
        query.bindValue(":user_id", user_id);
        query.bindValue(":server_id", server_id);
        query.exec();

        QJsonObject response_params;
        while(query.next())
        {
            QJsonObject user_info{
                {"user_status", query.value("user_status").toInt()},
                {"user_login", query.value("login").toString()}
            };
            if (query.value("friend_status").toString().isEmpty())
                user_info["friend_status"] = FriendShipState::NotFriends;
            else
                user_info["friend_status"] = query.value("friend_status").toInt();
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
    else if (request_type == GET_FRIEND_LIST)
    {
        int user_id = messageJson.value("user_id").toInt();

        user_ids_to_send.insert(user_id);

        QSqlQuery query;
        query.prepare(R"(
        SELECT
            u.id AS user_id,
            u.login AS user_login,
            u.status AS user_status,
            json_agg(json_build_object('id', s.id, 'name', s.name))
                FILTER (WHERE s.id IS NOT NULL) AS common_servers
        FROM users u
        JOIN compadres c ON (
            (c.user1_id = :user_id AND c.user2_id = u.id)
            OR
            (c.user2_id = :user_id AND c.user1_id = u.id)
        )
        LEFT JOIN (
            SELECT sur1.user_id AS user1_id, sur1.server_id
            FROM Server_User_relation sur1
            JOIN Server_User_relation sur2
                ON sur1.server_id = sur2.server_id
            WHERE sur2.user_id = :user_id
        ) common ON common.user1_id = u.id
        LEFT JOIN Servers s ON s.id = common.server_id
        WHERE c.status = :status
        GROUP BY u.id, u.login, u.status;
    )");

        query.bindValue(":user_id", user_id);
        query.bindValue(":status", FriendShipState::Accepted);

        QJsonObject response_params;
        if (query.exec())
        {
            while (query.next())
            {
                QJsonObject user_info{
                    {"user_login", query.value("user_login").toString()},
                    {"user_status", query.value("user_status").toInt()}
                };

                QJsonArray commonServers;
                QVariant commonServersVariant = query.value("common_servers");
                if (!commonServersVariant.isNull())
                {
                    QString jsonStr = commonServersVariant.toString();
                    QJsonParseError parseError;
                    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
                    if (parseError.error == QJsonParseError::NoError && doc.isArray())
                    {
                        commonServers = doc.array();
                    }
                }

                user_info.insert("common_servers", commonServers);
                response_params.insert(query.value("user_id").toString(), user_info);
            }
        }

        QJsonObject response{
            {"friend_list", response_params}
        };

        messageJsonToSend = generateResponse(true, GET_FRIEND_LIST, response);
    }
    else if(request_type == CREATE_FRIENDSHIP)
    {
        int sender_id = messageJson.value("sender_id").toInt();
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
    else if(request_type == GET_PERSONAL_CHATS_LIST)
    {
        int user_id = messageJson.value("user_id").toInt();
        user_ids_to_send.insert(user_id);

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            SELECT
                ch.id AS channel_id,
                u.id AS other_user_id,
                u.login AS other_user_login,
                c.id AS compadres_id
            FROM channels ch
            JOIN compadres c ON c.id = ch.compadres_id
            JOIN users u ON
                ( (c.user1_id = :user_id AND u.id = c.user2_id) OR
                  (c.user2_id = :user_id AND u.id = c.user1_id) )
            WHERE ch.is_voice = false;
        )");
        query.bindValue(":user_id", user_id);
        query.exec();

        QJsonObject response_params;
        while(query.next())
        {
            QJsonObject user_info{
                {"user_id", query.value("other_user_id").toInt()},
                {"user_login", query.value("other_user_login").toString()},
                {"compadres_id", query.value("compadres_id").toInt()}
            };
            response_params.insert(query.value("channel_id").toString(), user_info);
        }
        QJsonObject response{
            {"personal_chats_list", response_params}
        };

        messageJsonToSend = generateResponse(true, GET_PERSONAL_CHATS_LIST, response);
    }
    else if(request_type == CONNECT_TO_VOICE_CHANNEL)
    {
        int user_id = messageJson.value("user_id").toInt();
        int channel_id = messageJson.value("channel_id").toInt();

        QSqlQuery query;
        query.clear();
        query.prepare(R"(
            SELECT login FROM users
            WHERE id = :user_id
        )");
        query.bindValue(":user_id", user_id);
        query.exec();
        query.next();
        User user;
        user.id = user_id;
        user.name = query.value("login").toString();

        if(voiceChannels__.contains(channel_id))
        {
            voiceChannels__[channel_id].push_back(user);
        }
        else
        {
            voiceChannels__.insert(channel_id, QList<User>{user});
        }

        query.clear();
        query.prepare(R"(
            SELECT compadres_id, server_id FROM channels
            WHERE id = :channel_id
        )");
        query.bindValue(":channel_id", channel_id);
        query.exec();
        query.next();
        QJsonObject response{
            {"server_id", query.value("server_id").toInt()},
            {"compadres_id", query.value("compadres_id").toInt()},
            {"channel_id", channel_id},
        };

        query.clear();
        query.prepare(R"(
            SELECT DISTINCT u.id, u.login
            FROM Channels c
            JOIN Servers s ON c.server_id = s.id
            JOIN Server_User_relation sur ON s.id = sur.server_id
            JOIN Users u ON sur.user_id = u.id
            WHERE c.id = :channel_id;
        )");
        query.bindValue(":channel_id", channel_id);
        query.exec();
        while(query.next())
        {
            user_ids_to_send.insert(query.value(0).toInt());
        }

        QJsonObject users{
            {"id", user.id},
            {"login", user.name},
        };
        response.insert("users", users);

        messageJsonToSend = generateResponse(true, CONNECT_TO_VOICE_CHANNEL, response);
    }
    else if (request_type == SEND_WHISPER)
    {
        int author_id = messageJson.value("author_id").toInt();
        int target_id = messageJson.value("target_id").toInt();
        QString message = messageJson.value("message").toString();

        QSqlQuery query;

        QString sql = R"(
        WITH existing_compadre AS (
            SELECT id FROM compadres
            WHERE
                (user1_id = :author_id AND user2_id = :target_id)
                OR
                (user1_id = :target_id AND user2_id = :author_id)
            LIMIT 1
        ),
        inserted_compadre AS (
            INSERT INTO compadres (user1_id, user2_id, status)
            SELECT :author_id, :target_id, :friendshipState
            WHERE NOT EXISTS (SELECT 1 FROM existing_compadre)
            RETURNING id
        ),
        compadre_id_union AS (
            SELECT id FROM existing_compadre
            UNION ALL
            SELECT id FROM inserted_compadre
        ),
        existing_channel AS (
            SELECT id FROM channels
            WHERE compadres_id = (SELECT id FROM compadre_id_union)
            LIMIT 1
        ),
        inserted_channel AS (
            INSERT INTO channels (compadres_id, owner_id, is_voice, name)
            SELECT id, :author_id, false, 'whispers' FROM compadre_id_union
            WHERE NOT EXISTS (SELECT 1 FROM existing_channel)
            RETURNING id
        )
        SELECT
            (SELECT id FROM inserted_compadre) AS new_compadre_id,
            (SELECT id FROM existing_compadre) AS existing_compadre_id,
            (SELECT id FROM inserted_channel) AS new_channel_id,
            (SELECT id FROM existing_channel) AS existing_channel_id,
            u_target.login AS target_login,
            u_author.login AS author_login
        FROM users u_target, users u_author
        WHERE u_target.id = :target_id AND u_author.id = :author_id
    )";

        query.prepare(sql);
        query.bindValue(":author_id", author_id);
        query.bindValue(":target_id", target_id);
        query.bindValue(":friendshipState", FriendShipState::Chat);
        query.exec();

        if (query.next())
        {
            QVariant new_compadre_id = query.value("new_compadre_id");
            QVariant existing_compadre_id = query.value("existing_compadre_id");
            QVariant new_channel_id = query.value("new_channel_id");
            QVariant existing_channel_id = query.value("existing_channel_id");
            QString target_login = query.value("target_login").toString();
            QString author_login = query.value("author_login").toString();

            QJsonObject msg;
            QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

            int compadre_id = new_compadre_id.isNull() ? existing_compadre_id.toInt() : new_compadre_id.toInt();
            int channel_id = new_channel_id.isNull() ? existing_channel_id.toInt() : new_channel_id.toInt();

            // Отправка автору сообщения
            {
                user_ids_to_send.insert(author_id);

                if (!new_channel_id.isNull())
                {
                    insertTextMessage(author_id, author_login, new_channel_id.toInt(), TEXT, message, timestamp, msg);
                    QJsonObject response_params;
                    QJsonObject user_info{
                        {"user_id", target_id},
                        {"user_login", target_login},
                        {"compadres_id", compadre_id}
                    };
                    response_params.insert(QString::number(channel_id), user_info);
                    QJsonObject response{
                        {"personal_chats_list", response_params}
                    };

                    messageJsonToSend = generateResponse(true, GET_PERSONAL_CHATS_LIST, response);
                }
                else if (!existing_channel_id.isNull())
                {
                    messageJsonToSend = {
                        {REQUEST, MESSAGE},
                        {TYPE, TEXT},
                        {"server_id", 0},
                        {"channel_id", existing_channel_id.toInt()}
                    };
                    insertTextMessage(author_id, author_login, existing_channel_id.toInt(), TEXT, message, timestamp, msg);

                    messageJsonToSend["message"] = msg;
                }
                broadcastMessage(messageJsonToSend, user_ids_to_send);
            }
            // Отправка цели сообщения
            {
                user_ids_to_send.insert(target_id);

                if (!new_channel_id.isNull())
                {
                    QJsonObject response_params;
                    QJsonObject user_info{
                        {"user_id", author_id},
                        {"user_login", author_login},
                        {"compadres_id", compadre_id}
                    };
                    response_params.insert(QString::number(channel_id), user_info);
                    QJsonObject response{
                        {"personal_chats_list", response_params}
                    };

                    messageJsonToSend = generateResponse(true, GET_PERSONAL_CHATS_LIST, response);
                }
                else if (!existing_channel_id.isNull())
                {
                    messageJsonToSend = {
                        {REQUEST, MESSAGE},
                        {TYPE, TEXT},
                        {"server_id", 0},
                        {"channel_id", existing_channel_id.toInt()}
                    };

                    messageJsonToSend["message"] = msg;
                }
                broadcastMessage(messageJsonToSend, user_ids_to_send);
            }
        }
        return;
    }
    else if (request_type == GET_SERVER_SETTINGS)
    {
        int server_id = messageJson["server_id"].toInt();
        int user_id = messageJson["user_id"].toInt();
        user_ids_to_send.insert(user_id);
        QSqlQuery query;
        query.prepare(R"(
            SELECT
                i.id AS invite_id,
                i.roles_id AS role_id,
                i.author_id AS invite_author_id,
                i.URL AS invite_url,
                i.uses_left AS invite_uses_left,
                i.created_at AS invite_created_at,
                i.exprired_at AS invite_expired_at,

                r.name AS role_name,
                r.is_highest_permission AS role_is_highest,
                r.user_permissions AS role_user_permissions,
                r.admin_permissions AS role_admin_permissions,

                s.id AS server_id,
                s.invite_default_id AS server_invite_default_id,
                s.name AS server_name,
                s.description AS server_description

            FROM
                invite_to_server i
            JOIN
                Roles r ON i.roles_id = r.id
            JOIN
                Servers s ON r.server_id = s.id
            WHERE
                s.id = :server_id
        )");

        query.bindValue(":server_id", server_id);
        query.exec();

        QJsonObject invitesArray;

        while (query.next()) {
            QJsonObject inviteObject;

            inviteObject["invite_id"]        = query.value("invite_id").toInt();
            inviteObject["role_id"]          = query.value("role_id").toInt();
            inviteObject["invite_author_id"] = query.value("invite_author_id").toInt();
            inviteObject["invite_url"]       = query.value("invite_url").toString();
            inviteObject["invite_uses_left"] = query.value("invite_uses_left").isNull()
                                                   ? QJsonValue::Null
                                                   : QJsonValue(query.value("invite_uses_left").toInt());

            inviteObject["invite_created_at"] = query.value("invite_created_at").toDateTime().toString(Qt::ISODateWithMs);
            inviteObject["invite_expired_at"] = query.value("invite_expired_at").isNull()
                                                    ? QJsonValue::Null
                                                    : QJsonValue(query.value("invite_expired_at").toDateTime().toString(Qt::ISODateWithMs));

            inviteObject["role_name"]              = query.value("role_name").toString();
            inviteObject["role_is_highest"]        = query.value("role_is_highest").toBool();
            inviteObject["role_user_permissions"]  = query.value("role_user_permissions").toInt();
            inviteObject["role_admin_permissions"] = query.value("role_admin_permissions").toInt();

            inviteObject["server_id"]                = query.value("server_id").toInt();
            inviteObject["server_invite_default_id"] = query.value("server_invite_default_id").isNull()
                                                           ? QJsonValue::Null
                                                           : QJsonValue(query.value("server_invite_default_id").toInt());
            inviteObject["server_name"]              = query.value("server_name").toString();
            inviteObject["server_description"]       = query.value("server_description").toString();

            invitesArray[QString::number(query.value("invite_id").toInt())] = inviteObject;
        }

        QJsonObject result;
        result["invites"] = invitesArray;

        messageJsonToSend = generateResponse(true, GET_SERVER_SETTINGS, result);
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

bool ChatServer::insertTextMessage(int sender_id, QString &userName, int channel_id, const QString &message_type, QString &content, QString &created_at, QJsonObject &msgHandler)
{
    QSqlQuery insertQuery;

    // Подготовка JSON для content
    QJsonObject contentObj;
    contentObj["text"] = content; // Вставка текста под ключом "text"
    QString jsonContent = QJsonDocument(contentObj).toJson(QJsonDocument::Compact);

    insertQuery.prepare("INSERT INTO Message (author_id, channel_id, type, content, created_at) "
                        "VALUES (:author_id, :channel_id, :type, :content, :created_at) "
                        "RETURNING id");
    insertQuery.bindValue(":author_id", sender_id);
    insertQuery.bindValue(":channel_id", channel_id);
    insertQuery.bindValue(":type", message_type);
    insertQuery.bindValue(":content", jsonContent);
    insertQuery.bindValue(":created_at", created_at);

    if (insertQuery.exec() && insertQuery.next()) {
        int msg_id = insertQuery.value(0).toInt();
        QJsonObject msg = messageToJson(userName, contentObj, created_at);
        msg[TYPE] = message_type;
        msgHandler[QString::number(msg_id)] = msg;
        return true;
    } else {
        qWarning() << "Failed to insert message:" << insertQuery.lastError();
        return false;
    }
}

bool ChatServer::insertInviteMessage(int sender_id, QString &userName, int channel_id, const QString &message_type, int inv_id_, QString &server_name_, QString &created_at, QJsonObject &msgHandler)
{
    QSqlQuery insertQuery;

    QJsonObject contentObj;
    contentObj["invite_id"] = inv_id_;
    contentObj["server_name"] = server_name_;
    QString jsonContent = QJsonDocument(contentObj).toJson(QJsonDocument::Compact);

    insertQuery.prepare("INSERT INTO Message (author_id, channel_id, type, content, created_at) "
                        "VALUES (:author_id, :channel_id, :type, :content, :created_at) "
                        "RETURNING id");
    insertQuery.bindValue(":author_id", sender_id);
    insertQuery.bindValue(":channel_id", channel_id);
    insertQuery.bindValue(":type", message_type);
    insertQuery.bindValue(":content", jsonContent);
    insertQuery.bindValue(":created_at", created_at);

    if (insertQuery.exec() && insertQuery.next()) {
        int msg_id = insertQuery.value(0).toInt();
        QJsonObject msg = messageToJson(userName, contentObj, created_at);
        msg[TYPE] = message_type;
        msgHandler[QString::number(msg_id)] = msg;
        return true;
    } else {
        qWarning() << "Failed to insert message:" << insertQuery.lastError();
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
