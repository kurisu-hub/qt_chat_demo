#include "mytcpsocket.h"
#include "protocol.h"
#include "operatedb.h"
#include "mytcpserver.h"
#include "presencestore.h"
#include <QUuid>
MyTcpSocket::MyTcpSocket()
{

    connect(this,&QTcpSocket::readyRead,this,&MyTcpSocket::recvMsg);
    connect(this,&QTcpSocket::disconnected,this,&MyTcpSocket::clientOffline);
    m_pmh=new MsgHandler;
    //初始化最后活跃时间
    m_lastActiveTime = QDateTime::currentDateTime();
}

PDU *MyTcpSocket::readMsg()
{
    qDebug()<<"socket中的总长度为:"<<this->bytesAvailable();
    uint uiPDULen=0;
    this->read((char*)&uiPDULen,sizeof(uint));
    uint uiMsgLen=uiPDULen-sizeof(PDU);
    PDU*pdu=mkPDU(uiMsgLen);
    this->read((char*)pdu+sizeof(uint),uiPDULen-sizeof(uint));
    qDebug()<<"uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
    return pdu;
}

PDU *MyTcpSocket::handleMsg(PDU *pdu)
{
    qDebug()<<"readMsg uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
    PDU*respdu=NULL;
    m_pmh->pdu=pdu;
    switch (pdu->uiType){
    case ENUM_MSG_TYPE_REGIST_REQUEST:
    {
            respdu=m_pmh->regist();
            break;
    }
    case ENUM_MSG_TYPE_LOGIN_REQUEST:
    {
        respdu=m_pmh->login(m_strLoginName);
        bool loginOk = false;
        memcpy(&loginOk, respdu->caData, sizeof(bool));
        if (loginOk) {
            m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_authenticated = true;
            m_offlineHandled = false;
            if (PresenceStore::getInstance().login(m_strLoginName, m_sessionId)) {
                MyTcpServer::getInstance().notifyFriendsPresence(m_strLoginName, true, 0);
            }
        }
        break;
    }
    case ENUM_MSG_TYPE_FIND_USER_REQUEST:
    {
        respdu=m_pmh->findUser();
        break;
    }
    case ENUM_MSG_TYPE_ONLINE_USER_REQUEST:
    {
        respdu=m_pmh->onlineUser();
        break;
    }
    case ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_REQUEST:
    {
        QByteArray owner = m_strLoginName.toUtf8();
        char ownerName[32] = {0};
        memcpy(ownerName, owner.constData(), qMin(owner.size(), 31));
        const QStringList friends = OperateDB::getInstance().handleFlushFriend(ownerName);
        QStringList onlineFriends;
        for (const QString &friendName : friends) {
            if (PresenceStore::getInstance().isOnline(friendName)) onlineFriends.append(friendName);
        }
        respdu = mkPDU(onlineFriends.size() * 32);
        respdu->uiType = ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_RESPOND;
        for (int i = 0; i < onlineFriends.size(); ++i) {
            QByteArray name = onlineFriends.at(i).toUtf8();
            memcpy(respdu->caMsg + i * 32, name.constData(), qMin(name.size(), 31));
        }
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
    {
        respdu=m_pmh->addFriend();
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE_REQUEST:
    {
        respdu=m_pmh->addfriendAgree();
        break;
    }
    case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST:
    {
        respdu=m_pmh->flushFriend();
        break;
    }
    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:
    {
        respdu=m_pmh->deleteFriend();
        break;
    }
    case ENUM_MSG_TYPE_CHAT_REQUEST:
    {
        respdu=m_pmh->chat();
        break;
    }
    case ENUM_MSG_TYPE_CREATE_FILE_REQUEST:
    {
        respdu=m_pmh->createFile();
        break;
    }
    case ENUM_MSG_TYPE_FLUSH_FILE_REQUEST:
    {
        respdu=m_pmh->flushFile();
        break;
    }
    case ENUM_MSG_TYPE_DEL_FILE_REQUEST:
    {
        respdu=m_pmh->delFile();
        break;
    }
    case ENUM_MSG_TYPE_RENAME_FILE_REQUEST:
    {
        respdu=m_pmh->renameFile();
        break;
    }
    case ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST:
    {
        respdu=m_pmh->uploadFileInit();
        break;
    }
    case ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST:
    {
        respdu=m_pmh->uploadFileData();
        break;
    }
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST:
    {
        respdu=m_pmh->downloadFile();
        break;
    }
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_REQUEST:
    {
        respdu=m_pmh->downloadFileData();
        break;
    }
    case ENUM_MSG_TYPE_SHARE_FILE_REQUEST:
    {
        respdu=m_pmh->shareFile();
        break;
    }
    case ENUM_MSG_TYPE_SHARE_FILE_RESPOND:
    {
        respdu=m_pmh->shareFileAgree();
        break;
    }
    case ENUM_MSG_TYPE_HEARTBEAT_REQUEST:
    {
        //收到心跳请求，更新活跃时间并响应
        updateActiveTime();
        PresenceStore::getInstance().heartbeat(m_strLoginName, m_sessionId);
        respdu = mkPDU();
        respdu->uiType = ENUM_MSG_TYPE_HEARTBEAT_RESPOND;
        qDebug()<<"收到用户"<<m_strLoginName<<"的心跳包";
        break;
    }
    default:
        break;
}
    return respdu;
}

MyTcpSocket::~MyTcpSocket()
{
    delete m_pmh;
}
void MyTcpSocket::recvMsg()
{
    qDebug()<<"recvMsg消息的总长度"<<this->bytesAvailable();

    QByteArray data=readAll();
    buffer.append(data);
    while(buffer.size()>=int(sizeof(PDU))){
        PDU*pdu=(PDU*)buffer.data();

        if(buffer.size()<int(sizeof(PDU))){
            break;
        }
        updateActiveTime();
        PDU*respdu=handleMsg(pdu);
        sendMsg(respdu);
        buffer.remove(0,pdu->uiTotalLen);

   }

}

void MyTcpSocket::clientOffline()
{
    MyTcpServer::getInstance().userOffline(this, 0);
    MyTcpServer::getInstance().removeSocket(this);
}

void MyTcpSocket::sendMsg(PDU *pdu)
{   if(pdu==NULL){
    return;
    }
    this->write((char*)pdu,pdu->uiTotalLen);
    qDebug()<<"send uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
}

void MyTcpSocket::updateActiveTime()
{
    m_lastActiveTime = QDateTime::currentDateTime();
}

bool MyTcpSocket::isTimeout()
{
    //超过90秒没有活动则认为超时
    qint64 seconds = m_lastActiveTime.secsTo(QDateTime::currentDateTime());
    return seconds > 90;
}

