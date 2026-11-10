#include "mytcpserver.h"
#include "operatedb.h"
#include "presencestore.h"
#include "security.h"
#include <QPointer>

MyTcpServer::MyTcpServer()
{
    m_pHeartbeatCheckTimer = new QTimer(this);
    connect(m_pHeartbeatCheckTimer, &QTimer::timeout, this, &MyTcpServer::checkHeartbeat);
    m_pHeartbeatCheckTimer->start(5000);
}
void MyTcpServer::incomingConnection(qintptr handle)
{
    MyTcpSocket *socket = new MyTcpSocket;
    socket->setParent(this);
    if (!socket->setSocketDescriptor(handle)) { socket->deleteLater(); return; }
    m_tcpSocketList.append(socket);
}
void MyTcpServer::removeSocket(MyTcpSocket *socket)
{
    m_tcpSocketList.removeOne(socket);
    socket->deleteLater();
}
MyTcpServer &MyTcpServer::getInstance() { static MyTcpServer instance; return instance; }
void MyTcpServer::resend(char *target, PDU *pdu)
{
    if (!target || !pdu) return;
    const QString user = QString::fromUtf8(target);
    const QList<MyTcpSocket *> sockets = m_tcpSocketList;
    for (MyTcpSocket *socket : sockets) {
        if (socket->m_authenticated && !socket->m_offlineHandled && socket->m_strLoginName == user
                && socket->state() == QAbstractSocket::ConnectedState) socket->sendMsg(pdu);
    }
}
void MyTcpServer::notifyFriendsPresence(const QString &user, bool online, quint8 reason)
{
    const QStringList friends = OperateDB::getInstance().handleFlushFriend(user.toUtf8().constData());
    PDU *pdu = mkPDU();
    pdu->uiType = online ? ENUM_MSG_TYPE_FRIEND_ONLINE_NOTIFY : ENUM_MSG_TYPE_FRIEND_OFFLINE_NOTIFY;
    Security::putField(pdu->caData, user);
    pdu->caData[32] = online ? 1 : 0; pdu->caData[33] = static_cast<char>(reason);
    for (const QString &name : friends) {
        if (!PresenceStore::getInstance().isOnline(name)) continue;
        QByteArray target = name.toUtf8(); resend(target.data(), pdu);
    }
    free(pdu);
}
void MyTcpServer::userOffline(MyTcpSocket *socket, quint8 reason)
{
    if (!socket || socket->m_offlineHandled || socket->m_strLoginName.isEmpty()) return;
    socket->m_offlineHandled = true;
    if (PresenceStore::getInstance().logout(socket->m_strLoginName, socket->m_sessionId))
        notifyFriendsPresence(socket->m_strLoginName, false, reason);
}
void MyTcpServer::checkHeartbeat()
{
    const QList<MyTcpSocket *> sockets = m_tcpSocketList;
    for (MyTcpSocket *socket : sockets) {
        socket->m_pmh->expireUpload();
        if (socket->isTimeout()) { userOffline(socket, 1); socket->abort(); }
    }
}
