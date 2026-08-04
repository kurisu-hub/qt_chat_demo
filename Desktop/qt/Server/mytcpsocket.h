#ifndef MYTCOSOCKET_H
#define MYTCOSOCKET_H

#include "msghandler.h"
#include "protocol.h"

#include <QObject>
#include <QTcpSocket>
#include <QDateTime>

class MyTcpSocket : public QTcpSocket
{
    Q_OBJECT
public:
    MyTcpSocket();
    //定义一个登录名
    QString m_strLoginName;
    QString m_sessionId;
    bool m_authenticated = false;
    bool m_offlineHandled = false;
    QByteArray buffer;
    PDU*readMsg();
    MsgHandler*m_pmh;//新增成员变量
    void sendMsg(PDU*pdu);
    PDU*handleMsg(PDU*pdu);
    ~MyTcpSocket();

    //心跳检测相关
    QDateTime m_lastActiveTime;  //最后活跃时间
    void updateActiveTime();     //更新活跃时间
    bool isTimeout();            //检查是否超时

    //图形验证码相关
    QString m_captchaText;            //当前连接的验证码文本
    void generateCaptcha();           //生成新验证码文本
    bool verifyCaptcha(const QString &input);  //校验验证码，正确则失效

public slots:
    void recvMsg();
    void clientOffline();
};

#endif // MYTCOSOCKET_H
