#include "ChatClient.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ChatClient client;
    client.show();

    // ChatClient client1;
    // client1.show();
    return app.exec();
}
