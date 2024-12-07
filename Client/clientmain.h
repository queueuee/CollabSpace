#ifndef CLIENTMAIN_H
#define CLIENTMAIN_H

#include "authorization.h"
#include "systemmanager.h"
#include "networkmanager.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QMessageBox>
#include <QTimer>
#include <QScopedPointer>
#include <QDateTime>


QT_BEGIN_NAMESPACE
namespace Ui {
class ClientMain;
}
QT_END_NAMESPACE

class ClientMain : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientMain(QWidget *parent = nullptr);
    ~ClientMain();

private slots:
    void connectToServer();           // Подключение к серверу
    void sendMessage();               // Отправка текстового сообщения
    void startVoiceChat();            // Старт голосового чата
    void leaveVoiceChat();            // Остановка голосового чата
    void on_micOnOffButton_clicked();
    void on_headersOnOffButton_clicked();

    void handleMessageReceived(const QString &userName, const QString &content, const QString &timestamp);
    void handleConnectionSuccess();
    void handleConnectionFailed();

private:
    SystemManager    *systemManager__;
    NetworkManager   *networkManager__;  // Новый класс для управления сетью
    Ui::ClientMain   *ui__;
};

#endif // CLIENTMAIN_H
