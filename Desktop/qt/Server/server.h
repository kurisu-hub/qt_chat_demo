#ifndef SERVER_H
#define SERVER_H

#include "mytcpserver.h"

#include <QWidget>
//继承父类
class Server : public QWidget
{
    Q_OBJECT

public:
    ~Server();
    void loadConfig();
    QString m_strIP;
    quint16 m_usPort;
    QString m_strRootPath;
    static Server&getInstance();
private:
    Server(QWidget *parent = nullptr);
    Server (const Server&Instance)=delete;
    Server&operator=(const Server&)=delete;
};
#endif // SERVER_H
