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
    connect(networkManager__, &NetworkManager::updateUser, this, &ClientMain::on_updateUser);
    connect(networkManager__, &NetworkManager::acceptedFriendship, this, &ClientMain::on_friendshipAccepted);
    connect(networkManager__, &NetworkManager::addFriendRequest, this, &ClientMain::on_addFriendRequest);

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
        connect(server, &Server::sendFriendRequest, this, [&](int server_id_, int user_id_){
            QJsonObject messageJson{
                                    {REQUEST, CREATE_FRIENDSHIP},
                                    {"sender_id", userData__.user_id},
                                    {"server_id", server_id_},
                                    {"target_id", user_id_},
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

void ClientMain::on_updateUser(const QJsonObject &response)
{
    int userId = response["user_id"].toInt();
    UserState newState = (UserState)response["status"].toInt();
    QString userLogin = response["user_login"].toString();

    // Обновляем состояние на серверах
    for (auto& server : userServers__)
    {
        server->participantUpdateStatus(userId, newState);
    }

    // Поиск ячейки с именем пользователя в таблице
    int rows = ui__->tableWidget->rowCount();
    int cols = ui__->tableWidget->columnCount();
    int currentRow = -1;
    int currentColumn = -1;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            QTableWidgetItem* item = ui__->tableWidget->item(row, col);
            if (item && item->text() == userLogin)
            {
                currentRow = row;
                currentColumn = col;
                break;
            }
        }
        if (currentRow != -1)
            break;
    }

    if (currentRow == -1)
        return;

    // Перемещаем ячейку
    QTableWidgetItem* movedItem = ui__->tableWidget->takeItem(currentRow, currentColumn);
    ui__->tableWidget->setItem(currentRow, currentColumn, nullptr);

    // Проверяем, осталась ли строка пустой
    bool isRowEmpty = true;
    for (int col = 0; col < cols; ++col)
    {
        QTableWidgetItem* item = ui__->tableWidget->item(currentRow, col);
        if (item && !item->text().isEmpty())
        {
            isRowEmpty = false;
            break;
        }
    }

    if (isRowEmpty)
    {
        ui__->tableWidget->removeRow(currentRow);
        --rows; // строк стало на одну меньше
        if (currentRow < rows)
        {
            // перемещённая строка была выше — нужно сдвинуть все ниже
            if (currentRow < rows) {
                if (currentRow < rows)
                    rows = ui__->tableWidget->rowCount(); // обновим количество строк
            }
        }
    }

    // Ищем, куда вставить ячейку
    int targetColumn = static_cast<int>(newState);
    int insertRow = rows;

    for (int row = 0; row < rows; ++row)
    {
        QTableWidgetItem* item = ui__->tableWidget->item(row, targetColumn);
        if (!item || item->text().isEmpty())
        {
            insertRow = row;
            break;
        }
    }

    if (insertRow == rows)
    {
        ui__->tableWidget->setRowCount(rows + 1);
    }

    ui__->tableWidget->setItem(insertRow, targetColumn, movedItem);
}

void ClientMain::on_friendshipAccepted(const int id, const QString &username, int userState)
{
    if (userFriends__.contains(id))
        return;

    Participant *user = new Participant(id,
                                        username,
                                        (UserState)userState);
    userFriends__[id] = user;

    int stateColumn = user->getState();
    int rowCount = ui__->tableWidget->rowCount();
    bool needToInsert = true;

    // Проверка, все ли строки в нужном столбце заняты непустыми значениями
    for (int row = 0; row < rowCount; ++row)
    {
        QTableWidgetItem* item = ui__->tableWidget->item(row, stateColumn);
        if (!item || item->text().isEmpty())
        {
            needToInsert = false;
            break;
        }
    }

    // Если есть хотя бы одна пустая ячейка — вставляем туда, иначе вставляем новую строку
    if (needToInsert)
    {
        ui__->tableWidget->setRowCount(rowCount + 1);

        // Сдвигаем строки вниз в выбранном столбце
        for (int row = rowCount; row > 0; --row)
        {
            QTableWidgetItem* aboveItem = ui__->tableWidget->item(row - 1, stateColumn);
            if (aboveItem)
            {
                ui__->tableWidget->setItem(row, stateColumn, new QTableWidgetItem(*aboveItem));
            }
            else
            {
                ui__->tableWidget->setItem(row, stateColumn, new QTableWidgetItem(""));
            }
        }

        QTableWidgetItem* newItem = new QTableWidgetItem(username);
        ui__->tableWidget->setItem(0, stateColumn, newItem);
    }
    else
    {
        // Вставка в первую пустую ячейку
        for (int row = 0; row < rowCount; ++row)
        {
            QTableWidgetItem* item = ui__->tableWidget->item(row, stateColumn);
            if (!item || item->text().isEmpty())
            {
                ui__->tableWidget->setItem(row, stateColumn, new QTableWidgetItem(username));
                break;
            }
        }
    }
}

void ClientMain::on_addFriendRequest(const int id_, const QString &username_)
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);

    QLabel *label = new QLabel(username_, container);
    QPushButton *acceptButton = new QPushButton("Принять", container);

    connect(acceptButton, &QPushButton::clicked, this, [=]()
    {
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

    QVBoxLayout *scrollLayout = qobject_cast<QVBoxLayout *>(ui__->friendRequestsScrlAreaContents->layout());

    scrollLayout->insertWidget(0, container);
}

void ClientMain::handleTextMessageReceived(int server_id,
                                              int channel_id,
                                              const QString &userName,
                                              const QString &content,
                                              const QString& timestamp)
{
    userServers__[server_id]->getChannels()[channel_id]->setLastMessage(userName, content, timestamp);
}


void ClientMain::on_toFriendRequestsBtn_clicked()
{
    int currentIndex = ui__->stackedWidget->currentIndex();
    int nextIndex = (currentIndex + 1) % 2;
    ui__->toFriendRequestsBtn->setText(nextIndex == 0 ? ">" : "<");
    ui__->label_4->setText(nextIndex == 0 ? "Друзья" : "Заявки в друзья");
    ui__->stackedWidget->setCurrentIndex(nextIndex);
}

