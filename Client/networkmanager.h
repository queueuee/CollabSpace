#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QWebSocket>
#include <QUdpSocket>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QAudioFormat>
#include <QEventLoop>
#include <QJsonArray>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    void connectToServer(const QString &login_,
                        const QString &passwordHash_,
                        const QUrl &Url_,
                        const int &status_);
    void sendMessageJsonToServer(QJsonObject &messageJson);
    void startVoiceChat();
    void leaveVoiceChat();
    void onOffHeadphones(bool);
    void onOffMic(bool);

    bool isMicrophoneEnabled() const;
    bool areHeadphonesEnabled() const;

    QWebSocket* getWebSocket() const;
    QUdpSocket* getUdpSocket() const;

    void parseJson(QJsonObject &message);

signals:
    void connectionSuccess();
    void connectionFailed();
    void getAuthToken(const QJsonObject &loginData);
    void textMessageReceived(int server_id,
                             int channel_id,
                             int msg_id,
                             const QString &type,
                             const QString &userName,
                             const QJsonObject &content,
                             const QString &timestamp);
    void createServer(const QJsonObject &frame);
    void userServerListRecived(const QJsonObject &userServerList);
    void openServerListRecived(const QJsonObject &openServerList);
    void joinToServerAnswer(const QJsonObject &answer);
    void deleteServerAnswer(const QJsonObject &answer);
    void leaveServerAnswer(const QJsonObject &answer);
    void serverParticipantsList(const QJsonObject &answer);
    void updateUser(const QJsonObject &answer);
    void acceptedFriendship(const int id, const QString &username, int userState, const QJsonArray &commonServers = QJsonArray());
    void addFriendRequest(const int id, const QString &username);
    void personalChat(const int key, const QString &username, const int user_id, const int compadres_id);
    void addUserToVoiceChannel(const int user_id, const QString &username, const int server_id, const int compadres_id, const int channel_id);
    void serverInviteData(const QJsonObject inviteData_);

private slots:
    void sendAudio();
    void readUdpAudio();
    void onConnected();
    void onDisconnected();
    void onJsonAnswerReceived(const QString &message);

private:
    void setupAudioFormat();
    void onOffAudioOutput(bool);
    void onOffAudioInput(bool);


    QWebSocket                                      *webSocket__;
    QUdpSocket                                      *udpSocket__;
    QScopedPointer<QAudioInput>                     audioInput__;
    QScopedPointer<QAudioOutput>                    audioOutput__;
    QIODevice                                       *audioInputDevice__;
    QIODevice                                       *audioOutputDevice__;
    QAudioFormat                                    format;


    bool                                            microphoneEnabled__ = true;
    bool                                            headphonesEnabled__ = true;
    bool                                            voiceChatActive__ = false;
};

#endif // NETWORKMANAGER_H
