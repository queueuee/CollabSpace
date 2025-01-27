
#include "networkmanager.h"
#include "../possible_requests.h"


#include <QMessageBox>
#include <QHostAddress>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>

#define COLLAB_SPACE_URL "ws://127.0.0.1:12345"
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

QString NetworkManager::connectToCollabSpaceServer(const QString &login_,
                                                   const QString &passwordHash_,
                                                   const int &status_)
{
    QEventLoop loop;
    QString responseError;

    connect(this, &NetworkManager::jsonAnswerReceived, [&loop, &responseError](const QString &message_) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(message_.toUtf8());
        responseError = jsonDoc.object().value(TYPE).toString();
        loop.quit();
    });

    webSocket__->open(QUrl(COLLAB_SPACE_URL));

    QJsonObject messageJson{
        {REQUEST, LOGIN_USER},
        {"login", login_},
        {"passwordHash", passwordHash_},
        {"status", QString::number(status_)}
    };
    QByteArray data = QJsonDocument(messageJson).toJson();

    QTimer::singleShot(10, this, [this, data]()
    {
        webSocket__->sendTextMessage(QString::fromUtf8(data));
    });

    loop.exec();

    return responseError;
}

void printJson(const QJsonDocument &doc)
{
    // Проверяем, является ли JSON объектом или массивом
    if (doc.isObject()) {
        // Выводим JSON объект в строковом формате
        qDebug() << "JSON Object:" << doc.object();
    }
    else {
        qDebug() << "Invalid JSON format.";
    }
}

void NetworkManager::sendMessage(const QString &userName, const QString &message)
{
    const int server_id = 0, channel_id = 0;

    if (!message.isEmpty()) {
        QJsonObject messageJson{
            {REQUEST, MESSAGE},
            {TYPE, TEXT},
            {"server_id", server_id},
            {"chat_id", channel_id},
            {"userName", userName},
            {"content", message},
            {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
        };
        QByteArray data = QJsonDocument(messageJson).toJson();
        webSocket__->sendTextMessage(QString::fromUtf8(data));
        printJson(QJsonDocument(messageJson));
    }
}
void NetworkManager::onConnected()
{
    emit connectionSuccess();
}

void NetworkManager::onDisconnected()
{
    emit connectionFailed();
}

void NetworkManager::onJsonAnswerReceived(const QString &message_)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(message_.toUtf8());
    printJson(QJsonDocument(jsonDoc));
    if (jsonDoc.isObject())
    {
        QJsonObject messageJson = jsonDoc.object();
        parseJson(messageJson);


        emit jsonAnswerReceived(message_);
    }
    else
    {
        qWarning() << "Invalid message format received.";
    }
}

// Заменить if на switch
void NetworkManager::parseJson(QJsonObject &messageJson_)
{
    auto request_type = messageJson_.value(REQUEST).toString();
    if (request_type == MESSAGE)
    {
        auto message_type = messageJson_.value(TYPE).toString();
        if (message_type == TEXT)
        {
            QString userName = messageJson_.value("userName").toString();
            QString content = messageJson_.value("content").toString();
            QString timestamp = messageJson_.value("timestamp").toString();

            emit textMessageReceived(userName, content, timestamp);

            return;
        }
        qWarning() << "Invalid message type:"<< message_type;
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
