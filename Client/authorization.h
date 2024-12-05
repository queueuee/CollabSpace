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
    explicit Authorization(SystemManager*& systemManager_, QWidget *parent = nullptr);
    ~Authorization();

private slots:
    void on_connectBtn_clicked();

    void on_cancelBtn_clicked();

private:
    Ui::Authorization *ui__;
    SystemManager*& systemManager__;

    void showWarningMessage(const QString &message_);
    void authFinished(QString error_);
    void disableFieldsOnAuthProcess(bool);
};

#endif // AUTHORIZATION_H
