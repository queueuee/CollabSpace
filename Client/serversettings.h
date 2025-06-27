#ifndef SERVERSETTINGS_H
#define SERVERSETTINGS_H

#include <QMainWindow>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QFrame>
#include <QMap>
#include <QPushButton>



namespace Ui {
class serverSettings;
}

class serverSettings : public QMainWindow
{
    Q_OBJECT
    class InviteWidgetTemplate : public QFrame
    {
    public:
        InviteWidgetTemplate(const QJsonObject, QFrame *parent = nullptr);
        QPushButton* getSendInvBtn() {return sendInvBtn__;};
    private:
        QPushButton* sendInvBtn__;
    };

public:
    explicit serverSettings(const QString serverName, QWidget *parent = nullptr);
    void addIvite(const QJsonObject);
    QMap<int, InviteWidgetTemplate*> getInviteList() {return invites__; };
    ~serverSettings();


private slots:
    void on_rolesSettingsPbtn_clicked();

    void on_invitationsSettingsPbtn_clicked();

signals:
    void on_createInvite(const QJsonObject);

private:
    Ui::serverSettings *ui;

    // Заменить на нормальный класс Invite
    QMap<int, InviteWidgetTemplate*> invites__;
};

#endif // SERVERSETTINGS_H
