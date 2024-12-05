#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H


#include <QObject>
#include <QDebug>

struct dbAuthData
{
    QString login = "";
    QString password = "";
};

class SystemManager : public QObject
{
    Q_OBJECT
public:
    SystemManager() {};
    ~SystemManager() {};

    // заглушка, пока нет бд
    QString userLogin;

    void startAuth(dbAuthData data_);
signals:
    void authFinish(QString error_);
};

#endif // SYSTEMMANAGER_H
