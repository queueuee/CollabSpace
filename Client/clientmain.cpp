#include "clientmain.h"
#include "./ui_clientmain.h"
#include "../possible_requests.h"

const QString programmName = "Collab Space";

// Класс ClientMain
ClientMain::ClientMain(QWidget *parent)
    : QMainWindow(parent),
    ui__(new Ui::ClientMain)
{
    ui__->setupUi(this);
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

    // Авторизация
    Authorization auth(networkManager__);
    auth.setModal(true);
    if (auth.exec() == QDialog::Accepted)
    {
        userData__ = auth.getUserData();
        setWindowTitle(programmName + " - " + userData__.login);
        getUserServerList();
        getOpenServerList();
    }
    else
    {
        QApplication::quit();
    }

    messages_to_download__ = 20;
    ui__->tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui__->mainToolBox->count();

    ui__->micOnOffButton->setFlat(micEnabled__);
    ui__->headersOnOffButton->setFlat(headphonesEnabled__);
}

ClientMain::~ClientMain()
{
    delete ui__;
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

        connect(server, &Server::sendMessageFromChannel, this, [&](int channel_id_, int mess_type_, const QString &message_){
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
        });

        connect(server, &Server::getMessagesList, this, [&](int channel_id_){
            QJsonObject messageJson{
                                    {REQUEST, GET_MESSAGES_LIST},
                                    {"channel_id", channel_id_},
                                    {"user_id", userData__.user_id},
                                    {"num_of_msgs", messages_to_download__},
                                    };
            networkManager__->sendMessageJsonToServer(messageJson);
        });

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


void ClientMain::leaveVoiceChat()
{
    //systemManager__->leaveVoiceChat();
    ui__->chatWindow->append("Voice chat stopped.");

    ui__->voiceDisconnectButton->setVisible(false);
    ui__->voiceConnectButton->setVisible(true);
}

void ClientMain::on_micOnOffButton_clicked()
{
    micEnabled__ = !micEnabled__;
    //systemManager__ -> onOffMicrophone(micEnabled__);
    ui__->micOnOffButton->setFlat(micEnabled__);
    ui__->micOnOffButton->setIcon(micEnabled__ ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));

    if (micEnabled__ && !headphonesEnabled__)
    {
        headphonesEnabled__ = true;
    //    systemManager__ -> onOffHeadphones(headphonesEnabled__);
        ui__->headersOnOffButton->setFlat(headphonesEnabled__);
        ui__->headersOnOffButton->setIcon(headphonesEnabled__ ? QIcon(":/icons/headers.png") : QIcon(":/icons/headersOff.png"));
    }
}

void ClientMain::on_headersOnOffButton_clicked()
{
    headphonesEnabled__ = !headphonesEnabled__;
    //systemManager__ -> onOffHeadphones(headphonesEnabled__);
    ui__->headersOnOffButton->setFlat(headphonesEnabled__);
    ui__->headersOnOffButton->setIcon(headphonesEnabled__ ? QIcon(":/icons/headers.png") : QIcon(":/icons/headersOff.png"));

    if (!headphonesEnabled__ && micEnabled__)
    {
        micEnabled__ = false;
    //    systemManager__ -> onOffMicrophone(micEnabled__);
        ui__->micOnOffButton->setFlat(micEnabled__);
        ui__->micOnOffButton->setIcon(micEnabled__ ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));
    }
    if (headphonesEnabled__ && !micEnabled__)
    {
        micEnabled__ = true;
    //    systemManager__ -> onOffMicrophone(micEnabled__);
        ui__->micOnOffButton->setFlat(micEnabled__);
        ui__->micOnOffButton->setIcon(micEnabled__ ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));
    }
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

void ClientMain::on_serverParticipantsList(const QJsonObject &info)
{
    const QJsonObject users_list = info["users_list"].toObject();
    for (const QString &participant_id : users_list.keys())
    {
        QJsonObject participantInfo = users_list[participant_id].toObject();
        Participant *user = new Participant(participant_id.toInt(),
                                            participantInfo["user_login"].toString(),
                                            (UserState)participantInfo["user_status"].toInt());
        userServers__[info["server_id"].toInt()]->participantAdd(user);
    }
}

void ClientMain::handleTextMessageReceived(int server_id,
                                              int channel_id,
                                              const QString &userName,
                                              const QString &content,
                                              const QString& timestamp)
{
    userServers__[server_id]->getChannels()[channel_id]->setLastMessage(userName, content, timestamp);
}
