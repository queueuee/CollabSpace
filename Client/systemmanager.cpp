#include "systemmanager.h"

void SystemManager::startAuth(dbAuthData data_)
{
    qDebug() << data_.login << data_.password;
    userLogin = data_.login;
    QString error = "";

    emit authFinish(error);
}
