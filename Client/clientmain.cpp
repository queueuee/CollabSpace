#include "clientmain.h"


const QString programmName = "Collab Space";


ClientMain::ClientMain(QWidget *parent)
    : QMainWindow(parent)
    , ui__(new Ui::ClientMain)
    , systemManager__(NULL)
    , tcpSocket__(new QTcpSocket(this))
    , udpSocket__(new QUdpSocket(this))
    , audioInput__(nullptr)
    , audioOutput__(nullptr)
    , audioInputDevice__(nullptr)
    , audioOutputDevice__(nullptr)
{
    ui__->setupUi(this);

    QTimer::singleShot(50, this, [this] ()
    {
        Authorization auth(systemManager__);
        auth.setModal(true);
        if(auth.exec() == QDialog::Accepted)
        {
            setWindowTitle(programmName + " - " + systemManager__->userLogin);

            // Запрос друзей/участников/сообщений из бд
            connectToServer();
        }
        else
            QApplication::quit();
    });

    ui__->micOnOffButton->setFlat(true);
    ui__->headersOnOffButton->setFlat(true);

    connect(ui__->sendButton, &QPushButton::clicked, this, &ClientMain::sendMessage);
    connect(ui__->voiceConnectButton, &QPushButton::clicked, this, &ClientMain::startVoiceChat);
    connect(ui__->voiceDisconnectButton, &QPushButton::clicked, this, &ClientMain::leaveVoiceChat);
    connect(tcpSocket__, &QTcpSocket::readyRead, this, &ClientMain::receiveMessage);
}

ClientMain::~ClientMain()
{
    if (voiceChatActive__)
    {
        leaveVoiceChat();
    }
    if (tcpSocket__->isOpen())
    {
        tcpSocket__->close();
    }
    if (udpSocket__->isOpen())
    {
        udpSocket__->close();
    }

    delete ui__;
}


void ClientMain::connectToServer()
{
    tcpSocket__->connectToHost(QHostAddress("127.0.0.1"), 12345);
    if (tcpSocket__->waitForConnected(2000))
    {
        ui__->chatWindow->append("Connected to the server.");
    }
    else
    {
        QMessageBox::warning(this, "Connection Error", "Failed to connect to the server.");
    }
}

void ClientMain::sendMessage() {
    QString message = ui__->messageInput->text();

    if (!message.isEmpty() && tcpSocket__->state() == QTcpSocket::ConnectedState)
    {
        QJsonObject messageJson{
            {"type", "text_message"},       // тип сообщения
            {"server_id", "0"},             // id сервера
            {"chat_id", "0"},               // id чата на сервере
            {"userName", systemManager__->userLogin},
            {"content", message},            // содержимое сообщения
            {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
        };

        QByteArray data = QJsonDocument(messageJson).toJson();
        tcpSocket__->write(data);
        ui__->chatWindow->append(systemManager__->userLogin + " " + QDateTime::currentDateTime().toString("HH:mm") + " (You)\n" + message + "\n");
        ui__->messageInput->clear();
    }
}

void ClientMain::receiveMessage()
{
    QByteArray data = tcpSocket__->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

    if (!jsonDoc.isObject())
    {
        qWarning() << "Invalid message format from server";
    }

    QJsonObject messageJson = jsonDoc.object();
    QString userName = messageJson.value("userName").toString();
    QString content = messageJson.value("content").toString();
    QString timestamp = messageJson.value("timestamp").toString();

    ui__->chatWindow->append(userName + " " + timestamp + "\n" + content + "\n");
}

void ClientMain::startVoiceChat()
{
    if (voiceChatActive__)
        return;

    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    audioInput__ = new QAudioInput(format, this);
    audioOutput__ = new QAudioOutput(format, this);

    if (!udpSocket__->bind(QHostAddress::AnyIPv4, quint16(0), QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint))
    {
        ui__->chatWindow->append("Failed to bind UDP socket.");
        delete audioInput__;
        audioInput__ = nullptr;
        return;
    }
    audioInputDevice__ = audioInput__->start();
    audioOutputDevice__ = audioOutput__->start();

    connect(audioInputDevice__, &QIODevice::readyRead, this, &ClientMain::sendAudio);
    connect(udpSocket__, &QUdpSocket::readyRead, this, &ClientMain::readUdpAudio);

    ui__->chatWindow->append("Voice chat started.");
    voiceChatActive__ = true;
}

void ClientMain::leaveVoiceChat()
{
    if (!voiceChatActive__)
        return;

    udpSocket__->writeDatagram("DISCONNECT", QHostAddress("127.0.0.1"), 54321);

    disconnect(audioInputDevice__, &QIODevice::readyRead, this, &ClientMain::sendAudio);
    disconnect(udpSocket__, &QUdpSocket::readyRead, this, &ClientMain::readUdpAudio);

    audioInput__->stop();
    audioOutput__->stop();

    // muteMic(true);
    // muteHeadphones(true);
    delete audioInput__;
    delete audioOutput__;

    udpSocket__->close();

    ui__->chatWindow->append("Voice chat stopped.");
    voiceChatActive__ = false;
}

void ClientMain::sendAudio()
{
    if (!micEnabled__ || !audioInputDevice__ || !udpSocket__)
    {
        return;
    }

    QByteArray audioData = audioInputDevice__->readAll();
    if (!audioData.isEmpty()) {
        udpSocket__->writeDatagram(audioData, QHostAddress("127.0.0.1"), 54321);
    }
}

void ClientMain::readUdpAudio()
{
    qDebug() << "readUdpAudio called.";
    qDebug() << "headphonesEnabled:" << headphonesEnabled__;
    qDebug() << "audioOutputDevice__:" << (audioOutputDevice__ ? "Valid" : "Null");

    if (!headphonesEnabled__ || !audioOutputDevice__)
    {
        if (udpSocket__)
        {
            while (udpSocket__->hasPendingDatagrams())
            {
                QByteArray buffer;
                buffer.resize(udpSocket__->pendingDatagramSize());
                udpSocket__->readDatagram(buffer.data(), buffer.size()); // Сбрасываем данные
            }
            qDebug() << "Audio output disabled, clearing socket buffer.";
        }
        return;
    }

    while (udpSocket__->hasPendingDatagrams())
    {
        QByteArray buffer;
        buffer.resize(udpSocket__->pendingDatagramSize());

        qint64 bytesRead = udpSocket__->readDatagram(buffer.data(), buffer.size());
        qDebug() << "Bytes read from UDP socket:" << bytesRead;

        if (!buffer.isEmpty())
        {
            qint64 bytesWritten = audioOutputDevice__->write(buffer);
            qDebug() << "Bytes written to audio output:" << bytesWritten;

            if (bytesWritten == -1)
            {
                qDebug() << "Error: Failed to write to audio output.";
                QMessageBox::warning(this, "Audio Error", "Failed to play audio.");
            }
        }
    }
}

void ClientMain::on_micOnOffButton_clicked()
{
    micEnabled__ = !micEnabled__;

    ui__->micOnOffButton->setFlat(micEnabled__);
    ui__->micOnOffButton->setIcon(micEnabled__ ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));

    muteMic(micEnabled__);
}

void ClientMain::muteMic(bool micEnabled_)
{
    if (micEnabled_)
    {
        if (audioInput__)
        {
            try
            {
                audioInputDevice__ = audioInput__->start();
                if (audioInputDevice__)
                {
                    connect(audioInputDevice__, &QIODevice::readyRead, this, &ClientMain::sendAudio);
                }
            }
            catch (...)
            {
                QMessageBox::warning(this, "Error", "Failed to enable microphone.");
            }
        }
    }
    else
    {
        if (audioInputDevice__)
        {
            disconnect(audioInputDevice__, &QIODevice::readyRead, this, &ClientMain::sendAudio);
            audioInput__->stop();
            audioInputDevice__ = nullptr;
        }
    }

}


void ClientMain::on_headersOnOffButton_clicked()
{

    headphonesEnabled__ = !headphonesEnabled__;

    ui__->headersOnOffButton->setFlat(headphonesEnabled__);
    ui__->headersOnOffButton->setIcon(headphonesEnabled__ ? QIcon(":/icons/headers.png") : QIcon(":/icons/headersOff.png"));

    muteHeadphones(headphonesEnabled__);
    on_micOnOffButton_clicked();
}

void ClientMain::muteHeadphones(bool headphonesEnabled_)
{
    if (audioOutput__)
    {
        qDebug() << "Stopping and deleting audio output.";
        audioOutput__->stop();
        delete audioOutput__;
        audioOutput__ = nullptr;
    }

    if (!headphonesEnabled_)
    {
        // Очищаем буфер UDP сокета
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
        audioOutput__ = new QAudioOutput(format, this);
        audioOutputDevice__ = audioOutput__->start();
    }

}
