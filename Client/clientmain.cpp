#include "clientmain.h"
#include "./ui_clientmain.h"

ClientMain::ClientMain(QWidget *parent)
    : QMainWindow(parent)
    , ui__(new Ui::ClientMain),
    tcpSocket__(new QTcpSocket(this)),
    udpSocket__(new QUdpSocket(this))
    //, audioInput__(nullptr),
    // audioOutput__(nullptr),
    // audioInputDevice__(nullptr),
    // audioOutputDevice__(nullptr)
{
    ui__->setupUi(this);

    ui__->micOnOffButton->setFlat(true);
    ui__->headersOnOffButton->setFlat(true);

    connect(ui__->connectButton, &QPushButton::clicked, this, &ClientMain::connectToServer);
    connect(ui__->sendButton, &QPushButton::clicked, this, &ClientMain::sendMessage);
    connect(ui__->voiceConnectButton, &QPushButton::clicked, this, &ClientMain::startVoiceChat);
    connect(ui__->voiceDisconnectButton, &QPushButton::clicked, this, &ClientMain::stopVoiceChat);
    connect(tcpSocket__, &QTcpSocket::readyRead, this, &ClientMain::receiveMessage);
}

ClientMain::~ClientMain()
{
    if (voiceChatActive)
    {
        stopVoiceChat();
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
    } else
    {
        QMessageBox::warning(this, "Connection Error", "Failed to connect to the server.");
    }
}

void ClientMain::sendMessage() {
    QString message = ui__->messageInput->text();
    if (!message.isEmpty() && tcpSocket__->state() == QTcpSocket::ConnectedState)
    {
        tcpSocket__->write(message.toUtf8());
        ui__->chatWindow->append("You: " + message);
        ui__->messageInput->clear();
    }
}


void ClientMain::receiveMessage()
{
    QByteArray data = tcpSocket__->readAll();
    ui__->chatWindow->append("Server: " + QString::fromUtf8(data));
}

void ClientMain::startVoiceChat()
{
    if (voiceChatActive)
        return;

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    // audioInput__ = new QAudioInput(format, this);
    // audioOutput__ = new QAudioOutput(format, this);

    if (!udpSocket__->bind(QHostAddress::AnyIPv4, quint16(0), QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint))
    {
        ui__->chatWindow->append("Failed to bind UDP socket.");
        // delete audioInput__;
        // audioInput__ = nullptr;
        return;
    }
    // audioInputDevice__ = audioInput__->start();
    // audioOutputDevice__ = audioOutput__->start();

    // connect(audioInputDevice__, &QIODevice::readyRead, this, &ClientMain::captureAudio);

    timer = new QTimer(this);
    timer->start(1000);
    connect(timer, &QTimer::timeout, this, &ClientMain::sendAudio);
    connect(udpSocket__, &QUdpSocket::readyRead, this, &ClientMain::readUdpAudio);

    ui__->chatWindow->append("Voice chat started.");
    voiceChatActive = true;
}

void ClientMain::stopVoiceChat()
{
    if (!voiceChatActive) return;

    udpSocket__->writeDatagram("DISCONNECT", QHostAddress("127.0.0.1"), 54321);

    disconnect(timer, &QTimer::timeout, this, &ClientMain::sendAudio);
    disconnect(udpSocket__, &QUdpSocket::readyRead, this, &ClientMain::readUdpAudio);

    // audioInput__->stop();
    // audioOutput__->stop();

    // delete audioInput__;
    // delete audioOutput__;

    udpSocket__->close();

    ui__->chatWindow->append("Voice chat stopped.");
    voiceChatActive = false;
}

void ClientMain::sendAudio() {
    // if (!audioInputDevice__ || !udpSocket__) return;
    if (!ui__->micOnOffButton->isFlat())
        // перенести это в captureAudio, это заменить проверкой длины audioData или чем-то еще, мб громкостью
        return;
    // Считываем данные из аудиоустройства
    // QByteArray audioData = audioInputDevice__->readAll();



    //
    QByteArray audioData;

    audioData.resize(5);
    audioData[0] = QRandomGenerator::global()->bounded(256);
    audioData[1] = QRandomGenerator::global()->bounded(256);
    audioData[2] = QRandomGenerator::global()->bounded(256);
    audioData[3] = QRandomGenerator::global()->bounded(256);
    audioData[4] = QRandomGenerator::global()->bounded(256);
    //



    if (!audioData.isEmpty()) {
        // Отправляем данные через UDP
        // qDebug() << "Send voice" << audioData;
        udpSocket__->writeDatagram(audioData, QHostAddress("127.0.0.1"), 54321);

        // // Воспроизводим данные на аудиовыходе
        // if (audioOutputDevice) {
        //     audioOutputDevice->write(audioData);
        // }
    }
}

void ClientMain::readUdpAudio() {
    if (!ui__->headersOnOffButton->isFlat())
        return;

    while (udpSocket__->hasPendingDatagrams()) {
        QByteArray buffer;
        // audioOutputDevice__->write(buffer);

        buffer.resize(udpSocket__->pendingDatagramSize());
        udpSocket__->readDatagram(buffer.data(), buffer.size());
        qDebug() << buffer;
        // if (audioOutputDevice__) {
        //     audioOutputDevice__->write(buffer);
        // }
    }
}

void ClientMain::on_micOnOffButton_clicked()
{
    ui__->micOnOffButton->setFlat(!ui__->micOnOffButton->isFlat());
}


void ClientMain::on_headersOnOffButton_clicked()
{
    ui__->headersOnOffButton->setFlat(!ui__->headersOnOffButton->isFlat());
}

