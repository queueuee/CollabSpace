#ifndef CLIENTMAIN_H
#define CLIENTMAIN_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QMessageBox>

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
    void stopVoiceChat();             // Остановка голосового чата
    void receiveMessage();            // Получение текстового сообщения
    void captureAudio();              // Запись и отправка аудио
    void readUdpAudio();              // Чтение и воспроизведение аудио

private:
    // Сетевые сокеты
    QTcpSocket *tcpSocket;            // TCP сокет для текстовых сообщений
    QUdpSocket *udpSocket;            // UDP сокет для голосового чата

    // Аудио компоненты
    QAudioInput *audioInput;          // Вход для записи аудио
    QAudioOutput *audioOutput;        // Выход для воспроизведения аудио
    QIODevice *audioInputDevice;      // Устройство для записи аудио
    QIODevice *audioOutputDevice;     // Устройство для воспроизведения аудио
    Ui::ClientMain *ui;

    bool voiceChatActive = false;     // Флаг активности голосового чата

    void setupUI();                   // Метод настройки интерфейса

};
#endif // CLIENTMAIN_H
