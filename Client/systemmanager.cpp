#include "systemmanager.h"
#include "../possible_requests.h"

// Класс Participant
Participant::Participant(int id_, QString login_, UserState status_) :
    id__(id_),
    login__(login_),
    state__(status_)
{
    mainWidget__ = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget__);
    mainWidget__->setLayout(mainLayout);

    menuWidget__ = new QWidget();
    QHBoxLayout *menuLayout = new QHBoxLayout(menuWidget__);
    menuWidget__->setLayout(menuLayout);
    menuWidget__->setVisible(false);

    messageEdit__ = new QLineEdit();

    openMenuBtn__ = new QPushButton(login__);

    addFriendBtn__ = new QPushButton("+");
    addFriendBtn__->setMaximumWidth(20);

    sendMsgBtn__ = new QPushButton("->");
    sendMsgBtn__->setMaximumWidth(20);

    openMenuBtn__->setFlat(true);
    connect(openMenuBtn__, &QPushButton::clicked, this, [=](){
        menuWidget__->setVisible(!menuWidget__->isVisible());
    });


    mainLayout->addWidget(openMenuBtn__);
    menuLayout->addWidget(addFriendBtn__);
    menuLayout->addWidget(messageEdit__);
    menuLayout->addWidget(sendMsgBtn__);

    mainLayout->addWidget(menuWidget__);
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
    QAction* deleteAction = nullptr;
    if (server->canDeleteServer())
        deleteAction = menu.addAction("Удалить сервер");
    QAction* renameAction = menu.addAction("Переименовать...");

    QAction* selected = menu.exec(tabBar()->mapToGlobal(pos));
    if (selected == leaveAction)
    {
        server->leaveServer();
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

    mainLayout->addWidget(voiceTabWidget);
    mainLayout->addWidget(textTabWidget);

    // Участники
    auto *participantsGroup = createParticipantsGroup();
    mainLayout->addWidget(participantsGroup);


    mainWidget__->setLayout(mainLayout);
}

void Server::participantAdd(Participant *user_)
{
    if (participants__.contains(user_->getId()))
    {
        // Если пользователь есть, то нужно обновлять по нему данные
        return;
    }

    participants__[user_->getId()] = user_;
    switch(user_->getState()){
    case UserState::Online:
        onlineLayout__->addWidget(user_->getMainWidget());
        break;
    default:
        offlineLayout__->addWidget(user_->getMainWidget());
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

void Channel::setLastMessage(const QString &sender_name_,
                             const QString &content_,
                             const QString &created_at_)
{
    QDateTime dateTime = QDateTime::fromString(created_at_, Qt::ISODate);
    QString formattedDate;
    if (dateTime.daysTo(QDateTime::currentDateTime()) >= 1)
        formattedDate = dateTime.toString("hh:mm, dd-MM-yyyy");
    else
        formattedDate = dateTime.toString("hh:mm");

    chatWindow->append(sender_name_ + " " + formattedDate + '\n' + content_);
}

void Channel::createVoiceChannel()
{
    QGridLayout* layout = new QGridLayout();

    QLabel* titleLabel = new QLabel(name__);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    layout->addWidget(titleLabel, 0, 0, 1, 2);

    QPushButton* connectButton = new QPushButton("Подключиться");
    QPushButton* disconnectButton = new QPushButton("Отключиться");
    QPushButton* micButton = new QPushButton();
    micButton->setIcon(QIcon(":/icons/mic.png"));

    layout->addWidget(connectButton, 1, 0);
    layout->addWidget(disconnectButton, 2, 0);
    layout->addWidget(micButton, 1, 1);


    mainWidget__->setLayout(layout);
}

void Channel::createTextChannel()
{
    auto *textChannelTab = new QWidget();
    auto *textChannelLayout = new QVBoxLayout(textChannelTab);

    chatWindow = new QTextEdit();
    chatWindow->setReadOnly(true);

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

    textChannelLayout->addWidget(chatWindow);
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
