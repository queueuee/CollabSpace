#include "ChatClient.h"
#include <QHostAddress>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

ChatClient::ChatClient(QWidget *parent)
    : QMainWindow(parent),
    tcpSocket(new QTcpSocket(this)),
    udpSocket(new QUdpSocket(this)),
    audioInput(nullptr),
    audioOutput(nullptr),
    audioInputDevice(nullptr),
    audioOutputDevice(nullptr) {

    setupUI();

    connect(connectButton, &QPushButton::clicked, this, &ChatClient::connectToServer);
    connect(sendButton, &QPushButton::clicked, this, &ChatClient::sendMessage);
    connect(voiceConnectButton, &QPushButton::clicked, this, &ChatClient::startVoiceChat);
    connect(voiceDisconnectButton, &QPushButton::clicked, this, &ChatClient::stopVoiceChat);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &ChatClient::receiveMessage);
}

ChatClient::~ChatClient() {
    if (voiceChatActive) {
        stopVoiceChat();
    }
    if (tcpSocket->isOpen()) {
        tcpSocket->close();
    }
    if (udpSocket->isOpen()) {
        udpSocket->close();
    }
}

void ChatClient::setupUI() {
    messageInput = new QLineEdit(this);
    chatWindow = new QTextEdit(this);
    chatWindow->setReadOnly(true);

    sendButton = new QPushButton("Send", this);
    connectButton = new QPushButton("Connect", this);
    voiceConnectButton = new QPushButton("Start Voice Chat", this);
    voiceDisconnectButton = new QPushButton("Stop Voice Chat", this);

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(chatWindow);

    QHBoxLayout *messageLayout = new QHBoxLayout();
    messageLayout->addWidget(messageInput);
    messageLayout->addWidget(sendButton);
    mainLayout->addLayout(messageLayout);

    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->addWidget(connectButton);
    controlLayout->addWidget(voiceConnectButton);
    controlLayout->addWidget(voiceDisconnectButton);
    mainLayout->addLayout(controlLayout);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    setWindowTitle("Chat Client");
    resize(400, 300);
}

void ChatClient::connectToServer() {
    tcpSocket->connectToHost(QHostAddress("127.0.0.1"), 12345);
    if (tcpSocket->waitForConnected(3000)) {
        chatWindow->append("Connected to the server.");
    } else {
        QMessageBox::warning(this, "Connection Error", "Failed to connect to the server.");
    }
}

void ChatClient::sendMessage() {
    QString message = messageInput->text();
    if (!message.isEmpty() && tcpSocket->state() == QTcpSocket::ConnectedState) {
        tcpSocket->write(message.toUtf8());
        chatWindow->append("You: " + message);
        messageInput->clear();
    }
}

void ChatClient::receiveMessage() {
    QByteArray data = tcpSocket->readAll();
    chatWindow->append("Server: " + QString::fromUtf8(data));
}

void ChatClient::startVoiceChat() {
    if (voiceChatActive) return;

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    audioInput = new QAudioInput(format, this);
    audioOutput = new QAudioOutput(format, this);

    if (!udpSocket->bind(QHostAddress::AnyIPv4, quint16(0), QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        chatWindow->append("Failed to bind UDP socket.");
        delete audioInput;
        audioInput = nullptr;
        return;
    }
    audioInputDevice = audioInput->start();
    audioOutputDevice = audioOutput->start();

    connect(audioInputDevice, &QIODevice::readyRead, this, &ChatClient::captureAudio);
    connect(udpSocket, &QUdpSocket::readyRead, this, &ChatClient::readUdpAudio);

    chatWindow->append("Voice chat started.");
    voiceChatActive = true;
}

void ChatClient::stopVoiceChat() {
    if (!voiceChatActive) return;

    audioInput->stop();
    audioOutput->stop();

    delete audioInput;
    delete audioOutput;

    udpSocket->close();

    chatWindow->append("Voice chat stopped.");
    voiceChatActive = false;
}

void ChatClient::captureAudio() {
    if (!audioInputDevice || !udpSocket) return;

    // Считываем данные из аудиоустройства
    QByteArray audioData = audioInputDevice->readAll();
    if (!audioData.isEmpty()) {
        // Отправляем данные через UDP
        udpSocket->writeDatagram(audioData, QHostAddress("127.0.0.1"), 54321);

        // Воспроизводим данные на аудиовыходе
        if (audioOutputDevice) {
            audioOutputDevice->write(audioData);
        }
    }
}

void ChatClient::readUdpAudio() {
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(buffer.data(), buffer.size());

        if (audioOutputDevice) {
            audioOutputDevice->write(buffer);
        }
    }
}
