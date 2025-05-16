#include "authorization.h"
#include "../possible_requests.h"

#define COLLAB_SPACE_URL "ws://127.0.0.1:12345"


Authorization::Authorization(NetworkManager* networkManager_, QWidget *parent)
    : QDialog(parent)
    , networkManager__(networkManager_)
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

    connect(networkManager__, &NetworkManager::getAuthToken, this, &Authorization::authFinished);

    enableFieldsOnAuthProcess(false);
    userData__.login = ui__->loginCheck->text();
    userData__.password = ui__->passCheck->text();

    networkManager__->connectToServer(userData__.login, userData__.password, QUrl(COLLAB_SPACE_URL), 1);
}

void Authorization::on_cancelBtn_clicked()
{
    reject();
}

void Authorization::authFinished(const QJsonObject &userData)
{
    qDebug() << userData;
    QString error;
    if(userData.value(TYPE).toString().trimmed() == OK)
    {
        QJsonObject info = userData.value(INFO).toObject();
        userData__.user_id = info.value("user_id").toInt();

        error = "";
    }
    else
    {
        QJsonObject infoObject= userData.value(INFO).toObject();
        QJsonDocument doc(infoObject);

        error = doc.toJson(QJsonDocument::Compact);
    }

    enableFieldsOnAuthProcess(true);
    if (error.isEmpty())
        accept();
    else
    {
        showWarningMessage(error);
        qDebug() << error;
        reject();
    }
}

void Authorization::enableFieldsOnAuthProcess(bool enable_)
{
    ui__->loginCheck->setEnabled(enable_);
    ui__->passCheck->setEnabled(enable_);
    ui__->connectBtn->setEnabled(enable_);

}
