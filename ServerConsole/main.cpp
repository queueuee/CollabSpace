#include <QCoreApplication>
#include "ChatServer.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    ChatServer server;
    if (!server.startServer(12345, 54321)) {
        qCritical() << "Failed to start the server.";
        return -1;
    }

    qDebug() << "Server is running.";
    return app.exec();
}
