#include "networkmanager.h"
#include <QMessageBox>
#include <QHostAddress>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
    tcpSocket__(new QTcpSocket(this)),
    udpSocket__(new QUdpSocket(this)),
    audioInputDevice__(nullptr),
    audioOutputDevice__(nullptr),
    microphoneEnabled__(true),
    headphonesEnabled__(true)
{
    connect(tcpSocket__, &QTcpSocket::readyRead, this, [this]()
    {
        QByteArray data = tcpSocket__->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isObject())
        {
            QJsonObject messageJson = jsonDoc.object();
            QString userName = messageJson.value("userName").toString();
            QString content = messageJson.value("content").toString();
            QString timestamp = messageJson.value("timestamp").toString();
            emit messageReceived(userName, content, timestamp);
        }
        else
        {
            qWarning() << "Invalid message format received.";
        }
    });
}

NetworkManager::~NetworkManager()
{
    if (audioInputDevice__) {
        audioInputDevice__->close();
    }
    if (audioOutputDevice__) {
        audioOutputDevice__->close();
    }
    udpSocket__->close();
    udpSocket__->deleteLater();
    tcpSocket__->close();
    tcpSocket__->deleteLater();
}

void NetworkManager::connectToServer(const QString &host, quint16 port)
{
    tcpSocket__->connectToHost(QHostAddress(host), port);
    if (tcpSocket__->waitForConnected(2000)) {
        emit connectionSuccess();
    } else {
        emit connectionFailed();
    }
}

void NetworkManager::sendMessage(const QString &userName, const QString &message)
{
    if (!message.isEmpty() && tcpSocket__->state() == QTcpSocket::ConnectedState) {
        QJsonObject messageJson{
            {"type", "text_message"},
            {"server_id", "0"},
            {"chat_id", "0"},
            {"userName", userName},
            {"content", message},
            {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
        };
        QByteArray data = QJsonDocument(messageJson).toJson();
        tcpSocket__->write(data);
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

    udpSocket__ = new QUdpSocket(this);
    udpSocket__->bind(QHostAddress::AnyIPv4, quint16(0), QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);

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

    udpSocket__->writeDatagram("DISCONNECT", QHostAddress("127.0.0.1"), 54321);

    onOffAudioOutput(false);
    onOffAudioInput(false);

    disconnect(udpSocket__, &QUdpSocket::readyRead, this, &NetworkManager::readUdpAudio);
    udpSocket__->close();

    voiceChatActive__ = false;
}

void NetworkManager::sendAudio()
{
    if (!microphoneEnabled__ || !audioInputDevice__)
        return;

    QByteArray audioData = audioInputDevice__->readAll();
    udpSocket__->writeDatagram(audioData, QHostAddress("127.0.0.1"), 54321);
}

void NetworkManager::readUdpAudio()
{
    if (!headphonesEnabled__ || !audioOutputDevice__)
    {
        if (udpSocket__)
        {
            while (udpSocket__->hasPendingDatagrams())
            {
                QByteArray buffer;
                buffer.resize(udpSocket__->pendingDatagramSize());
                udpSocket__->readDatagram(buffer.data(), buffer.size());
            }
        }
        return;
    }

    while (udpSocket__->hasPendingDatagrams())
    {
        QByteArray buffer;
        buffer.resize(udpSocket__->pendingDatagramSize());

        qint64 bytesRead = udpSocket__->readDatagram(buffer.data(), buffer.size());

        if (!buffer.isEmpty())
        {
            qint64 bytesWritten = audioOutputDevice__->write(buffer);
        }
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
        if (!audioOn_ && !audioInput__.isNull())
        {
            audioInput__.reset();
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
        qDebug() << "Stopping audio output.";
        audioOutput__->stop();
        audioOutput__.reset();
    }

    if (!audioOn_)
    {
        if (udpSocket__)
        {
            while (udpSocket__->hasPendingDatagrams())
            {
                QByteArray buffer;
                buffer.resize(udpSocket__->pendingDatagramSize());
                udpSocket__->readDatagram(buffer.data(), buffer.size());
            }
            qDebug() << "UDP socket buffer cleared.";
        }
        audioOutputDevice__ = nullptr;
        qDebug() << "Headphones disabled, audio output device set to null.";
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

QTcpSocket* NetworkManager::getTcpSocket() const
{
    return tcpSocket__;
}

QUdpSocket* NetworkManager::getUdpSocket() const
{
    return udpSocket__;
}
