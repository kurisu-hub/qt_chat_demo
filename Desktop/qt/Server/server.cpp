#include "mytcpserver.h"
#include "server.h"

#include <QFile>
#include <QDebug>
#include <QMessageBox>
#include <QDir>

Server::Server(QWidget *parent)
    : QWidget(parent)
{   //加载配置文件
    loadConfig();
    if (!QDir().mkpath(m_strRootPath)) qFatal("Cannot create server storage root");
    //调用监听函数listen
    if (!MyTcpServer::getInstance().listen(QHostAddress(m_strIP),m_usPort)) qFatal("Cannot listen on configured address");
}

Server::~Server()
{
}

void Server::loadConfig()
{
    QFile file(":/connect.config");
    if(file.open(QIODevice::ReadOnly)){
        QString str=QString(file.readAll());
       qDebug()<<"date"<<str;
       QStringList stringlist=str.split("\r\n");
       m_strIP=stringlist[0];
       m_usPort=stringlist[1].toUShort();
       m_strRootPath=stringlist[2];
       qDebug()<<"IP:"<<m_strIP<<"usPort:"<<m_usPort<<"m_strRootPath"<<m_strRootPath;
    }else{
       QMessageBox::information(this,"提示","未找到信息");
    }

}

Server &Server::getInstance()
{
    static Server instance;
    return instance;
}

