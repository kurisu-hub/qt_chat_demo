#include "client.h"
#include "friend.h"
#include "protocol.h"
#include "ui_friend.h"

#include <QInputDialog>
#include <QDebug>
#include <QSet>
Friend::Friend(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Friend)
{   //在构造函数里面调用刷新函数
    ui->setupUi(this);
    m_pOnlineUser=new OnlineUser;
    m_pChat=new Chat;
    flushfriend();

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
    for (const QString &friendName : friendList) {
        QListWidgetItem *item = new QListWidgetItem(friendName, ui->listwidget);
        item->setData(Qt::UserRole, friendName);
        refreshFriendItem(item);
    }
}

void Friend::applyPresenceSnapshot(const QStringList &onlineFriends)
{
    m_onlineFriends = QSet<QString>(onlineFriends.begin(), onlineFriends.end());
    for (int i = 0; i < ui->listwidget->count(); ++i) refreshFriendItem(ui->listwidget->item(i));
}

void Friend::updateFriendPresence(const QString &friendName, bool online)
{
    if (online) m_onlineFriends.insert(friendName);
    else m_onlineFriends.remove(friendName);
    for (int i = 0; i < ui->listwidget->count(); ++i) {
        QListWidgetItem *item = ui->listwidget->item(i);
        if (item->data(Qt::UserRole).toString() == friendName) {
            refreshFriendItem(item);
            return;
        }
    }
}

void Friend::refreshFriendItem(QListWidgetItem *item)
{
    const QString friendName = item->data(Qt::UserRole).toString();
    item->setText(friendName + (m_onlineFriends.contains(friendName)
        ? QString::fromUtf8(" [在线]") : QString::fromUtf8(" [离线]")));
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
    QString strTarName=pItem->data(Qt::UserRole).toString();
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
    m_pChat->m_strChatName=pItem->data(Qt::UserRole).toString();
    if(m_pChat->isHidden()){
        m_pChat->show();
    }
}
