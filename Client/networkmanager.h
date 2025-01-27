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

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    QString connectToCollabSpaceServer(const QString &login_,
                                       const QString &passwordHash_,
                                       const int &status_);
    void sendMessage(const QString &userName, const QString &message);
    void startVoiceChat();
    void leaveVoiceChat();
    void onOffHeadphones(bool);
    void onOffMic(bool);

    bool isMicrophoneEnabled() const;
    bool areHeadphonesEnabled() const;

    QWebSocket* getWebSocket() const;
    QUdpSocket* getUdpSocket() const;

signals:
    void connectionSuccess();
    void connectionFailed();
    void textMessageReceived(const QString &userName,
                             const QString &content,
                             const QString &timestamp);
    void jsonAnswerReceived(const QString &message_);
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
    void parseJson(QJsonObject &message);

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
