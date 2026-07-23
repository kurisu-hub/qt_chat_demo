#include "mytcpsocket.h"
#include "protocol.h"
#include "operatedb.h"
MyTcpSocket::MyTcpSocket()
{

    connect(this,&QTcpSocket::readyRead,this,&MyTcpSocket::recvMsg);
    connect(this,&QTcpSocket::disconnected,this,&MyTcpSocket::clientOffline);
    m_pmh=new MsgHandler;
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
        PDU*respdu=handleMsg(pdu);
        sendMsg(respdu);
        buffer.remove(0,pdu->uiTotalLen);

   }

}

void MyTcpSocket::clientOffline()
{
    OperateDB::getInstance().handleOffline(m_strLoginName.toStdString().c_str());
}

void MyTcpSocket::sendMsg(PDU *pdu)
{   if(pdu==NULL){
    return;
    }
    this->write((char*)pdu,pdu->uiTotalLen);
    qDebug()<<"send uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
}

