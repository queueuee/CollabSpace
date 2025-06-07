#include "clientmain.h"
#include "./ui_clientmain.h"
#include "../possible_requests.h"

#include <QShortcut>

const QString programmName = "Collab Space";

// Класс ClientMain
ClientMain::ClientMain(QWidget *parent)
    : QMainWindow(parent),
    ui__(new Ui::ClientMain)
{
    ui__->setupUi(this);

    QShortcut *shortcutZoomIn = new QShortcut(QKeySequence("Ctrl+="), this);
    connect(shortcutZoomIn, &QShortcut::activated, this, &ClientMain::zoomIn);

    QShortcut *shortcutZoomOut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(shortcutZoomOut, &QShortcut::activated, this, &ClientMain::zoomOut);

    videoChatWindow__ = new VideoChatWindow();

    ui__->voiceDisconnectButton->setVisible(false);

    ui__->tabWidget->tabBar()->setTabVisible(2, false);
    networkManager__ = new NetworkManager(this);

    connect(networkManager__, &NetworkManager::userServerListRecived, this, &ClientMain::on_getUserServerList);
    connect(networkManager__, &NetworkManager::openServerListRecived, this, &ClientMain::on_getOpenServerList);
    connect(networkManager__, &NetworkManager::joinToServerAnswer, this, &ClientMain::on_joinToServerAnswer);
    connect(networkManager__, &NetworkManager::textMessageReceived, this, &ClientMain::handleTextMessageReceived);
    connect(networkManager__, &NetworkManager::leaveServerAnswer, this, &ClientMain::on_leaveServerAnswer);
    connect(networkManager__, &NetworkManager::deleteServerAnswer, this, &ClientMain::on_deleteServerAnswer);
    connect(networkManager__, &NetworkManager::serverParticipantsList, this, &ClientMain::on_serverParticipantsList);
    connect(networkManager__, &NetworkManager::updateUser, this, &ClientMain::on_updateUser);
    connect(networkManager__, &NetworkManager::acceptedFriendship, this, &ClientMain::on_friendshipAccepted);
    connect(networkManager__, &NetworkManager::addFriendRequest, this, &ClientMain::on_addFriendRequest);
    connect(networkManager__, &NetworkManager::personalChat, this, &ClientMain::on_addPersonalChat);
    connect(networkManager__, &NetworkManager::addUserToVoiceChannel, this, &ClientMain::addUserToVoiceChannel);
    connect(ui__->tabWidget, &CustomTabWidget::openSettings, this, &ClientMain::openServerSettings);
    connect(networkManager__, &NetworkManager::serverInviteData, this, &ClientMain::serverInviteData);

    // Авторизация
    Authorization auth(networkManager__);
    auth.setModal(true);
    if (auth.exec() == QDialog::Accepted)
    {
        userData__ = auth.getUserData();
        setWindowTitle(programmName + " - " + userData__.login);
        getUserServerList();
        getOpenServerList();
        getFriendRequests();
        getFriendsList();
        getPersonalChatsList();
    }
    else
    {
        QApplication::quit();
    }

    messages_to_download__ = 20;
    ui__->backToPersonalMsgsListBtn->setVisible(false);

    ui__->tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui__->mainToolBox->count();

    ui__->micOnOffButton->setFlat(micEnabled__);
    ui__->headersOnOffButton->setFlat(headphonesEnabled__);

    ui__->tableWidget->verticalHeader()->setVisible(false);
    ui__->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui__->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(ui__->tableWidget, &QTableWidget::cellDoubleClicked, this, [=](int row, int column){
        QTableWidgetItem* item = ui__->tableWidget->item(row, column);
        if (item)
        {
            QMessageBox::information(this, "Ячейка нажата",
                                     QString("Вы дважды кликнули на ячейку [%1, %2]: %3")
                                         .arg(row).arg(column).arg(item->text()));
        }
    });

}

ClientMain::~ClientMain()
{
    logOut();
    delete ui__;
}

void ClientMain::zoomIn()
{
    QFont font = this->font();
    int size = font.pointSize();
    font.setPointSize(size + 1);
    this->setFont(font);
}

void ClientMain::zoomOut()
{
    QFont font = this->font();
    int size = font.pointSize();
    if (size > 6)
        font.setPointSize(size - 1);
    this->setFont(font);
}

void ClientMain::startVoiceChat()
{
    //systemManager__ -> startVoiceChat();
    ui__->chatWindow->append("Voice chat started.");

    ui__->voiceDisconnectButton->setVisible(true);
    ui__->voiceConnectButton->setVisible(false);
}

void ClientMain::on_getUserServerList(const QJsonObject &info)
{
    for (const QString &server_id : info.keys())
    {
        if (userServers__.contains(server_id.toInt()))
            continue;
        QJsonObject serverData = info[server_id].toObject();

        // Извлекаем данные о каналах
        QJsonArray channelsArray = serverData.value("channels").toArray();
        QMap<int, QJsonObject> channelsMap;

        for (const QJsonValue &channelValue : channelsArray) {
            QJsonObject channelObject = channelValue.toObject();
            int channelId = channelObject.value("id").toInt();
            channelsMap.insert(channelId, channelObject);
        }

        // Создаем сервер с новыми параметрами
        Server *server = new Server(
            server_id.toInt(),
            serverData.value("server_name").toString(),
            serverData.value("server_desc").toString(),
            serverData.value("is_open").toBool(),
            QByteArray::fromBase64(serverData.value("server_img").toString().toUtf8()),
            serverData.value("role_name").toString(),
            serverData.value("is_highest_perm").toBool(),
            serverData.value("user_perm").toInt(),
            serverData.value("admin_perm").toInt(),
            channelsMap
            );

        connect(server, &Server::sendMessageFromChannel, this, &ClientMain::on_sendMessageFromChannel);

        connect(server, &Server::getMessagesList, this, &ClientMain::on_getMessagesList);

        connect(server, &Server::deleteServerSignal, this, [&](int id_){
            QJsonObject messageJson{
                                    {REQUEST, DELETE_SERVER},
                                    {"user_id", userData__.user_id},
                                    {"server_id", id_},
                                    };
            networkManager__->sendMessageJsonToServer(messageJson);
        });

        connect(server, &Server::leaveServerSignal, this, [&](int id_){
            QJsonObject messageJson{
                                    {REQUEST, LEAVE_SERVER},
                                    {"user_id", userData__.user_id},
                                    {"server_id", id_},
                                    };
            networkManager__->sendMessageJsonToServer(messageJson);
        });

        for (auto &voiceChannel : server->voiceChannels())
        {
            connect(voiceChannel, &Channel::connectToVoiceChannel, this, &ClientMain::connectToVoiceChannel);
            connect(voiceChannel, &Channel::disconnectFromVoiceChannel, this, &ClientMain::disconnectFromVoiceChannel);
            connect(voiceChannel, &Channel::startVideoChat, this, &ClientMain::on_startVideoChatBtn);
            connect(voiceChannel, &Channel::connectToVideoChat, this, &ClientMain::on_connectToVideoChatBtn);
        }

        userServers__[server_id.toInt()] = server;

        ui__->tabWidget->insertServerTab(server, server_id.toInt());
        // Привязать к сигналу смены таба (чтобы срабатывало при переключении на сервер)
        getServerParticipantsList(server_id.toInt());
    }
}
void ClientMain::on_joinToServerAnswer(const QJsonObject &info)
{
    getUserServerList();
}

void ClientMain::on_leaveServerAnswer(const QJsonObject &info)
{
    ui__->tabWidget->clearTab();
    // Утечка памяти мб
    userServers__.clear();
    getUserServerList();
}

void ClientMain::on_deleteServerAnswer(const QJsonObject &info)
{
    clearOpenServers();
    ui__->tabWidget->clearTab();
    // Утечка памяти мб
    userServers__.clear();
    getUserServerList();
}

void ClientMain::clearOpenServers()
{
    QLayout* layout = ui__->publicChannelsHandler_scrollbar->layout();
    if (layout) {
        int count = layout->count();
        for (int i = 0; i < count - 1; ++i) {
            QLayoutItem* item = layout->takeAt(0);
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
    }
    openServers__.clear();
    getOpenServerList();
}

void ClientMain::logOut()
{
    QJsonObject logoutRequest{
        {REQUEST, LOGOUT_USER},
        {"user_id", userData__.user_id},
        {"user_login", userData__.login},
    };

    networkManager__->sendMessageJsonToServer(logoutRequest);
}


void ClientMain::on_getOpenServerList(const QJsonObject &info)
{
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(ui__->publicChannelsHandler_scrollbar->layout());
    for (const QString &server_id : info.keys())
    {
        if (openServers__.contains(server_id.toInt()))
            continue;

        QJsonObject serverData = info[server_id].toObject();

        ShortServer *a = new ShortServer(server_id.toInt(),
                                         serverData["invite_id"].toInt(),
                                         serverData["server_name"].toString(),
                                         serverData["server_desc"].toString(),
                                         QByteArray::fromBase64(serverData.value("server_img").toString().toUtf8()));
        QPushButton *joinServerBtn = a->getJoinBtn();
        connect(a, &ShortServer::serverShortInfo, this, [=](const QString &name_, const QString &desc_){
            ui__->serverName_label->setText(name_);
            ui__->serverInfo_textEdit->setPlainText(desc_);

            disconnect(ui__->joinServer_pushButton, nullptr, nullptr, nullptr);
            connect(ui__->joinServer_pushButton, &QPushButton::clicked, this, [=]() {
                joinServerBtn->click();
            });
        });
        connect(a, &ShortServer::acceptInvite, this, &ClientMain::on_joinServer);

        openServers__[server_id.toInt()] = a;
        layout->insertWidget(layout->count() - 1, a->getWidget());
    }
}

void ClientMain::on_joinServer(int id_)
{
    QJsonObject joinServerRequest{
        {REQUEST, JOIN_SERVER},
        {"user_id", userData__.user_id},
        {"invite_id", id_}
    };

    networkManager__->sendMessageJsonToServer(joinServerRequest);
}

void ClientMain::connectToVoiceChannel(int id)
{
    QJsonObject connectToChannelRequest{
        {REQUEST, CONNECT_TO_VOICE_CHANNEL},
        {"user_id", userData__.user_id},
        {"channel_id", id},
    };

    networkManager__->sendMessageJsonToServer(connectToChannelRequest);
}

void ClientMain::disconnectFromVoiceChannel(int id)
{
    QJsonObject logoutRequest{
        {REQUEST, DISCONNECT_FROM_VOICE_CHANNEL},
        {"user_id", userData__.user_id},
        {"channel_id", id},
    };

    networkManager__->sendMessageJsonToServer(logoutRequest);
}

void ClientMain::addUserToVoiceChannel(const int user_id, const QString &username, const int server_id, const int compadres_id, const int channel_id)
{
    if (server_id > 0)
    {
        userServers__[server_id]->getChannels()[channel_id]->addUserToVoice(relatedUsers__[user_id]);
        return;
    }
}

void ClientMain::on_startVideoChatBtn(int channel_id)
{
    videoChatWindow__->startVideo("0.0.0.0", "5002");
    videoChatWindow__->show();
}

void ClientMain::on_connectToVideoChatBtn(int channel_id, int user_id)
{
    if (user_id == userData__.user_id)
        return;

    videoChatWindow__->addVideo(1, "user2", "0.0.0.0", "5002");
    videoChatWindow__->show();
}

void ClientMain::openServerSettings(int server_id_, const QString &server_name_)
{
    QJsonObject settingsRequest{
                              {REQUEST, GET_SERVER_SETTINGS},
                              {"user_id", userData__.user_id},
                              {"server_id", server_id_},
                              };

    networkManager__->sendMessageJsonToServer(settingsRequest);
    serverSettings__ = new serverSettings(server_name_);
    serverSettings__->show();
}

void ClientMain::serverInviteData(const QJsonObject invData_)
{
    if (serverSettings__)
        serverSettings__->addIvite(invData_);

    auto inviteTemplate = serverSettings__->getInviteList();
    int server_id = invData_["server_id"].toInt();

    for (auto &key : inviteTemplate.keys())
    {
        connect(inviteTemplate[key]->getSendInvBtn(), &QPushButton::clicked, this, [=](){
            QDialog *dl = new QDialog();
            dl->setMinimumSize(400, 300);
            dl->setLayout(new QVBoxLayout());
            for(auto &user : relatedUsers__)
            {
                if (user->getId() == userData__.user_id)
                    continue;
                if(!user->getSharedServers().contains(server_id))
                {
                    QWidget *userInvHandler = new QWidget();
                    userInvHandler->setLayout(new QHBoxLayout());
                    QLabel *userNameLbl = new QLabel(user->getLogin());
                    QPushButton *inviteBtn = new QPushButton("Отправить");
                    connect(inviteBtn, &QPushButton::clicked, this, [=](){
                        QString server_name = userServers__[server_id]->getName();
                        on_sendInviteClicked(user->getId(), server_name, invData_);
                    });
                    userInvHandler->layout()->addWidget(userNameLbl);
                    userInvHandler->layout()->addWidget(inviteBtn);

                    dl->layout()->addWidget(userInvHandler);
                }
            }
            QSpacerItem *spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
            dl->layout()->addItem(spacer);
            dl->exec();
        });
    }
}

void ClientMain::on_sendInviteClicked(int target_id_, QString &serverName, const QJsonObject invData_)
{
    qDebug() << target_id_;
    QJsonObject messageJson{
                            {REQUEST, MESSAGE},
                            {TYPE, INVITE},
                            {"target_id", target_id_},
                            {"user_id", userData__.user_id},
                            {"serverName", serverName},
                            {"invite_id", invData_["invite_id"].toInt()},
                            };
    networkManager__->sendMessageJsonToServer(messageJson);
}

void ClientMain::leaveVoiceChat()
{
    //systemManager__->leaveVoiceChat();
    ui__->chatWindow->append("Voice chat stopped.");

    ui__->voiceDisconnectButton->setVisible(false);
    ui__->voiceConnectButton->setVisible(true);
}

void ClientMain::handleConnectionFailed()
{
    QMessageBox::warning(this, "Connection Error", "Failed to connect to the server.");
}

void ClientMain::on_createServerButton_clicked()
{
    createServer(ui__->serverName_lineEdit->text(),
                  ui__->serverDesc_textEdit->toPlainText(),
                  ui__->numOfVoiceChannels_spinBox->value(),
                  ui__->numOfTextChannels_spinBox->value(),
                  ui__->isOpenServer);
    getUserServerList();
    getOpenServerList();
}
void ClientMain::createServer(const QString &name_,
                                 const QString &description_,
                                 const int num_of_voiceChannels_,
                                 const int num_of_textChannels_,
                                 bool is_open_)
{
    QJsonObject createServerRequest{
        {REQUEST, CREATE_SERVER},
        {"owner_id", userData__.user_id},
        {"name", name_},
        {"description", description_},
        {"num_of_text", num_of_textChannels_},
        {"num_of_voice", num_of_voiceChannels_},
        {"is_open", is_open_},
        {"icon_img", "no"}
    };
    networkManager__->sendMessageJsonToServer(createServerRequest);
}

void ClientMain::getUserServerList()
{
    QJsonObject getServerListRequest{
        {REQUEST, GET_USER_SERVERS_LIST},
        {"user_id", userData__.user_id}
    };

    networkManager__->sendMessageJsonToServer(getServerListRequest);
}

void ClientMain::getOpenServerList()
{
    QJsonObject getServerListRequest{
        {REQUEST, GET_OPEN_SERVERS_LIST},
        {"user_id", userData__.user_id}
    };

    networkManager__->sendMessageJsonToServer(getServerListRequest);
}

void ClientMain::getServerParticipantsList(int id_)
{
    QJsonObject fillServerRequest{
        {REQUEST, GET_SERVER_PARTICIPANTS_LIST},
        {"user_id", userData__.user_id},
        {"server_id", id_}
    };

    networkManager__->sendMessageJsonToServer(fillServerRequest);
}

void ClientMain::getFriendRequests()
{
    QJsonObject getFriendRequests{
        {REQUEST, GET_FRIEND_REQUESTS},
        {"user_id", userData__.user_id},
    };

    networkManager__->sendMessageJsonToServer(getFriendRequests);
}

void ClientMain::getFriendsList()
{
    QJsonObject getFriendList{
        {REQUEST, GET_FRIEND_LIST},
        {"user_id", userData__.user_id},
    };

    networkManager__->sendMessageJsonToServer(getFriendList);
}

void ClientMain::getPersonalChatsList()
{
    QJsonObject getPersonalMessagesList{
        {REQUEST, GET_PERSONAL_CHATS_LIST},
        {"user_id", userData__.user_id},
    };

    networkManager__->sendMessageJsonToServer(getPersonalMessagesList);
}

void ClientMain::on_serverParticipantsList(const QJsonObject &info)
{
    const QJsonObject users_list = info["users_list"].toObject();
    for (const QString &participant_id : users_list.keys())
    {
        QJsonObject participantInfo = users_list[participant_id].toObject();
        if (!relatedUsers__.contains(participant_id.toInt()))
        {
            UserProfile *user = new UserProfile(participant_id.toInt(),
                                                participantInfo["user_login"].toString(),
                                                (UserState)participantInfo["user_status"].toInt(),
                                                (FriendShipState)participantInfo["friend_status"].toInt());
            relatedUsers__[participant_id.toInt()] = user;
        }
        connect(relatedUsers__[participant_id.toInt()], &UserProfile::sendFriendRequest, this, &ClientMain::on_sendFriendRequest);

        userServers__[info["server_id"].toInt()]->participantAdd(relatedUsers__[participant_id.toInt()]);
    }
}

void ClientMain::on_updateUser(const QJsonObject &response)
{
    int userId = response["user_id"].toInt();
    UserState newState = (UserState)response["status"].toInt();
    QString userLogin = response["user_login"].toString();

    if (!relatedUsers__.contains(userId))
        return;
    relatedUsers__[userId]->setState(newState);
}

void ClientMain::on_friendshipAccepted(const int id, const QString &username, int userState, const QJsonArray &commonServers)
{
    if (!relatedUsers__.contains(id))
    {
        UserProfile *user = new UserProfile(id,
                                            username,
                                            (UserState)userState,
                                            FriendShipState::Accepted);
        connect(user, &UserProfile::sendWhisper, this, &ClientMain::on_sendWhisper);
        relatedUsers__[id] = user;
    }
    if (!commonServers.empty())
        for (auto server : commonServers)
        {
            QString serverName = server["name"].toString();
            relatedUsers__[id]->addSharedServer(server["id"].toInt(), serverName);
        }
    connect(relatedUsers__[id], &UserProfile::statusUpdate, ui__->tableWidget, [=](int id_, UserState state_){
        ui__->tableWidget->on_friendStatusUpdate(id_, state_, username);
    });

    ui__->tableWidget->addFriend(relatedUsers__[id]);
}

void ClientMain::on_addFriendRequest(const int id_, const QString &username_)
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);

    QLabel *label = new QLabel(username_, container);
    QPushButton *acceptButton = new QPushButton("Принять", container);
    QPushButton *rejectButton = new QPushButton("Отклонить", container);

    connect(acceptButton, &QPushButton::clicked, this, [=]()
    {
        container->deleteLater();
        QJsonObject acceptFriendshipRequest{
            {REQUEST, ACCEPT_FRIENDSHIP},
            {"accepter_id", userData__.user_id},
            {"sender_id", id_}
        };

        networkManager__->sendMessageJsonToServer(acceptFriendshipRequest);
    });

    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(acceptButton);
    layout->addWidget(rejectButton);


    QVBoxLayout *scrollLayout = qobject_cast<QVBoxLayout *>(ui__->friendRequestsScrlAreaContents->layout());

    scrollLayout->insertWidget(0, container);
}

void ClientMain::on_addPersonalChat(const int channel_id, const QString &username, const int user_id, const int compadres_id)
{
    if (personalMessages__.contains(channel_id))
        return;

    personalMessages__[channel_id] = new Channel(channel_id, compadres_id, "someName", false, QDateTime::currentDateTimeUtc());

    connect(personalMessages__[channel_id], &Channel::sendMessageFromChannel, this, &ClientMain::on_sendMessageFromChannel);

    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);

    QPushButton *toChatBtn = new QPushButton(username, container);
    connect(toChatBtn, &QPushButton::clicked, this, [=](){
        on_getMessagesList(channel_id);

        int currentIndex = ui__->personalMessagesStackedWidget->currentIndex();
        int nextIndex = (currentIndex + 1) % 2;
        ui__->backToPersonalMsgsListBtn->setVisible(true);
        ui__->label_2->setText(username);
        ui__->personalMessagesStackedWidget->setCurrentIndex(nextIndex);

        personalMessages__[channel_id]->getWidget()->setVisible(true);
    });

    layout->addWidget(toChatBtn);

    QVBoxLayout *scrollLayout = qobject_cast<QVBoxLayout *>(ui__->personalMessagesListScrollAreaWidgetContents->layout());

    scrollLayout->insertWidget(0, container);

    QVBoxLayout *page6Layout = qobject_cast<QVBoxLayout *>(ui__->page_6->layout());
    if (!page6Layout)
        page6Layout = new QVBoxLayout();

    page6Layout->addWidget(personalMessages__[channel_id]->getWidget());
    personalMessages__[channel_id]->getWidget()->setVisible(false);
}

void ClientMain::on_sendWhisper(const int target_id, const QString &message_)
{
    QJsonObject whisperJson{
        {REQUEST, SEND_WHISPER},
        {"author_id", userData__.user_id},
        {"target_id", target_id},
        {"message", message_},
    };
    networkManager__->sendMessageJsonToServer(whisperJson);
}

void ClientMain::on_sendMessageFromChannel(const int channel_id_, const int mess_type_, const QString &message_)
{
    if (!message_.isEmpty())
    {
        QJsonObject messageJson{
                                {REQUEST, MESSAGE},
                                {TYPE, TEXT},
                                {"channel_id", channel_id_},
                                {"user_id", userData__.user_id},
                                {"content", message_},
                                };
        networkManager__->sendMessageJsonToServer(messageJson);
    }
}

void ClientMain::on_getMessagesList(const int channel_id_)
{
    QJsonObject messageJson{
                            {REQUEST, GET_MESSAGES_LIST},
                            {"channel_id", channel_id_},
                            {"user_id", userData__.user_id},
                            {"num_of_msgs", messages_to_download__},
                            };
    networkManager__->sendMessageJsonToServer(messageJson);
}

void ClientMain::on_sendFriendRequest(const int user_id_)
{
    QJsonObject messageJson{
        {REQUEST, CREATE_FRIENDSHIP},
        {"sender_id", userData__.user_id},
        {"target_id", user_id_},
    };
    networkManager__->sendMessageJsonToServer(messageJson);
}

void ClientMain::handleTextMessageReceived(int server_id,
                                           int channel_id,
                                           int msg_id,
                                           const QString &type_,
                                           const QString &userName,
                                           const QJsonObject &content,
                                           const QString& timestamp)
{    
    if (server_id > 0)
    {
        userServers__[server_id]->getChannels()[channel_id]->setLastMessage(msg_id, type_, userName, content, timestamp);
        return;
    }

    personalMessages__[channel_id]->setLastMessage(msg_id, type_, userName, content, timestamp);
}


void ClientMain::on_toFriendRequestsBtn_clicked()
{
    int currentIndex = ui__->stackedWidget->currentIndex();
    int nextIndex = (currentIndex + 1) % 2;
    ui__->toFriendRequestsBtn->setText(nextIndex == 0 ? ">" : "<");
    ui__->label_4->setText(nextIndex == 0 ? "Друзья" : "Заявки в друзья");
    ui__->stackedWidget->setCurrentIndex(nextIndex);
}


void ClientMain::on_backToPersonalMsgsListBtn_clicked()
{
    int currentIndex = ui__->personalMessagesStackedWidget->currentIndex();
    int nextIndex = (currentIndex + 1) % 2;
    ui__->backToPersonalMsgsListBtn->setVisible(false);
    ui__->label_2->setText("Сообщения");
    ui__->personalMessagesStackedWidget->setCurrentIndex(nextIndex);

    for (auto &key : personalMessages__) {
        key->getWidget()->setVisible(false);
    }
}

