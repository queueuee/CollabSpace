#include "systemmanager.h"



SystemManager::SystemManager(QObject *parent)
    : QObject(parent),
    networkManager__(new NetworkManager(this))
{
    connect(networkManager__, &NetworkManager::connectionFailed, this, &SystemManager::handleConnectionFailed);
    connect(networkManager__, &NetworkManager::textMessageReceived, this, &SystemManager::handleMessageReceived);

}

void SystemManager::startAuth(loginAuthData data_)
{
    qDebug() << data_.login << data_.password;
    userLogin = data_.login;

    QString error = networkManager__->connectToCollabSpaceServer(data_.login,
                                                                 data_.password,
                                                                 1);

    emit authFinish(error);
}


void SystemManager::sendMessage(const QString &message)
{
    networkManager__->sendMessage(userLogin, message);
}

void SystemManager::startVoiceChat()
{
    networkManager__->startVoiceChat();
}

void SystemManager::leaveVoiceChat()
{
    networkManager__->leaveVoiceChat();
}

void SystemManager::onOffMicrophone(bool enabled)
{
    networkManager__->onOffMic(enabled);
}

void SystemManager::onOffHeadphones(bool enabled)
{
    networkManager__->onOffHeadphones(enabled);
}


void SystemManager::handleConnectionFailed()
{
    QMessageBox::warning(nullptr, "Connection Error", "Failed to connect to the server.");
}

void SystemManager::handleMessageReceived(const QString &userName, const QString &content, const QString &timestamp)
{
    emit handleMessage(userName, content, timestamp);
}
