#ifndef CLIENTMAIN_H
#define CLIENTMAIN_H

#include "authorization.h"
#include "systemmanager.h"
#include "serversettings.h"

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
    void zoomIn();
    void zoomOut();
    void handleTextMessageReceived(int server_id,
                                   int channel_id,
                                   int msg_id,
                                   const QString &type_,
                                   const QString &userName,
                                   const QJsonObject &content,
                                   const QString &timestamp);

    void on_getUserServerList(const QJsonObject &response);
    void on_getOpenServerList(const QJsonObject &response);
    void on_joinToServerAnswer(const QJsonObject &response);
    void on_leaveServerAnswer(const QJsonObject &response);
    void on_deleteServerAnswer(const QJsonObject &response);
    void on_serverParticipantsList(const QJsonObject &response);
    void on_updateUser(const QJsonObject &response);
    void on_friendshipAccepted(const int id, const QString &username, int userState, const QJsonArray& commonServers = QJsonArray());
    void on_addFriendRequest(const int id, const QString &username);
    void on_addPersonalChat(const int channel_id, const QString &username, const int user_id, const int compadres_id);
    void on_sendWhisper(const int target_id, const QString& message_);
    void on_sendMessageFromChannel(const int channel_id_, const int mess_type_, const QString &message_);
    void on_getMessagesList(const int channel_id_);
    void on_sendFriendRequest(const int user_id_);
    void on_joinServer(int id);
    void connectToVoiceChannel(int id);
    void disconnectFromVoiceChannel(int id);
    void addUserToVoiceChannel(const int user_id,
                               const QString &username,
                               const int server_id,
                               const int compadres_id,
                               const int channel_id);
    void on_startVideoChatBtn(int channel_id);
    void on_connectToVideoChatBtn(int channel_id, int user_id);
    void openServerSettings(int server_id_, const QString& server_name_);
    void serverInviteData(const QJsonObject invData_);
    void on_sendInviteClicked(int target_id_, QString &serverName, const QJsonObject invData_);

    void startVoiceChat();            // Старт голосового чата

    void leaveVoiceChat();            // Остановка голосового чата

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
    QMap<int, UserProfile*>                         relatedUsers__;
    QMap<int, ShortServer*>                         openServers__;
    VideoChatWindow*                                videoChatWindow__;
    serverSettings*                                 serverSettings__;


    bool                                            micEnabled__ = true;
    bool                                            headphonesEnabled__ = true;


    Ui::ClientMain                                  *ui__;
};

#endif // CLIENTMAIN_H
