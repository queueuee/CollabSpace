#ifndef CLIENTMAIN_H
#define CLIENTMAIN_H

#include "authorization.h"
#include "systemmanager.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QMessageBox>
#include <QTimer>
#include <QScopedPointer>
#include <QDateTime>


QT_BEGIN_NAMESPACE
namespace Ui {
class ClientMain;
}
QT_END_NAMESPACE


class ClientMain : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientMain(QWidget *parent = nullptr);
    ~ClientMain();

private slots:
    void handleTextMessageReceived(int server_id,
                                   int channel_id,
                                   const QString &userName,
                                   QString &content,
                                   const QString &timestamp);

    void on_getUserServerList(const QJsonObject &response);
    void on_getOpenServerList(const QJsonObject &response);
    void on_joinToServerAnswer(const QJsonObject &response);
    void on_leaveServerAnswer(const QJsonObject &response);
    void on_deleteServerAnswer(const QJsonObject &response);
    void on_serverParticipantsList(const QJsonObject &response);
    void on_updateUser(const QJsonObject &response);
    void on_friendshipAccepted(const int id, const QString &username, int userState);
    void on_addFriendRequest(const int id, const QString &username);
    void on_addPersonalChat(const int channel_id, const QString &username, const int user_id, const int compadres_id);
    void on_sendWhisper(const int target_id, const QString& message_);
    void on_sendMessageFromChannel(const int channel_id_, const int mess_type_, const QString &message_);
    void on_getMessagesList(const int channel_id_);
    void on_sendFriendRequest(const int user_id_);
    void on_joinServer(int id);

    void startVoiceChat();            // Старт голосового чата

    void leaveVoiceChat();            // Остановка голосового чата

    void on_micOnOffButton_clicked();

    void on_headersOnOffButton_clicked();

    void handleConnectionFailed();

    void on_createServerButton_clicked();

    void on_toFriendRequestsBtn_clicked();

    void on_backToPersonalMsgsListBtn_clicked();

private:
    void createServer(const QString &name_,
                      const QString &description_,
                      const int num_of_voiceChannels_,
                      const int num_of_textChannels_,
                      bool is_open_);
    void getUserServerList();
    void getOpenServerList();
    void getServerParticipantsList(int id_);
    void getFriendRequests();
    void getFriendsList();
    void getPersonalChatsList();
    void clearOpenServers();
    void logOut();

    UserData                                        userData__;
    int                                             messages_to_download__;

    SystemManager                                   *systemManager__;
    NetworkManager                                  *networkManager__;

    QMap<int, Server*>                              userServers__;
    QMap<int, Channel*>                             personalMessages__;
    QMap<int, UserProfile*>                         userFriends__;
    QMap<int, UserProfile*>                         relatedUsers__;
    QMap<int, ShortServer*>                         openServers__;


    bool                                        micEnabled__ = true;
    bool                                        headphonesEnabled__ = true;


    Ui::ClientMain                              *ui__;
};

#endif // CLIENTMAIN_H
