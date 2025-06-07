#ifndef AUTHORIZATION_H
#define AUTHORIZATION_H

#include "ui_authorization.h"

#include "systemmanager.h"

#include <QDialog>
#include <QObject>
#include <QMessageBox>

namespace Ui {
class Authorization;
}

class Authorization : public QDialog
{
    Q_OBJECT

public:
    explicit Authorization(NetworkManager* networkManager_, QWidget *parent = nullptr);
    const UserData getUserData() { return userData__; };
    ~Authorization();

private slots:
    void on_connectBtn_clicked();

    void on_cancelBtn_clicked();

private:

    void showWarningMessage(const QString &message_);
    void authFinished(const QJsonObject &userData);
    void enableFieldsOnAuthProcess(bool);

    UserData                               userData__;
    NetworkManager                              *networkManager__;

    Ui::Authorization                           *ui__;
};

#endif // AUTHORIZATION_H
