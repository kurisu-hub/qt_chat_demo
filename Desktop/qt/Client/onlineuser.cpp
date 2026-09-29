#include "client.h"
#include "onlineuser.h"
#include "protocol.h"
#include "ui_onlineuser.h"

OnlineUser::OnlineUser(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OnlineUser)
{
    ui->setupUi(this);
}

OnlineUser::~OnlineUser()
{
    delete ui;
}

void OnlineUser::updateOnlineUser(QStringList nameList)
{
   ui->listWidget->clear();
   ui->listWidget->addItems(nameList);
}

void OnlineUser::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
    PDU*pdu=mkPDU();
    QString strCurName=Client::getInstance().m_strLoginName;
    QString strTarName=item->text();
    pdu->uiType=ENUM_MSG_TYPE_ADD_FRIEND_REQUEST;
    copyTextField(pdu->caData, strCurName);
    copyTextField(pdu->caData+32, strTarName);
    Client::getInstance().sendMsg(pdu);

}
