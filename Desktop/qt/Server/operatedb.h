#ifndef OPERATEDB_H
#define OPERATEDB_H

#include <QObject>
#include <QSqlDatabase>

class OperateDB : public QObject
{
    Q_OBJECT
public:
    QSqlDatabase m_db;
    ~OperateDB();
    //数据库的连接函数
    void connect();
    //单例
    static OperateDB&getInstance();
    //创造处理注册功能的函数
    bool handleRegist(char*caName,char*caPwd);
    //创造处理登录功能的函数
    bool handleLogin(char*caName,char*caPwd);
    //创造处理设置在线的函数，用const是为了在tss.cs这么转的时候这个成员变量不是变量需要用const来做参数
    void handleOffline(const char*caName);
    int handleFindUser(const char*caName);
    QStringList handleOnlineUser();
    int handleAddFriend(char*caCurName,char*caTarName);
    bool handleAddFriendAgree(char*caCurName,char*caTarName);
    QStringList handleFlushFriend(char*caName);
    bool handleDeleteFriend(char*caCurName,char*caTarName);
    int isFriend(char*caCurName,char*caTarName);
private:
    explicit OperateDB(QObject *parent = nullptr);
    OperateDB(const OperateDB&instance)=delete;
    OperateDB&operator=(const OperateDB)=delete;
signals:

};

#endif // OPERATEDB_H
