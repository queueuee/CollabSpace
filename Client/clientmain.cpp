#include "clientmain.h"
#include "./ui_clientmain.h"


const QString programmName = "Collab Space";

ClientMain::ClientMain(QWidget *parent)
    : QMainWindow(parent),
    ui__(new Ui::ClientMain),
    systemManager__(nullptr)
{
    ui__->setupUi(this);

    // Авторизация
    QTimer::singleShot(50, this, [this]() {
        Authorization auth(systemManager__);
        auth.setModal(true);
        if (auth.exec() == QDialog::Accepted) {
            setWindowTitle(programmName + " - " + systemManager__->userLogin);

            connect(systemManager__, &SystemManager::handleMessage, this, &ClientMain::handleMessageReceived);
        } else {
            QApplication::quit();
        }
    });


    connect(ui__->sendButton, &QPushButton::clicked, this, &ClientMain::sendMessage);
    connect(ui__->voiceConnectButton, &QPushButton::clicked, this, &ClientMain::startVoiceChat);
    connect(ui__->voiceDisconnectButton, &QPushButton::clicked, this, &ClientMain::leaveVoiceChat);

    ui__->micOnOffButton->setFlat(micEnabled__);
    ui__->headersOnOffButton->setFlat(headphonesEnabled__);
}

ClientMain::~ClientMain()
{
    delete ui__;
}

void ClientMain::sendMessage()
{
    QString message = ui__->messageInput->text();
    if (!message.isEmpty()) {
        systemManager__ -> sendMessage(message);
        ui__->chatWindow->append(systemManager__->userLogin + " " + QDateTime::currentDateTime().toString("HH:mm") + " (You)\n" + message + "\n");
        ui__->messageInput->clear();
    }
}

void ClientMain::startVoiceChat()
{
    systemManager__ -> startVoiceChat();
    ui__->chatWindow->append("Voice chat started.");
}

void ClientMain::leaveVoiceChat()
{
    systemManager__->leaveVoiceChat();
    ui__->chatWindow->append("Voice chat stopped.");
}

void ClientMain::on_micOnOffButton_clicked()
{
    micEnabled__ = !micEnabled__;
    systemManager__ -> onOffMicrophone(micEnabled__);
    ui__->micOnOffButton->setFlat(micEnabled__);
    ui__->micOnOffButton->setIcon(micEnabled__ ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));

    if (micEnabled__ && !headphonesEnabled__)
    {
        headphonesEnabled__ = true;
        systemManager__ -> onOffHeadphones(headphonesEnabled__);
        ui__->headersOnOffButton->setFlat(headphonesEnabled__);
        ui__->headersOnOffButton->setIcon(headphonesEnabled__ ? QIcon(":/icons/headers.png") : QIcon(":/icons/headersOff.png"));
    }
}

void ClientMain::on_headersOnOffButton_clicked()
{
    headphonesEnabled__ = !headphonesEnabled__;
    systemManager__ -> onOffHeadphones(headphonesEnabled__);
    ui__->headersOnOffButton->setFlat(headphonesEnabled__);
    ui__->headersOnOffButton->setIcon(headphonesEnabled__ ? QIcon(":/icons/headers.png") : QIcon(":/icons/headersOff.png"));

    if (!headphonesEnabled__ && micEnabled__)
    {
        micEnabled__ = false;
        systemManager__ -> onOffMicrophone(micEnabled__);
        ui__->micOnOffButton->setFlat(micEnabled__);
        ui__->micOnOffButton->setIcon(micEnabled__ ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));
    }
    if (headphonesEnabled__ && !micEnabled__)
    {
        micEnabled__ = true;
        systemManager__ -> onOffMicrophone(micEnabled__);
        ui__->micOnOffButton->setFlat(micEnabled__);
        ui__->micOnOffButton->setIcon(micEnabled__ ? QIcon(":/icons/mic.png") : QIcon(":/icons/micOff.png"));
    }
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
