#include "serversettings.h"
#include "ui_serversettings.h"

#include <QDebug>
#include <QComboBox>

serverSettings::serverSettings(const QString serverName_, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::serverSettings)
{
    ui->setupUi(this);
    setWindowTitle("Настройки " + serverName_);
    setAttribute(Qt::WA_DeleteOnClose);
}

void serverSettings::addIvite(const QJsonObject invData)
{
    InviteWidgetTemplate* invite = new InviteWidgetTemplate(invData);
    int invite_id = invData["invite_id"].toInt();

    if (invites__.contains(invite_id))
        invites__[invite_id]->deleteLater();

    invites__.insert(invite_id, invite);
    ui->invitesListWidget->layout()->addWidget(invite);
}

serverSettings::~serverSettings()
{
    delete ui;
}

void serverSettings::on_rolesSettingsPbtn_clicked()
{
    ui->rolesSettingsPbtn->setFlat(!ui->rolesSettingsPbtn->isFlat());
    ui->rolesHandler->setVisible(ui->rolesSettingsPbtn->isFlat());
}


void serverSettings::on_invitationsSettingsPbtn_clicked()
{
    ui->invitationsSettingsPbtn->setFlat(!ui->invitationsSettingsPbtn->isFlat());
    ui->invitationsHandler->setVisible(ui->invitationsSettingsPbtn->isFlat());
}


serverSettings::InviteWidgetTemplate::InviteWidgetTemplate(const QJsonObject invData, QFrame *parent)
{
    QString invite_url = invData["invite_url"].toString();
    int invite_author_id = invData["invite_author_id"].toInt();
    int uses_left = invData["invite_uses_left"].isNull()
                        ? -1
                        : invData["invite_uses_left"].toInt();

    QString datetimeStr = invData["invite_created_at"].toString();
    QDateTime dateTime = QDateTime::fromString(datetimeStr, Qt::ISODate);
    QString created_at;
    created_at = dateTime.toString("hh:mm | dd.MM.yyyy");

    QString expired_at = invData["invite_expired_at"].isNull()
                             ? QString()
                             : invData["invite_expired_at"].toString();

    int role_id = invData["role_id"].toInt();
    QString role_name = invData["role_name"].toString();
    bool role_is_highest = invData["role_is_highest"].toBool();
    int user_permissions = invData["role_user_permissions"].toInt();
    int admin_permissions = invData["role_admin_permissions"].toInt();

    int server_id = invData["server_id"].toInt();
    QString server_name = invData["server_name"].toString();
    QString server_description = invData["server_description"].toString();
    QJsonValue server_invite_default_id = invData["server_invite_default_id"];

    QGridLayout *mainLayout = new QGridLayout();
    mainLayout->addWidget(new QLabel("Автор:"), 0, 0);
    mainLayout->addWidget(new QLabel("user" + QString::number(invite_author_id)), 0, 1);
    mainLayout->addWidget(new QLabel("Действует до:"), 0, 3);
    mainLayout->addWidget(new QLabel(expired_at), 0, 4);
    mainLayout->addWidget(new QLabel("Создано:"), 1, 3);
    mainLayout->addWidget(new QLabel(created_at), 1, 4);
    QComboBox *rolesComboBox = new QComboBox();
    rolesComboBox->addItem("Гость", QVariant(1));
    rolesComboBox->addItem("Участник", QVariant(2));
    rolesComboBox->addItem("Модератор", QVariant(3));
    rolesComboBox->addItem("Администратор", QVariant(4));
    rolesComboBox->setCurrentIndex(0);
    mainLayout->addWidget(rolesComboBox, 0, 5, 2, 1);
    sendInvBtn__ = new QPushButton("Пригласить");
    mainLayout->addWidget(sendInvBtn__, 0, 6);
    mainLayout->addWidget(new QPushButton("Удалить"), 1, 6);

    setLayout(mainLayout);
}
