#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include "mytcpsocket.h"

#include <QObject>
#include <QTcpServer>
#include <QTimer>

class MyTcpServer : public QTcpServer
{
    Q_OBJECT
public:
    //函数的重写
    void incomingConnection(qintptr handle) override;
    void removeSocket(MyTcpSocket*mysocket);
    static MyTcpServer& getInstance();
    void resend(char*turcaName,PDU*pdu);
    void notifyFriendsPresence(const QString &userName, bool online, quint8 reason);
    void userOffline(MyTcpSocket *socket, quint8 reason);

private slots:
    void checkHeartbeat();  //检查心跳超时

private:
    MyTcpServer();
    MyTcpServer(const MyTcpServer&instance)=delete;
    MyTcpServer&operator=(const MyTcpServer&)=delete;
    QList<MyTcpSocket*>m_tcpSocketList;
    QTimer* m_pHeartbeatCheckTimer;  //心跳检查定时器
};

#endif // MYTCPSERVER_H
