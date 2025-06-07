#include "systemmanager.h"
#include "../possible_requests.h"

#include <gst/video/videooverlay.h>

// Класс Participant
UserProfile::UserProfile(int id_, QString login_, UserState status_, FriendShipState friendshipState_) :
    id__(id_),
    login__(login_),
    state__(status_),
    friendshipState__(friendshipState_)
{
    setWindowTitle("Профиль " + login__);
    setMinimumSize(QSize(100, 200));

    mainWidget__ = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(mainWidget__);
    mainWidget__->setLayout(mainLayout);

    messageEdit__ = new QLineEdit();

    if (friendshipState__ == FriendShipState::NotFriends || friendshipState__ == FriendShipState::Chat)
    {
        addFriendBtn__ = new QPushButton("+");
        addFriendBtn__->setMaximumWidth(30);
        connect(addFriendBtn__, &QPushButton::clicked, this, [=](){
            emit sendFriendRequest(id__);
        });
        mainLayout->addWidget(addFriendBtn__);
    }

    sendMsgBtn__ = new QPushButton("->");
    sendMsgBtn__->setMaximumWidth(30);
    connect(sendMsgBtn__, &QPushButton::clicked, this, [=](){
        emit sendWhisper(id__, messageEdit__->text());
        messageEdit__->clear();
    });

    mainLayout->addWidget(messageEdit__);
    mainLayout->addWidget(sendMsgBtn__);

    setLayout(mainLayout);
}

void UserProfile::showProfile()
{
    this->show();
}

// Класс HorizontalTabStyle
QSize HorizontalTabStyle::sizeFromContents(ContentsType type, const QStyleOption* option,
                                           const QSize& size, const QWidget* widget) const
{
    if (type == CT_TabBarTab)
    {
        const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (tab)
        {
            const QFontMetrics& fm = option->fontMetrics;
            int maxWidth = 100;
            QRect textRect = fm.boundingRect(QRect(0, 0, maxWidth, 100),
                                             Qt::TextWordWrap, tab->text);
            QSize newSize = textRect.size() + QSize(20, 20);
            return newSize;
        }
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

void HorizontalTabStyle::drawControl(ControlElement element, const QStyleOption* option,
                                     QPainter* painter, const QWidget* widget) const
{
    if (element == CE_TabBarTabLabel)
    {
        const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (tab && tab->shape == QTabBar::RoundedWest)
        {
            QStyleOptionTab modifiedTab(*tab);
            modifiedTab.shape = QTabBar::RoundedNorth;

            painter->save();
            QRect rect = modifiedTab.rect;
            QTextOption textOption;
            textOption.setWrapMode(QTextOption::WordWrap);
            textOption.setAlignment(Qt::AlignCenter);

            painter->setFont(widget ? widget->font() : QApplication::font());
            painter->setPen(modifiedTab.palette.color(QPalette::WindowText));
            painter->drawText(rect.adjusted(5, 5, -5, -5), modifiedTab.text, textOption);
            painter->restore();
            return;
        }
    }

    QProxyStyle::drawControl(element, option, painter, widget);
}


// Класс CustomTabWidget
CustomTabWidget::CustomTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabPosition(QTabWidget::West);
    tabBar()->setStyle(new HorizontalTabStyle());
    connect(tabBar(), &QTabBar::customContextMenuRequested,
            this, &CustomTabWidget::onTabContextMenuRequested);
}

void CustomTabWidget::insertServerTab(Server* server, int serverId)
{
    if (serverIds_.contains(serverId))
        return;

    QString title = server->getName();
    QWidget* content = server->getWidget();

    int serverInsertIndex = count() - 2;
    if (serverInsertIndex < 1) serverInsertIndex = 1;

    insertTab(serverInsertIndex, content, title);
    servers_.insert(serverInsertIndex - 1, server);
    serverIds_.insert(serverId, server);
}

Server* CustomTabWidget::getServer(int id) const
{
    return serverIds_.value(id, nullptr);
}

void CustomTabWidget::clearTab()
{
    while(3 < count()){
        removeServerTab(1);
    }
}

void CustomTabWidget::onTabContextMenuRequested(const QPoint& pos)
{
    int index = tabBar()->tabAt(pos);
    if (index <= 0 || index >= count() - 1)
        return;

    Server* server = servers_.value(index - 1, nullptr);
    if (!server) return;

    QMenu menu;
    QAction* leaveAction = menu.addAction("Покинуть сервер");
    QAction* settingsAction = menu.addAction("Настроить сервер");
    QAction* deleteAction = nullptr;
    if (server->canDeleteServer())
        deleteAction = menu.addAction("Удалить сервер");
    QAction* renameAction = menu.addAction("Переименовать...");

    QAction* selected = menu.exec(tabBar()->mapToGlobal(pos));
    if (selected == leaveAction)
    {
        server->leaveServer();
    }
    else if (selected == settingsAction)
    {
        const QString serverName = server->getName();
        emit openSettings(server->getId(), serverName);
    }
    else if (selected == deleteAction)
    {
        server->deleteServer();
    }
    else if (selected == renameAction)
    {
        bool ok;
        QString newName = QInputDialog::getText(this, "Переименовать сервер",
                                                "Новое имя:",
                                                QLineEdit::Normal,
                                                tabText(index), &ok);
        if (ok && !newName.isEmpty())
            setTabText(index, newName);
    }
}

void CustomTabWidget::removeServerTab(int index)
{
    if (index <= 0 || index >= count() - 1)
        return;

    Server* server = servers_.value(index - 1, nullptr);
    if (!server) return;

    removeTab(index);
    serverIds_.remove(serverIds_.key(server));
    servers_.removeAt(index - 1);
    server->deleteLater();
}


// Класс ShortServer
ShortServer::ShortServer(int id_,
                         int defaultInviteId_,
                         const QString &name_,
                         const QString &description_,
                         QByteArray img_) :
    id__(id_),
    defaultInviteId__(defaultInviteId_),
    name__(name_),
    description__(description_),
    img__(img_)
{
    mainWidget__ = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    QPushButton *pbt = new QPushButton(name__);
    connect(pbt, &QPushButton::clicked, this, [=](){
        emit serverShortInfo(name__, description_);
    });
    pbt->setFlat(true);
    mainLayout->addWidget(pbt);

    joinServerBtn__ = new QPushButton();
    connect(joinServerBtn__, &QPushButton::clicked, [=](){
        emit acceptInvite(defaultInviteId__);
    });

    mainWidget__->setLayout(mainLayout);
}


// Класс Server
Server::Server(int id_,
               const QString &name_,
               const QString &description_,
               bool is_open_,
               QByteArray img_,
               QString roleName_,
               bool is_highest_perm_,
               int user_perm_,
               int admin_perm_,
               const QMap<int, QJsonObject> &channels_) :
    ShortServer(id_, 0, name_, description_, img_),
    is_open__(is_open_),
    roleName__(roleName_),
    is_highest_perm__(is_highest_perm_),
    user_perm__(user_perm_),
    admin_perm__(admin_perm_)
{
    mainWidget__ = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(this);


    // Текстовые каналы
    QTabWidget *textTabWidget = new QTabWidget();
    textTabWidget->setObjectName("textChannels_tabWidget");
    // Вкладка "Текстовые каналы"
    QWidget *textChannelsTab = new QWidget();
    QVBoxLayout *textChannelsLayout = new QVBoxLayout(textChannelsTab);


    // Голосовые каналы
    QTabWidget *voiceTabWidget = new QTabWidget();
    voiceTabWidget->setObjectName("voiceChannels_tabWidget");
    // Вкладка "Голосовые каналы"
    QWidget *voiceChannelsListTab = new QWidget();
    QVBoxLayout *voiceChannelsListLayout = new QVBoxLayout(voiceChannelsListTab);
    for (auto it = channels_.begin(); it != channels_.end(); ++it)
    {
        int channel_id = it.key();
        QJsonObject channelData = it.value();

        bool isVoice = channelData.value("is_voice").toBool();
        QString channelName = channelData.value("name").toString();

        Channel *channel = new Channel(channel_id, 0, channelName, isVoice, QDateTime::currentDateTime());
        QPushButton *channelButton = new QPushButton(channelName);
        channels__[channel_id] = channel;

        QTabWidget *targetTabWidget = isVoice ? voiceTabWidget : textTabWidget;
        QVBoxLayout *targetLayout = isVoice ? voiceChannelsListLayout : textChannelsLayout;

        targetLayout->addWidget(channelButton);

        QObject::connect(channelButton, &QPushButton::clicked, [=]()
        {
            for (int i = 0; i < targetTabWidget->count(); ++i) {
                if (targetTabWidget->tabText(i) == channelName) {
                    targetTabWidget->setCurrentIndex(i);
                    return;
                }
            }

            if (targetTabWidget->count() == 3) {
                QWidget *oldTab = targetTabWidget->widget(1);
                targetTabWidget->removeTab(1);
            }

            targetTabWidget->insertTab(1, channel->getWidget(), channelName);
            targetTabWidget->setCurrentIndex(1);

        });


        connect(channel, &Channel::sendMessageFromChannel, this ,[&](int channel_id_,
                                                                     int mess_type_,
                                                                     const QString &message_){
            emit sendMessageFromChannel(channel_id_, mess_type_, message_);
        });

        if(!isVoice)
            connect(channelButton, &QPushButton::clicked, this ,[=](){
                emit getMessagesList(channel_id);
            });
    }

    voiceChannelsListLayout->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    voiceTabWidget->addTab(voiceChannelsListTab, "Голосовые каналы");

    QWidget *createVoiceChannelTab = new QWidget();
    voiceTabWidget->addTab(createVoiceChannelTab, "+");


    textChannelsLayout->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    textTabWidget->addTab(textChannelsTab, "Текстовые каналы");

    QWidget *createTextChannelTab = new QWidget();
    textTabWidget->addTab(createTextChannelTab, "+");

    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    splitter->addWidget(voiceTabWidget);
    splitter->addWidget(textTabWidget);

    // Участники
    auto *participantsGroup = createParticipantsGroup();
    splitter->addWidget(participantsGroup);

    mainLayout->addWidget(splitter);
    mainWidget__->setLayout(mainLayout);
}

QMap<int, Channel *> Server::voiceChannels()
{
    QMap<int, Channel *> voiceChannels;
    for (auto &channel : channels__)
    {
        if(channel->isVoice())
            voiceChannels.insert(channel->getID(), channel);
    }

    return voiceChannels;
}

void Server::participantAdd(UserProfile *user_)
{
    if (participants__.contains(user_->getId()))
    {
        // Если пользователь есть, то нужно обновлять по нему данные
        return;
    }

    QPushButton *userBtn = new QPushButton(user_->getLogin());
    participants__[user_->getId()] = userBtn;
    userBtn->setFlat(true);

    connect(user_, &UserProfile::statusUpdate, this, &Server::on_userStatusUpdate);
    connect(userBtn, &QPushButton::clicked, user_, &UserProfile::showProfile);

    switch(user_->getState()){
    case UserState::Online:
        onlineLayout__->addWidget(userBtn);
        break;
    default:
        offlineLayout__->addWidget(userBtn);
        break;
    }
}

QGroupBox *Server::createParticipantsGroup() {
    auto *groupBox = new QGroupBox("Участники");
    auto *layout = new QVBoxLayout(groupBox);

    auto *onlineLabel = new QLabel("В сети");
    auto *offlineLabel = new QLabel("Не в сети");

    onlineLayout__ = new QVBoxLayout();
    offlineLayout__ = new QVBoxLayout();

    layout->addWidget(onlineLabel);
    layout->addLayout(onlineLayout__);
    layout->addWidget(offlineLabel);
    layout->addLayout(offlineLayout__);

    layout->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    return groupBox;
}

void Server::on_userStatusUpdate(int user_id_, UserState state_)
{
    QPushButton *userWidget_ = participants__[user_id_];
    for (int i = 0; i < onlineLayout__->count(); ++i) {
        if (onlineLayout__->itemAt(i)->widget() == userWidget_) {
            onlineLayout__->removeWidget(userWidget_);
            break;
        }
    }

    for (int i = 0; i < offlineLayout__->count(); ++i) {
        if (offlineLayout__->itemAt(i)->widget() == userWidget_) {
            offlineLayout__->removeWidget(userWidget_);
            break;
        }
    }

    switch(state_){
    case UserState::Online:
        onlineLayout__->addWidget(userWidget_);
        break;
    default:
        offlineLayout__->addWidget(userWidget_);
        break;
    }

}

Channel::Channel(int id_,
                 int compadres_id_,
                 const QString &name_,
                 bool is_voice_,
                 QDateTime created_at_,
                 QObject *parent)
    : QObject(parent),
    id__(id_),
    compadres_id__(compadres_id_),
    name__(name_),
    is_voice__(is_voice_),
    created_at__(created_at_)
{
    mainWidget__ = new QWidget();
    if (is_voice_)
        createVoiceChannel();
    else
    {
        createTextChannel();
    }
}

int Channel::getMessagesCount()
{
    if (messagesLayout__)
        // 1 - спейсер
        return messagesLayout__->count() - 1;
    else
        return -1;
}

void Channel::setLastMessage(int id,
                             const QString &type,
                             const QString &sender_name_,
                             const QJsonObject &content_,
                             const QString &created_at_)
{
    if (messages.contains(id))
        messages[id]->getWidget()->deleteLater();

    QDateTime dateTime = QDateTime::fromString(created_at_, Qt::ISODate);
    QString formattedDate;
    if (dateTime.daysTo(QDateTime::currentDateTime()) >= 1)
        formattedDate = dateTime.toString("hh:mm, dd-MM-yyyy");
    else
        formattedDate = dateTime.toString("hh:mm");

    Message *msg = new Message(-1, type, sender_name_, content_, formattedDate);

    QWidget *messageHandler = msg->getWidget();
    messages[id] = msg;
    messagesLayout__->addWidget(messageHandler);

}

void Channel::addUserToVoice(UserProfile *user_)
{
    if(!is_voice__)
        return;

    if (voiceUsers.contains(user_->getId()))
        return;

    QPushButton *user = new QPushButton(user_->getLogin());
    connect(user, &QPushButton::clicked, this, [=](){
        emit connectToVideoChat(id__, user_->getId());
    });
    user->setFlat(true);
    voiceUsersLayout__->addWidget(user);


    voiceUsers.insert(user_->getId(), user);
}

void Channel::createVoiceChannel()
{

    QVBoxLayout* layout = new QVBoxLayout();
    voiceUsersLayout__ = new QVBoxLayout();
    voiceUsersLayout__ ->addWidget(new QLabel("Участники:"));

    QLabel* titleLabel = new QLabel(name__);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    layout->addWidget(titleLabel);
    layout->addLayout(voiceUsersLayout__);
    layout->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    QPushButton* connectButton = new QPushButton("Подключиться");
    QPushButton* disconnectButton = new QPushButton("Отключиться");
    QPushButton *shareVideo = new QPushButton("Начать трансляцию");
    disconnectButton->setEnabled(false);
    QPushButton* micButton = new QPushButton();
    QPushButton* headersButton = new QPushButton();

    connectButton->setMaximumSize(QSize(300, 300));
    connect(connectButton, &QPushButton::clicked, this, [=](){
        emit connectToVoiceChannel(id__);
        connectButton->setEnabled(false);
        disconnectButton->setEnabled(true);

        shareVideo->setEnabled(true);
        shareVideo->setVisible(true);
    });

    disconnectButton->setMaximumSize(QSize(300, 300));
    connect(disconnectButton, &QPushButton::clicked, this, [=](){
        emit disconnectFromVoiceChannel(id__);
        connectButton->setEnabled(true);
        disconnectButton->setEnabled(false);
        shareVideo->setVisible(false);
        shareVideo->setEnabled(false);
    });

    micButton->setIcon(QIcon(":/icons/mic.png"));
    headersButton->setIcon(QIcon(":/icons/headers.png"));

    micButton->setMaximumSize(QSize(30, 30));
    headersButton->setMaximumSize(QSize(30, 30));

    connect(shareVideo, &QPushButton::clicked, this, [=](){
        emit startVideoChat(id__);
        shareVideo->setEnabled(false);
    });
    shareVideo->setVisible(false);

    QGridLayout *buttonsLayout = new QGridLayout();
    buttonsLayout->addWidget(connectButton, 0, 0);
    buttonsLayout->addWidget(disconnectButton, 1, 0);
    buttonsLayout->addWidget(micButton, 0, 1);
    buttonsLayout->addWidget(headersButton, 1, 1);
    buttonsLayout->addWidget(shareVideo, 2, 0, 1, 2);

    layout->addLayout(buttonsLayout);
    mainWidget__->setLayout(layout);
}

void Channel::createTextChannel()
{
    auto *textChannelTab = new QWidget();
    auto *textChannelLayout = new QVBoxLayout(textChannelTab);

    messagesScrollArea__ = new QScrollArea();
    messagesScrollArea__->setWidgetResizable(true);

    QWidget *messagesContainer = new QWidget();
    messagesLayout__ = new QVBoxLayout(messagesContainer);
    QSpacerItem *topSpacer = new QSpacerItem(
        0, 0,
        QSizePolicy::Minimum,
        QSizePolicy::Expanding
        );
    messagesLayout__->addItem(topSpacer);
    messagesContainer->setLayout(messagesLayout__);
    messagesScrollArea__->setWidget(messagesContainer);

    auto *inputLayout = new QHBoxLayout();
    auto *addButton = new QToolButton();
    addButton->setText("+");
    QLineEdit *messageInput = new QLineEdit();
    auto *emojiButton = new QPushButton(":D");
    auto *sendButton = new QPushButton("->");

    inputLayout->addWidget(addButton);
    inputLayout->addWidget(messageInput);
    inputLayout->addWidget(emojiButton);
    inputLayout->addWidget(sendButton);

    textChannelLayout->addWidget(messagesScrollArea__);
    textChannelLayout->addLayout(inputLayout);

    mainWidget__->setLayout(textChannelLayout);

    connect(sendButton, &QPushButton::clicked, this, [=](){
        emit sendMessageFromChannel(id__, 0, messageInput->text());
        messageInput->clear();
    });
}


SystemManager::SystemManager(NetworkManager *networkManager_, QObject *parent)
    : QObject(parent),
    networkManager__(networkManager_)
{
}


void SystemManager::handleConnectionFailed()
{
    QMessageBox::warning(nullptr, "Connection Error", "Failed to connect to the server.");
}

void FriendsTable::addFriend(UserProfile *user_)
{
    QString username = user_->getLogin();
    int stateColumn = user_->getState();
    int rowCount = this->rowCount();
    bool needToInsert = true;

    // Проверка, все ли строки в нужном столбце заняты непустыми значениями
    for (int row = 0; row < rowCount; ++row)
    {
        QTableWidgetItem* item = this->item(row, stateColumn);
        if (!item || item->text().isEmpty())
        {
            needToInsert = false;
            break;
        }
    }

    // Если есть хотя бы одна пустая ячейка — вставляем туда, иначе вставляем новую строку
    if (needToInsert)
    {
        this->setRowCount(rowCount + 1);

        // Сдвигаем строки вниз в выбранном столбце
        for (int row = rowCount; row > 0; --row)
        {
            QTableWidgetItem* aboveItem = this->item(row - 1, stateColumn);
            if (aboveItem)
            {
                this->setItem(row, stateColumn, new QTableWidgetItem(*aboveItem));
            }
            else
            {
                this->setItem(row, stateColumn, new QTableWidgetItem(""));
            }
        }

        QTableWidgetItem* newItem = new QTableWidgetItem(username);
        this->setItem(0, stateColumn, newItem);
    }
    else
    {
        // Вставка в первую пустую ячейку
        for (int row = 0; row < rowCount; ++row)
        {
            QTableWidgetItem* item = this->item(row, stateColumn);
            if (!item || item->text().isEmpty())
            {
                this->setItem(row, stateColumn, new QTableWidgetItem(username));
                break;
            }
        }
    }
}

void FriendsTable::on_friendStatusUpdate(int user_id_, UserState status_, const QString &userName_)
{
    int rows = this->rowCount();
    int cols = this->columnCount();
    int currentRow = -1;
    int currentColumn = -1;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            QTableWidgetItem* item = this->item(row, col);
            if (item && item->text() == userName_)
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

    QTableWidgetItem* movedItem = this->takeItem(currentRow, currentColumn);
    this->setItem(currentRow, currentColumn, nullptr);

    bool isRowEmpty = true;
    for (int col = 0; col < cols; ++col)
    {
        QTableWidgetItem* item = this->item(currentRow, col);
        if (item && !item->text().isEmpty())
        {
            isRowEmpty = false;
            break;
        }
    }

    if (isRowEmpty)
    {
        this->removeRow(currentRow);
        --rows;
        if (currentRow < rows)
        {
            if (currentRow < rows) {
                if (currentRow < rows)
                    rows = this->rowCount();
            }
        }
    }

    int targetColumn = static_cast<int>(status_);
    int insertRow = rows;

    for (int row = 0; row < rows; ++row)
    {
        QTableWidgetItem* item = this->item(row, targetColumn);
        if (!item || item->text().isEmpty())
        {
            insertRow = row;
            break;
        }
    }

    if (insertRow == rows)
    {
        this->setRowCount(rows + 1);
    }

    this->setItem(insertRow, targetColumn, movedItem);

}

Message::Message(int id_, const QString &type_, const QString &authorName_, const QJsonObject &content_, const QString &dateTime_, QWidget *parent) : QWidget(parent)
{
    mainWidget__ = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget__);
    QWidget *senderAndTimeWidget = new QWidget();
    QHBoxLayout *senderAndTimeLayout = new QHBoxLayout(senderAndTimeWidget);
    QLabel *sender = new QLabel(authorName_);
    QLabel *dateTime = new QLabel(dateTime_);
    senderAndTimeLayout->addWidget(sender);
    senderAndTimeLayout->addWidget(dateTime);
    senderAndTimeLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    sender->setTextInteractionFlags(Qt::TextBrowserInteraction);
    sender->setStyleSheet("font-weight: bold;");
    dateTime->setTextInteractionFlags(Qt::TextBrowserInteraction);

    mainLayout->addWidget(senderAndTimeWidget);
    if (type_ == TEXT)
    {
        QTextBrowser *content = new QTextBrowser();
        content->setText(content_["text"].toString());
        content->setFrameStyle(QFrame::NoFrame);
        mainLayout->addWidget(content);
    }
    else if (type_ == INVITE)
    {
        QWidget *inviteHandler = new QWidget();
        inviteHandler->setLayout(new QHBoxLayout());
        QPushButton *acceptInvite = new QPushButton("Принять");
        QLabel *serverName = new QLabel(content_["server_name"].toString());
        serverName->setStyleSheet("font-weight: bold;");
        inviteHandler->layout()->addWidget(new QLabel("Приглашение на сервер:"));
        inviteHandler->layout()->addWidget(serverName);
        inviteHandler->layout()->addWidget(acceptInvite);
        mainLayout->addWidget(inviteHandler);
    }

}

VideoChatWindow::VideoChatWindow()
{
    gst_init(nullptr, nullptr);

    videosHandler = new QSplitter(Qt::Horizontal);
    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget(videosHandler);

    setLayout(layout);
}

VideoChatWindow::~VideoChatWindow()
{
    // for (auto &pipeline : videoWidgets__)
    // {
    //     gst_element_set_state(pipeline.first, GST_STATE_NULL);
    //     gst_object_unref(pipeline.first);
    //     pipeline.first = nullptr;
    // }
}

void VideoChatWindow::startVideo(const QString &ip_, const QString &port_)
{
    int port = port_.toInt();
    int bitrate = 8000;
    QString speedPreset = "superfast";
    // int cropLeft = 1920;

    GstElement* pipeline = gst_element_factory_make("fakesrc", nullptr);
    QVBoxLayout *videoLayout = new QVBoxLayout();
    QWidget *videoWidget = new QWidget();
    QWidget *videoPlayer = new QWidget();
    videoPlayer->setMinimumSize(360, 140);
    videoWidget->setLayout(videoLayout);
    QLabel *nameLabel = new QLabel("Ваша трансляция");
    nameLabel->setMaximumHeight(20);
    videoLayout->addWidget(nameLabel);
    videoLayout->addWidget(videoPlayer);

    videoWidgets__.insert(0, {QSharedPointer<GstElement>(pipeline, &gst_object_unref), videoPlayer});

    QString pipelineStr;
    pipelineStr = QString(
                      "ximagesrc use-damage=0 ! "
                      "videoconvert ! "
                      "tee name=t ! "
                      "queue ! videoconvert ! glimagesink "
                      "t. ! queue ! x264enc tune=zerolatency bitrate=%1 speed-preset=%2 ! "
                      "rtph264pay config-interval=1 pt=96 ! "
                      "udpsink host=%3 port=%4"
                      ).arg(bitrate).arg(speedPreset).arg(ip_).arg(port);

    QByteArray pipelineBytes = pipelineStr.toLocal8Bit();
    const char *pipelineDesc = pipelineBytes.constData();

    GError *error = nullptr;
    pipeline = gst_parse_launch(pipelineDesc, &error);
    if (!pipeline) {
        qDebug() << "Failed to create pipeline: " << error->message;
        g_error_free(error);
        return;
    }

    // Привязываем виджет к sink для отображения видео
    GstElement *sink = gst_bin_get_by_interface(GST_BIN(pipeline), GST_TYPE_VIDEO_OVERLAY);
    if (sink) {
        WId winId = videoWidgets__[0].second->winId();
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), winId);

// #if defined(Q_OS_WIN)
//         gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(sink),
//                                                0, 0,
//                                                screens__[0]->width(),
//                                                screens__[0]->height());
// #endif

        gst_object_unref(sink);
    }
    videosHandler->addWidget(videoWidget);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void VideoChatWindow::addVideo(int id_, const QString &name_, const QString &ip_, const QString &port)
{
    GstElement* pipeline = gst_element_factory_make("fakesrc", nullptr);
    QVBoxLayout *videoLayout = new QVBoxLayout();
    QWidget *videoWidget = new QWidget();
    QWidget *videoPlayer = new QWidget();
    videoPlayer->setMinimumSize(360, 140);
    videoWidget->setLayout(videoLayout);
    QLabel *nameLabel = new QLabel("Трансляция " + name_);
    nameLabel->setMaximumHeight(20);
    videoLayout->addWidget(nameLabel);
    videoLayout->addWidget(videoPlayer);

    videoWidgets__.insert(0, {QSharedPointer<GstElement>(pipeline, &gst_object_unref), videoPlayer});
    videosHandler->addWidget(videoWidget);

    // Здесь адрес UDP потока — поменяйте при необходимости
    std::string pipeline_desc;
    // if (isWayland())
    // {
        // pipeline_desc =
        //     "udpsrc port=5002 caps=\"application/x-rtp, media=video, encoding-name=H264, payload=96\" ! "
        //     "rtph264depay ! avdec_h264 ! videoconvert ! gtksink name=sink";
    // }
    // else
    // {
        pipeline_desc =
            "udpsrc port=5002 caps=\"application/x-rtp, media=video, encoding-name=H264, payload=96\" ! "
            "rtph264depay ! avdec_h264 ! videoconvert ! glimagesink name=sink";
//    }

    pipeline = gst_parse_launch(pipeline_desc.c_str(), nullptr);
    if (!pipeline) {
        qDebug() << "Failed to create GStreamer pipeline";
        return;
    }

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (!sink) {
        qDebug() << "Failed to get sink element from pipeline";
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return;
    }

    WId win_id = videoPlayer->winId();
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), (guintptr)win_id);

    gst_object_unref(sink);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

}

void VideoChatWindow::removeVideo(int id_)
{
    auto it = videoWidgets__.find(id_);
    if (it != videoWidgets__.end()) {
        GstElement* element = it.value().first.data();

        if (element) {
            gst_element_set_state(element, GST_STATE_NULL);
        }

        it.value().first.clear();
        it.value().second->deleteLater();

        videoWidgets__.erase(it);
    }
    while (videosHandler->count() > 0) {
        QWidget* widget = videosHandler->widget(0);
        if (widget) {
            widget->setParent(nullptr);
            delete widget;
        }
    }
}
void VideoChatWindow::closeEvent(QCloseEvent *event)
{
    for (int id : videoWidgets__.keys())
        removeVideo(id);
    event->accept();
}
