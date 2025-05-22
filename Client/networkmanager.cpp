
#include "networkmanager.h"
#include "../possible_requests.h"


#include <QMessageBox>
#include <QHostAddress>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>

#define COLLAB_SPACE_UDP_PORT 54321


NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
    webSocket__(new QWebSocket),
    udpSocket__(new QUdpSocket(this)),
    audioInputDevice__(nullptr),
    audioOutputDevice__(nullptr),
    microphoneEnabled__(true),
    headphonesEnabled__(true)
{
    connect(webSocket__, &QWebSocket::connected, this, &NetworkManager::onConnected);
    connect(webSocket__, &QWebSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(webSocket__, &QWebSocket::textMessageReceived, this, &NetworkManager::onJsonAnswerReceived);
}

NetworkManager::~NetworkManager()
{
    if (audioInputDevice__) {
        audioInputDevice__->close();
    }
    if (audioOutputDevice__) {
        audioOutputDevice__->close();
    }
    webSocket__->close();
    udpSocket__->close();
    webSocket__->deleteLater();
    udpSocket__->deleteLater();
}
QJsonObject messageJson;
void NetworkManager::connectToServer(const QString &login_,
                                            const QString &passwordHash_,
                                            const QUrl &Url_,
                                            const int &status_)
{
    webSocket__->open(Url_);

    messageJson = {
        {REQUEST, LOGIN_USER},
        {"login", login_},
        {"passwordHash", passwordHash_},
        {"status", QString::number(status_)}
    };

}

void NetworkManager::sendMessageJsonToServer(QJsonObject &messageJson)
{
    QByteArray data = QJsonDocument(messageJson).toJson();
    webSocket__->sendTextMessage(QString::fromUtf8(data));
}
void NetworkManager::onConnected()
{
    sendMessageJsonToServer(messageJson);

    emit connectionSuccess();
}

void NetworkManager::onDisconnected()
{
    emit connectionFailed();
}

void NetworkManager::onJsonAnswerReceived(const QString &message_)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(message_.toUtf8());
    if (jsonDoc.isObject())
    {
        QJsonObject messageJson = jsonDoc.object();
        parseJson(messageJson);
    }
    else
    {
        qWarning() << "Invalid message format received.";
    }
}

void NetworkManager::parseJson(QJsonObject &messageJson_)
{
    QString request_type = messageJson_.value(REQUEST).toString();

    if (request_type == LOGIN_USER)
    {
        emit getAuthToken(messageJson_);
    }
    else if (request_type == MESSAGE)
    {
        int channel_id = messageJson_.value("channel_id").toInt();
        int server_id = messageJson_.value("server_id").toInt();
        QJsonObject messagesObj = messageJson_.value("message").toObject();

        for (const QString &msg_id : messagesObj.keys()) {
            QJsonObject messageData = messagesObj.value(msg_id).toObject();
            QString userName = messageData.value("user_name").toString();
            QString content = messageData.value("content").toString();
            QString timestamp = messageData.value("timestamp").toString();

            emit textMessageReceived(server_id, channel_id, userName, content, timestamp);
        }
    }
    else if(request_type == GET_USER_SERVERS_LIST)
    {
        emit userServerListRecived(messageJson_["info"].toObject());
    }
    else if(request_type == GET_OPEN_SERVERS_LIST)
    {
        emit openServerListRecived(messageJson_["info"].toObject());
    }
    else if (request_type == CREATE_SERVER)
    {
        emit createServer(messageJson_["info"].toObject());
    }
    else if(request_type == JOIN_SERVER)
    {
        emit joinToServerAnswer(messageJson_["info"].toObject());
    }
    else if(request_type == DELETE_SERVER)
    {
        emit deleteServerAnswer(messageJson_["info"].toObject());
    }
    else if(request_type == USER_LEAVE_SERVER_INFO)
    {
        emit leaveServerAnswer(messageJson_["info"].toObject());
    }
    else if(request_type == GET_SERVER_PARTICIPANTS_LIST)
    {
        emit serverParticipantsList(messageJson_["info"].toObject());
    }
    else if(request_type == UPDATE_USER)
    {
        emit updateUser(messageJson_["info"].toObject());
    }

}


void NetworkManager::setupAudioFormat()
{
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
}

void NetworkManager::startVoiceChat()
{
    if (voiceChatActive__)
        return;

    setupAudioFormat();

    audioInput__.reset(new QAudioInput(format, this));
    audioOutput__.reset(new QAudioOutput(format, this));

    udpSocket__->bind(QHostAddress::AnyIPv4, COLLAB_SPACE_UDP_PORT, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);

    audioInputDevice__ = audioInput__->start();
    audioOutputDevice__ = audioOutput__->start();

    connect(audioInputDevice__, &QIODevice::readyRead, this, &NetworkManager::sendAudio);
    connect(udpSocket__, &QUdpSocket::readyRead, this, &NetworkManager::readUdpAudio);

    voiceChatActive__ = true;
}

void NetworkManager::leaveVoiceChat()
{
    if (!voiceChatActive__)
        return;

    udpSocket__->writeDatagram("DISCONNECT", QHostAddress::LocalHost, COLLAB_SPACE_UDP_PORT);

    onOffAudioOutput(false);
    onOffAudioInput(false);

    voiceChatActive__ = false;
}

void NetworkManager::sendAudio()
{
    if (!microphoneEnabled__ || !audioInputDevice__)
        return;

    QByteArray audioData = audioInputDevice__->readAll();
    udpSocket__->writeDatagram(audioData, QHostAddress::LocalHost, COLLAB_SPACE_UDP_PORT);
}

void NetworkManager::readUdpAudio()
{
    if (!headphonesEnabled__ || !audioOutputDevice__)
        return;

    while (udpSocket__->hasPendingDatagrams()) {
        QByteArray audioData;
        audioData.resize(udpSocket__->pendingDatagramSize());
        udpSocket__->readDatagram(audioData.data(), audioData.size());
        audioOutputDevice__->write(audioData);
    }
}

void NetworkManager::onOffHeadphones(bool headphonesEnabled_)
{
    headphonesEnabled__ = headphonesEnabled_;
    if (voiceChatActive__)
        onOffAudioInput(headphonesEnabled_);
}

void NetworkManager::onOffMic(bool micEnabled_)
{
    microphoneEnabled__ = micEnabled_;

    if (voiceChatActive__)
        onOffAudioOutput(microphoneEnabled__);
}

void NetworkManager::onOffAudioOutput(bool audioOn_)
{
    if (!audioOn_)
    {
        if (audioInputDevice__)
        {
            disconnect(audioInputDevice__, &QIODevice::readyRead, this, &NetworkManager::sendAudio);
            audioInput__->stop();
            audioInputDevice__ = nullptr;
        }
    }
    else
    {
        audioInput__.reset(new QAudioInput(format, this));
        audioInputDevice__ = audioInput__->start();
        connect(audioInputDevice__, &QIODevice::readyRead, this, &NetworkManager::sendAudio);
    }
}

void NetworkManager::onOffAudioInput(bool audioOn_)
{
    if (audioOutput__)
    {
        audioOutput__->stop();
        audioOutput__.reset();
    }

    if (!audioOn_)
    {
        audioOutputDevice__ = nullptr;
    }
    else
    {
        audioOutput__.reset(new QAudioOutput(format, this));
        audioOutputDevice__ = audioOutput__->start();
    }
}

bool NetworkManager::isMicrophoneEnabled() const
{
    return microphoneEnabled__;
}

bool NetworkManager::areHeadphonesEnabled() const
{
    return headphonesEnabled__;
}

QWebSocket* NetworkManager::getWebSocket() const
{
    return webSocket__;
}

QUdpSocket* NetworkManager::getUdpSocket() const
{
    return udpSocket__;
}
