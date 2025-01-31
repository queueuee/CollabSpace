#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include "networkmanager.h"

#include <QObject>
#include <QDebug>
#include <QMessageBox>
#include <QMessageBox>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>



struct loginAuthData
{
    QString login = "";
    QString password = "";
};

class SystemManager : public QObject
{
    Q_OBJECT
public:
    explicit SystemManager(QObject *parent = nullptr);
    ~SystemManager() {};

    void sendMessage(const QString &message);
    void startVoiceChat();
    void leaveVoiceChat();
    void onOffMicrophone(bool);
    void onOffHeadphones(bool);


    // заглушка, пока нет бд
    QString userLogin;

    void startAuth(loginAuthData data_);

private:
    // Управление передачей сообщений
    NetworkManager                                  *networkManager__;

private slots:
    void handleConnectionFailed();
    void handleMessageReceived(const QString &userName,
                               const QString &content,
                               const QString &timestamp);


signals:
    void authFinish(QString error_);
    void handleMessage(const QString &userName,
                       const QString &content,
                       const QString &timestamp);
};

#endif // SYSTEMMANAGER_H
