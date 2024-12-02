#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>

class ChatClient : public QMainWindow {
    Q_OBJECT

public:
    explicit ChatClient(QWidget *parent = nullptr);
    ~ChatClient();

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

    // GUI элементы
    QLineEdit *messageInput;          // Поле ввода текста
    QTextEdit *chatWindow;            // Окно чата
    QPushButton *sendButton;          // Кнопка отправки текста
    QPushButton *connectButton;       // Кнопка подключения
    QPushButton *voiceConnectButton;  // Кнопка старта голосового чата
    QPushButton *voiceDisconnectButton; // Кнопка остановки голосового чата

    bool voiceChatActive = false;     // Флаг активности голосового чата

    void setupUI();                   // Метод настройки интерфейса
};

#endif // CHATCLIENT_H
