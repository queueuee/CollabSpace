#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QAudioFormat>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    void connectToServer(const QString &host, quint16 port);
    void sendMessage(const QString &userName, const QString &message);
    void startVoiceChat();
    void leaveVoiceChat();
    void onOffHeadphones(bool);
    void onOffMic(bool);

    bool isMicrophoneEnabled() const;
    bool areHeadphonesEnabled() const;

    QTcpSocket* getTcpSocket() const;
    QUdpSocket* getUdpSocket() const;

signals:
    void connectionSuccess();
    void connectionFailed();
    void messageReceived(const QString &userName, const QString &content, const QString &timestamp);

private slots:
    void sendAudio();
    void readUdpAudio();

private:
    void setupAudioFormat();

    QTcpSocket      *tcpSocket__;
    QUdpSocket      *udpSocket__;
    QScopedPointer<QAudioInput>  audioInput__;
    QScopedPointer<QAudioOutput> audioOutput__;
    QIODevice       *audioInputDevice__;
    QIODevice       *audioOutputDevice__;
    QAudioFormat    format;

    bool microphoneEnabled__ = true;
    bool headphonesEnabled__ = true;
    bool voiceChatActive__ = false;
};

#endif // NETWORKMANAGER_H
