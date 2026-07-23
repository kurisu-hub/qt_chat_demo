#include "client.h"
#include "friend.h"
#include "protocol.h"
#include "ui_friend.h"

#include <QInputDialog>
#include <QDebug>
Friend::Friend(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Friend)
{   //在构造函数里面调用刷新函数
    flushfriend();
    ui->setupUi(this);
    m_pOnlineUser=new OnlineUser;
    m_pChat=new Chat;

}

QListWidget *Friend::getFriend_LW()
{
     return ui->listwidget;
}

Friend::~Friend()
{
    delete ui;
    delete m_pOnlineUser;
    delete m_pChat;
}
//用于刷新好友列表的，传参为好友数组
void Friend::flushFriend_LW(QStringList friendList)
{
    ui->listwidget->clear();
    ui->listwidget->addItems(friendList);
}

void Friend::flushfriend()
{   //获取登录名方便获取列表，把登录名发送给服务器
    PDU*pdu=mkPDU();
    memcpy(pdu->caData,Client::getInstance().m_strLoginName.toStdString().c_str(),32);
    pdu->uiType=ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST;
    Client::getInstance().sendMsg(pdu);
}



void Friend::on_findUser_PB_clicked()
{
    //查找用户名，用getText函数，并且判断是否为空
    QString strName=QInputDialog::getText(this,"查找用户","用户名");
    qDebug()<<"strName:"<<strName;
    if(strName.isEmpty())
    {
        return;
    }
    //把信息发送给服务器，让服务器进行查找就行了
    PDU*pdu=mkPDU();
    memcpy(pdu->caData,strName.toStdString().c_str(),32);
    pdu->uiType=ENUM_MSG_TYPE_FIND_USER_REQUEST;
    Client::getInstance().sendMsg(pdu);
}

void Friend::on_onlieUser_PB_clicked()
{
    if(m_pOnlineUser->isHidden())
    {
        m_pOnlineUser->show();
    }
    PDU*pdu=mkPDU();
    pdu->uiType=ENUM_MSG_TYPE_ONLINE_USER_REQUEST;
    Client::getInstance().sendMsg(pdu);
}

void Friend::on_flush_PB_clicked()
{
    flushfriend();
}


void Friend::on_del_PB_clicked()
{

    QListWidgetItem* pItem = ui->listwidget->currentItem();
    if(!pItem){
        return;
    }
    QString strTarName=pItem->text();
    PDU*pdu=mkPDU();
    memcpy(pdu->caData,Client::getInstance().m_strLoginName.toStdString().c_str(),32);//第一个是用户
    memcpy(pdu->caData+32,strTarName.toStdString().c_str(),32);//第二个是目标
    pdu->uiType=ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST;
    Client::getInstance().sendMsg(pdu);
}

void Friend::on_chat_PB_clicked()
{
    QListWidgetItem* pItem = ui->listwidget->currentItem();
    if(!pItem){
        return;
    }
    m_pChat->m_strChatName=pItem->text();
    if(m_pChat->isHidden()){
        m_pChat->show();
    }
}
