#include "authorization.h"

Authorization::Authorization(SystemManager*& systemManager_, QWidget *parent)
    : QDialog(parent)
    , systemManager__(systemManager_)
    , ui__(new Ui::Authorization)
{
    ui__->setupUi(this);
}

Authorization::~Authorization()
{
    delete ui__;
}

void Authorization::showWarningMessage(const QString &message_)
{
    QMessageBox warningBox;
    warningBox.setIcon(QMessageBox::Warning);
    warningBox.setStandardButtons(QMessageBox::Ok);
    warningBox.setWindowTitle("Warning");
    warningBox.setText(message_);

    warningBox.exec();
}

void Authorization::on_connectBtn_clicked()
{
    if (ui__->loginCheck->text().length() == 0) {
        showWarningMessage("Не задан логин. Подключение невозможно");
        return;
    }
    if (ui__->passCheck->text().length() == 0) {
        showWarningMessage("Не задан пароль. Подключение невозможно");
        return;
    }

    systemManager__ = new SystemManager();
    connect(systemManager__, &SystemManager::authFinish, this, &Authorization::authFinished);

    disableFieldsOnAuthProcess(false);

    systemManager__->startAuth({ui__->loginCheck->text(), ui__->passCheck->text()});
    accept();
}

void Authorization::on_cancelBtn_clicked()
{
    reject();
}

void Authorization::authFinished(QString error_)
{
    disableFieldsOnAuthProcess(true);
    if (error_ == "ok")
        accept();
    else
        showWarningMessage(error_);
}

void Authorization::disableFieldsOnAuthProcess(bool enable_)
{
    ui__->loginCheck->setEnabled(enable_);
    ui__->passCheck->setEnabled(enable_);
    ui__->connectBtn->setEnabled(enable_);

}
