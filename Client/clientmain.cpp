#include "clientmain.h"
#include "./ui_clientmain.h"


const QString programmName = "Collab Space";

ClientMain::ClientMain(QWidget *parent)
    : QMainWindow(parent),
    ui__(new Ui::ClientMain),
    systemManager__(nullptr),
    networkManager__(new NetworkManager(this))
{
    ui__->setupUi(this);

    // Авторизация
    QTimer::singleShot(50, this, [this]() {
        Authorization auth(systemManager__);
        auth.setModal(true);
        if (auth.exec() == QDialog::Accepted) {
            setWindowTitle(programmName + " - " + systemManager__->userLogin);
            connectToServer();
        } else {
            QApplication::quit();
        }
    });

    // Подключение сигналов
    connect(networkManager__, &NetworkManager::connectionSuccess, this, &ClientMain::handleConnectionSuccess);
    connect(networkManager__, &NetworkManager::connectionFailed, this, &ClientMain::handleConnectionFailed);
    connect(networkManager__, &NetworkManager::messageReceived, this, &ClientMain::handleMessageReceived);

    connect(ui__->sendButton, &QPushButton::clicked, this, &ClientMain::sendMessage);
    connect(ui__->voiceConnectButton, &QPushButton::clicked, this, &ClientMain::startVoiceChat);
    connect(ui__->voiceDisconnectButton, &QPushButton::clicked, this, &ClientMain::leaveVoiceChat);

    ui__->micOnOffButton->setFlat(true);
    ui__->headersOnOffButton->setFlat(true);
}

ClientMain::~ClientMain()
{
    delete ui__;
}

void ClientMain::connectToServer()
{
    networkManager__->connectToServer("127.0.0.1", 12345);
}

void ClientMain::sendMessage()
{
    QString message = ui__->messageInput->text();
    if (!message.isEmpty()) {
        networkManager__->sendMessage(systemManager__->userLogin, message);
        ui__->chatWindow->append(systemManager__->userLogin + " " + QDateTime::currentDateTime().toString("HH:mm") + " (You)\n" + message + "\n");
        ui__->messageInput->clear();
    }
}

void ClientMain::startVoiceChat()
{
    networkManager__->startVoiceChat();
    ui__->chatWindow->append("Voice chat started.");
}

void ClientMain::leaveVoiceChat()
{
    networkManager__->leaveVoiceChat();
    ui__->chatWindow->append("Voice chat stopped.");
}

void ClientMain::on_micOnOffButton_clicked()
{
    bool micEnabled = !ui__->micOnOffButton->isFlat();
    networkManager__->onOffMic(micEnabled);
    ui__->micOnOffButton->setFlat(micEnabled);
    ui__->micOnOffButton->setIcon(micEnabled ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));
}

void ClientMain::on_headersOnOffButton_clicked()
{
    bool headphonesEnabled = !ui__->headersOnOffButton->isFlat();
    networkManager__->onOffHeadphones(headphonesEnabled);
    ui__->headersOnOffButton->setFlat(headphonesEnabled);
    ui__->headersOnOffButton->setIcon(headphonesEnabled ? QIcon(":/icons/headers.png") : QIcon(":/icons/headersOff.png"));
}

void ClientMain::handleMessageReceived(const QString &userName, const QString &content, const QString &timestamp)
{
    ui__->chatWindow->append(userName + " " + timestamp + "\n" + content + "\n");
}

void ClientMain::handleConnectionSuccess()
{
    ui__->chatWindow->append("Connected to the server.");
}

void ClientMain::handleConnectionFailed()
{
    QMessageBox::warning(this, "Connection Error", "Failed to connect to the server.");
}
