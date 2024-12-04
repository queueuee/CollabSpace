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

#include <QTimer>
#include <QRandomGenerator>

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
    void sendAudio();              // Запись и отправка аудио
    void readUdpAudio();              // Чтение и воспроизведение аудио

    void on_micOnOffButton_clicked();

    void on_headersOnOffButton_clicked();

private:
    QTimer *timer;
    // Сетевые сокеты
    QTcpSocket                              *tcpSocket__;            // TCP сокет для текстовых сообщений
    QUdpSocket                              *udpSocket__;            // UDP сокет для голосового чата

    // // Аудио компоненты
    // QAudioInput                             *audioInput__;          // Вход для записи аудио
    // QAudioOutput                            *audioOutput__;        // Выход для воспроизведения аудио
    // QIODevice                               *audioInputDevice__;      // Устройство для записи аудио
    // QIODevice                               *audioOutputDevice__;     // Устройство для воспроизведения аудио

    Ui::ClientMain                          *ui__;

    bool                                    voiceChatActive = false;     // Флаг активности голосового чата

};
#endif // CLIENTMAIN_H
