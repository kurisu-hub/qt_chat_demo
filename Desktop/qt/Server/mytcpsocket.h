#ifndef MYTCOSOCKET_H
#define MYTCOSOCKET_H

#include "msghandler.h"
#include "protocol.h"

#include <QObject>
#include <QTcpSocket>

class MyTcpSocket : public QTcpSocket
{
    Q_OBJECT
public:
    MyTcpSocket();
    //定义一个登录名
    QString m_strLoginName;
    QByteArray buffer;
    PDU*readMsg();
    MsgHandler*m_pmh;//新增成员变量
    void sendMsg(PDU*pdu);
    PDU*handleMsg(PDU*pdu);
    ~MyTcpSocket();
public slots:
    void recvMsg();
    void clientOffline();
};

#endif // MYTCOSOCKET_H
