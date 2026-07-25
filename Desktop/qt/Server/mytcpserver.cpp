#include "mytcpserver.h"
#include "mytcpsocket.h"
#include "operatedb.h"
#include "presencestore.h"

MyTcpServer::MyTcpServer()
{
    m_pHeartbeatCheckTimer = new QTimer(this);
    connect(m_pHeartbeatCheckTimer, &QTimer::timeout,
            this, &MyTcpServer::checkHeartbeat);
    m_pHeartbeatCheckTimer->start(5000);
}

void MyTcpServer::incomingConnection(qintptr handle)
{
    //重写函数，其中hadle参数是代表一个连接的客户的标志
    qDebug()<<"新客户端连接";
    //创造MyTcpSocket的指针，用于接收handle标志
    MyTcpSocket*pTSocket=new MyTcpSocket;
    pTSocket->setSocketDescriptor(handle);
    //用列表来存储这个地址
    m_tcpSocketList.append(pTSocket);
    for(int i=0;i<m_tcpSocketList.size();i++)
    {
     qDebug()<<m_tcpSocketList[i];
    }
}

void MyTcpServer::removeSocket(MyTcpSocket *mysocket)
{
    m_tcpSocketList.removeOne(mysocket);
    mysocket->deleteLater();
    mysocket=NULL;
    for(int i=0;i<m_tcpSocketList.size();i++)
    {
        qDebug()<<m_tcpSocketList[i]->m_strLoginName;
    }
}

MyTcpServer &MyTcpServer::getInstance()
{   //创造单例
    static MyTcpServer instance;
    return instance;
}

void MyTcpServer::resend(char *turcaName, PDU *pdu)
{
    if(turcaName==nullptr||pdu==nullptr){
        return;
    }
    for(int i=0;i<m_tcpSocketList.size();i++)
    {
        if(turcaName==m_tcpSocketList[i]->m_strLoginName){
            m_tcpSocketList[i]->write((char*)pdu,pdu->uiTotalLen);
            qDebug()<<"resend uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;

        }
    }
}

void MyTcpServer::notifyFriendsPresence(const QString &userName, bool online, quint8 reason)
{
    QByteArray owner = userName.toUtf8();
    char ownerName[32] = {0};
    memcpy(ownerName, owner.constData(), qMin(owner.size(), 31));
    const QStringList friends = OperateDB::getInstance().handleFlushFriend(ownerName);

    PDU *pdu = mkPDU();
    pdu->uiType = online ? ENUM_MSG_TYPE_FRIEND_ONLINE_NOTIFY
                          : ENUM_MSG_TYPE_FRIEND_OFFLINE_NOTIFY;
    memcpy(pdu->caData, ownerName, 32);
    pdu->caData[32] = online ? 1 : 0;
    pdu->caData[33] = static_cast<char>(reason);
    for (const QString &friendName : friends) {
        if (!PresenceStore::getInstance().isOnline(friendName)) continue;
        QByteArray target = friendName.toUtf8();
        char targetName[32] = {0};
        memcpy(targetName, target.constData(), qMin(target.size(), 31));
        resend(targetName, pdu);
    }
    free(pdu);
}

void MyTcpServer::userOffline(MyTcpSocket *socket, quint8 reason)
{
    if (socket == nullptr || socket->m_offlineHandled || socket->m_strLoginName.isEmpty()) return;
    socket->m_offlineHandled = true;
    if (PresenceStore::getInstance().logout(socket->m_strLoginName, socket->m_sessionId)) {
        notifyFriendsPresence(socket->m_strLoginName, false, reason);
    }
}

void MyTcpServer::checkHeartbeat()
{
    const QList<MyTcpSocket *> sockets = m_tcpSocketList;
    for (MyTcpSocket *socket : sockets) {
        if (socket && socket->m_authenticated && socket->isTimeout()) {
            userOffline(socket, 1);
            socket->disconnectFromHost();
        }
    }
}
