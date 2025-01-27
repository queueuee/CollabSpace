#include <QCoreApplication>
#include "ChatServer.h"


#define DB_HOSTNAME "127.0.0.1"
#define DB_NAME "prison"
#define DB_USERNAME "vitaliy"
#define DB_PASSWORD "admin"


int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(DB_HOSTNAME);
    //db.setHostName("195.58.52.85");
    db.setDatabaseName(DB_NAME);
    db.setUserName(DB_USERNAME);
    db.setPassword(DB_PASSWORD);

    if (!db.open())
    {
        qCritical() << "Database connection Error." << db.lastError().text();
        return -1;
    }
    else
    {
        qDebug() << "Db started on:" <<  db.hostName() << db.databaseName() << db.userName() << db.password();
    }


    ChatServer server(&db);
    if (!server.startServer(12345, 54321))
    {
        qCritical() << "Failed to start the server.";
        return -1;
    }

    qDebug() << "Server is running.";
    return app.exec();
}
