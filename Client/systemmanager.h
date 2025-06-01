#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include "networkmanager.h"

#include <QObject>
#include <QDebug>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QGroupBox>
#include <QScrollArea>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QMap>
#include <QProxyStyle>
#include <QStyleOptionTab>
#include <QPainter>
#include <QFontMetrics>
#include <QTextOption>
#include <QApplication>
#include <QMenu>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QScrollBar>
#include <QTimer>
#include <QTextBrowser>
#include <QSplitter>
#include <QPair>
#include <QSharedPointer>


#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>


#include <gst/gst.h>

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


// Разделить класс на participant и friend, унаследовав от абстрактного User
class UserProfile : public QDialog
{
    Q_OBJECT
public:
    UserProfile(int id_, QString login_, UserState status_, FriendShipState friendshipState);
    ~UserProfile() {};

    QWidget* getMainWidget() { return mainWidget__; };
    QString getLogin() {return login__;};
    UserState getState() {return state__;};
    void setState(UserState status_) { state__ = status_; emit statusUpdate(id__, state__);};
    int getId() { return id__; };
private:
    int id__;
    QByteArray img__;
    QString login__;

    QPushButton *addFriendBtn__;
    QPushButton *sendMsgBtn__;


    QLineEdit *messageEdit__;
    UserState state__;
    FriendShipState friendshipState__;

    QWidget* mainWidget__;

signals:
    void statusUpdate(int user_id_, UserState);
    void sendFriendRequest(int id_);
    void sendWhisper(int id_, const QString &message_);

public slots:
    void showProfile();
};

class VideoChatWindow : public QDialog
{
    Q_OBJECT
public:
    VideoChatWindow();
    ~VideoChatWindow();
    void startVideo(const QString &ip_, const QString &port);
    void addVideo(int id_, const QString &name_, const QString &ip_, const QString &port);
    void removeVideo(int id_);
protected:
    void closeEvent(QCloseEvent *event) override; // <-- переопределяем

private:
    QMap<int, QPair<QSharedPointer<GstElement>, QWidget*>> videoWidgets__;
    QSplitter *videosHandler;
};

class FriendsTable : public QTableWidget
{
    Q_OBJECT
public:
    FriendsTable(QWidget *parent = nullptr) : QTableWidget(parent) {};
    FriendsTable(int rows, int columns, QWidget *parent = nullptr) : QTableWidget(rows, columns, parent) {};

    void addFriend(UserProfile *user_);

    ~FriendsTable() = default;
public slots:
    void on_friendStatusUpdate(int user_id_, UserState status_, const QString &userName_);
};


class HorizontalTabStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                           const QSize& size, const QWidget* widget) const override;

    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget = nullptr) const override;
};

class Server;

class CustomTabWidget : public QTabWidget {
    Q_OBJECT
public:
    explicit CustomTabWidget(QWidget* parent = nullptr);

    void insertServerTab(Server* server, int serverId);
    Server* getServer(int id) const;
    void clearTab();

private slots:
    void onTabContextMenuRequested(const QPoint& pos);

private:
    void removeServerTab(int index);

    QList<Server*> servers_;
    QMap<int, Server*> serverIds_;
};

class Message : public QWidget
{
    Q_OBJECT
public:
    Message(int id_,
            const QString &type_,
            const QString &authorName_,
            QString &content_,
            const QString &dateTime_,
            QWidget *parent = nullptr);
    QWidget* getWidget() { return mainWidget__; };
private:
    QWidget *mainWidget__;

};

// создать виртуальный класс и унаследовать разные классы под ЛС и чаты севрера
class Channel : public QObject
{
    Q_OBJECT
public:
    explicit Channel(int id_,
                     int compadres_id_,
                     const QString &name_,
                     bool is_voice_,
                     QDateTime created_at_,
                     QObject *parent = nullptr);
    QWidget *getWidget(){return mainWidget__;};
    int getID() {return id__;};
    int getMessagesCount();
    void setLastMessage(const QString &sender_name_,
                        QString &content_,
                        const QString &created_at);
    void addUserToVoice(UserProfile *user_);
    bool isVoice() { return is_voice__; };
    ~Channel() {};

private:
    int                                         id__;
    int                                         compadres_id__;
    QString                                     name__;
    QDateTime                                   created_at__;
    bool                                        is_voice__;
    QWidget                                     *mainWidget__;

    //QTextEdit                                   *chatWindow;
    QScrollArea                                 *messagesScrollArea__;
    QVBoxLayout                                 *messagesLayout__;
    QVBoxLayout                                 *voiceUsersLayout__;

    QMap<int, QPushButton*>                     voiceUsers;
    void createVoiceChannel();
    void createTextChannel();

signals:
    void sendMessageFromChannel(int channel_id, int message_type, const QString &message);
    void connectToVoiceChannel(int channel_id);
    void disconnectFromVoiceChannel(int channel_id);
    void startVideoChat(int channel_id);
    void connectToVideoChat(int channel_id, int user_id);
};

class ShortServer : public QPushButton
{
    Q_OBJECT
public:
    explicit ShortServer(int id_,
                         int defaultInviteId_,
                         const QString &name_,
                         const QString &description_,
                         QByteArray img_);
    QString getName() {return name__;};
    QWidget* getWidget() {return mainWidget__;};
    QPushButton* getJoinBtn() {return joinServerBtn__;};
private:
    QPushButton* joinServerBtn__;

protected:
    int                                         id__;
    int                                         defaultInviteId__;
    QSet<int>                                   inviteIds__;
    QString                                     name__;
    QString                                     description__;
    QByteArray                                  img__;

    QWidget                                     *mainWidget__;

signals:
    void serverShortInfo(const QString &name_,
                         const QString &description_);
    void acceptInvite(int id_);
};


class Server : public ShortServer
{
    Q_OBJECT
public:
    explicit Server(int id__,
                    const QString &name_,
                    const QString &description_,
                    bool is_open_,
                    QByteArray img_,
                    QString roleName_,
                    bool is_highest_perm_,
                    int user_perm_,
                    int admin_perm_,
                    const QMap<int, QJsonObject> &channels_);
    ~Server() {};

    QMap<int, Channel*>  getChannels() {return channels__;};
    QMap<int, Channel*>  voiceChannels();
    void deleteServer() { is_highest_perm__ ?  emit deleteServerSignal(id__) : void(); };
    void leaveServer() { emit leaveServerSignal(id__); };
    bool canDeleteServer() { return is_highest_perm__; };
    void participantAdd(UserProfile *user_);

private:
    QVBoxLayout                                 *onlineLayout__;
    QVBoxLayout                                 *offlineLayout__;

    bool                                        is_open__;
    QString                                     roleName__;
    bool                                        is_highest_perm__;
    int                                         user_perm__;
    int                                         admin_perm__;
    QMap<int, Channel*>                         channels__;
    QMap<int, QPushButton*>                     participants__;
    QString                                     serverAdr__ = "0.0.0.0";


    QGroupBox *createParticipantsGroup();

signals:
    void sendMessageFromChannel(int channel_id_, int type_, const QString &message_);
    void getMessagesList(int channel_id_);
    void deleteServerSignal(int id_);
    void leaveServerSignal(int id_);

private slots:
    void on_userStatusUpdate(int user_id, UserState);
};


struct UserData
{
    QString login;
    QString password;
    QUrl    authUrl;
    int     user_id;
};

class SystemManager : public QObject
{
    Q_OBJECT
public:
    explicit SystemManager(NetworkManager *networkManager_, QObject *parent = nullptr);
    ~SystemManager() {};

    // заглушка
    QString userLogin;
    int user_id__;

private:
    NetworkManager                      *networkManager__;

private slots:
    void handleConnectionFailed();
signals:
    void authFinish(QString error_);
};

#endif // SYSTEMMANAGER_H
