#include "clientmain.h"
#include "./ui_clientmain.h"

ClientMain::ClientMain(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ClientMain),
    tcpSocket(new QTcpSocket(this)),
    udpSocket(new QUdpSocket(this)),
    audioInput(nullptr),
    audioOutput(nullptr),
    audioInputDevice(nullptr),
    audioOutputDevice(nullptr)
{
    ui->setupUi(this);

    connect(ui->connectButton, &QPushButton::clicked, this, &ClientMain::connectToServer);
    connect(ui->sendButton, &QPushButton::clicked, this, &ClientMain::sendMessage);
    connect(ui->voiceConnectButton, &QPushButton::clicked, this, &ClientMain::startVoiceChat);
    connect(ui->voiceDisconnectButton, &QPushButton::clicked, this, &ClientMain::stopVoiceChat);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &ClientMain::receiveMessage);
}

ClientMain::~ClientMain()
{
    if (voiceChatActive) {
        stopVoiceChat();
    }
    if (tcpSocket->isOpen()) {
        tcpSocket->close();
    }
    if (udpSocket->isOpen()) {
        udpSocket->close();
    }

    delete ui;
}


void ClientMain::connectToServer() {
    tcpSocket->connectToHost(QHostAddress("127.0.0.1"), 12345);
    if (tcpSocket->waitForConnected(3000)) {
        ui->chatWindow->append("Connected to the server.");
    } else {
        QMessageBox::warning(this, "Connection Error", "Failed to connect to the server.");
    }
}

void ClientMain::sendMessage() {
    QString message = ui->messageInput->text();
    if (!message.isEmpty() && tcpSocket->state() == QTcpSocket::ConnectedState) {
        tcpSocket->write(message.toUtf8());
        ui->chatWindow->append("You: " + message);
        ui->messageInput->clear();
    }
}

void ClientMain::receiveMessage() {
    QByteArray data = tcpSocket->readAll();
    ui->chatWindow->append("Server: " + QString::fromUtf8(data));
}

void ClientMain::startVoiceChat() {
    if (voiceChatActive)
        return;

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    audioInput = new QAudioInput(format, this);
    audioOutput = new QAudioOutput(format, this);

    if (!udpSocket->bind(QHostAddress::AnyIPv4, quint16(0), QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        ui->chatWindow->append("Failed to bind UDP socket.");
        delete audioInput;
        audioInput = nullptr;
        return;
    }
    audioInputDevice = audioInput->start();
    audioOutputDevice = audioOutput->start();

    connect(audioInputDevice, &QIODevice::readyRead, this, &ClientMain::captureAudio);
    connect(udpSocket, &QUdpSocket::readyRead, this, &ClientMain::readUdpAudio);

    ui->chatWindow->append("Voice chat started.");
    voiceChatActive = true;
}

void ClientMain::stopVoiceChat() {
    if (!voiceChatActive) return;

    audioInput->stop();
    audioOutput->stop();

    delete audioInput;
    delete audioOutput;

    udpSocket->close();

    ui->chatWindow->append("Voice chat stopped.");
    voiceChatActive = false;
}

void ClientMain::captureAudio() {
    if (!audioInputDevice || !udpSocket) return;

    // Считываем данные из аудиоустройства
    QByteArray audioData = audioInputDevice->readAll();
    if (!audioData.isEmpty()) {
        // Отправляем данные через UDP
        udpSocket->writeDatagram(audioData, QHostAddress("127.0.0.1"), 54321);

        // // Воспроизводим данные на аудиовыходе
        // if (audioOutputDevice) {
        //     audioOutputDevice->write(audioData);
        // }
    }
}

void ClientMain::readUdpAudio() {
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray buffer;
        audioOutputDevice->write(buffer);

        buffer.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(buffer.data(), buffer.size());

        if (audioOutputDevice) {
            audioOutputDevice->write(buffer);
        }
    }
}
