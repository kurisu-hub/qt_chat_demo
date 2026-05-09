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
    memcpy(pdu->caData,strCurName.toStdString().c_str(),32);
    memcpy(pdu->caData+32,strTarName.toStdString().c_str(),32);
    qDebug()<<"pdu->caData:"<<pdu->caData;
    qDebug()<<"pdu->caData+32:"<<pdu->caData+32;
    Client::getInstance().sendMsg(pdu);

}
