#include "operatedb.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
OperateDB::~OperateDB()
{
    m_db.close();
}

void OperateDB::connect()
{
    m_db.setHostName("localhost");
    m_db.setPort(3306);
    m_db.setUserName("root");
    m_db.setPassword("123456");
    m_db.setDatabaseName("mydb2503");
    if(m_db.open())
    {
        qDebug()<<"数据库连接成功";
    }
    else
    {
        qDebug()<<"连接失败"<<m_db.lastError().text();
    }

}

OperateDB &OperateDB::getInstance()
{
 static OperateDB instance;
 return instance;
}

OperateDB::OperateDB(QObject *parent) : QObject(parent)
{
    m_db=QSqlDatabase::addDatabase("QMYSQL");
}

bool OperateDB::handleRegist(char *caName, char *caPwd)
{   if (caName==NULL||caPwd==NULL)
    {
        return false;
    }

    QString sql=QString("select *from user_info where name='%1'").arg(caName);
    qDebug()<<"查询用户是否存在sql"<<sql;
    QSqlQuery q;
    if(!q.exec(sql))
    {
        return false;
    }
    if (q.next())
    {
        return false;
    }
    sql=QString("insert into user_info(name,pwd) values('%1','%2')").arg(caName).arg(caPwd);
    qDebug()<<"插入一条新用户 sql"<<sql;
    return q.exec(sql);

}

bool OperateDB::handleLogin(char *caName, char *caPwd)
{
    if (caName==NULL||caPwd==NULL)
       {
           return false;
       }
    QString sql=QString ("select*from user_info where name='%1' and pwd='%2'").arg(caName).arg(caPwd);
    QSqlQuery q;
    if(!q.exec(sql))
    {
        return false;
    }
    if(!q.next())
    {
        return false;
    }
    sql=QString("update user_info set online=1 where name='%1'").arg(caName);
    qDebug()<<"将onlin设置为1"<<sql;
    return q.exec(sql);
}

void OperateDB::handleOffline(const char *caName)
{   if(caName==NULL)
    {
        return;
    }
    QString sql=QString("update user_info set online=0 where name='%1'").arg(caName);
    QSqlQuery q;
    q.exec(sql);
}

int OperateDB::handleFindUser(const char *caName)
{
    if(caName==NULL)
    {
        return -1;
    }
    QString sql=QString("select online from user_info where name='%1'").arg(caName);
    qDebug()<<"查询字段online字段sql"<<sql;
    QSqlQuery q;
    q.exec(sql);
    if(q.next())
    {
        return q.value(0).toInt();
    }
    return 2;

}

QStringList OperateDB::handleOnlineUser()
{
    QString sql=QString("select name from user_info where online=1");
    QSqlQuery q;
    q.exec(sql);
    QStringList result;
    while(q.next())
    {
        result.append(q.value(0).toString());
    }
    return result;
}

int OperateDB::handleAddFriend(char *caCurName, char *caTarName)
{
    if(caCurName==NULL||caTarName==NULL){
        return -1;
    }
    QString sql=QString(R"(select *from friend where (
                        user_id=(select id from user_info where name='%1')
                        and
                        friend_id=(select id from user_info where name='%2')
                        or
                        friend_id=(select id from user_info where name='%1')
                        and
                        user_id=(select id from user_info where name='%2')
                                                );)").arg(caCurName).arg(caTarName);
    qDebug()<<"查询是否是好友";
    qDebug()<<"caTarName:"<<caTarName<<"caCurName:"<<caCurName;
    if(QString(caTarName)==QString(caCurName)){
        return -3;
    }
    QSqlQuery q;
    q.exec(sql);
    if(q.next()){
        return -2;
    }
    sql=QString("select online from user_info where name='%1'").arg(caTarName);
    q.exec(sql);
    if(q.next()){
        return q.value(0).toInt();
    }
    return -1;

}

bool OperateDB::handleAddFriendAgree(char *caCurName, char *caTarName)
{
 if(caCurName==NULL||caTarName==NULL){
     return false;
 }
 QString sql=QString(R"(
                     insert into friend(user_id,friend_id)
                     select u1.id,u2.id
                     from user_info u1,user_info u2
                     where u1.name='%1' and u2.name='%2';
                     )").arg(caCurName).arg(caTarName);
 QSqlQuery q;
 return q.exec(sql);
}

QStringList OperateDB::handleFlushFriend(char *caName)
{
    QStringList res;
    if(caName==NULL){
        return res;
    }
    //执行查询语句，这里只显示在线用户方便添加好友和聊天
    QString sql=QString (R"(
                         select name from user_info
                            where id in(
                               select user_id from friend where friend_id =
                               (select id from user_info where name='%1')
                               union
                               select friend_id from friend where user_id=
                               (select id from user_info where name='%1')
                                 )and online =1;
                         )").arg(caName);
    QSqlQuery q;
    q.exec(sql);
    //将每次运行结果添加到res里面
    while(q.next()){
        res.append(q.value(0).toString());
    }
    return res;
}

bool OperateDB::handleDeleteFriend(char *caCurName, char *caTarName)
{
    QString sql=QString (R"(delete from friend where (
                         user_id=(select id from user_info where name='%1')
                         and
                         friend_id=(select id from user_info where name='%2')
                         or
                         friend_id=(select id from user_info where name='%1')
                         and
                         user_id=(select id from user_info where name='%2')
                                                 );
                         )").arg(caCurName).arg(caTarName);
    QSqlQuery q;
    if(q.exec(sql)){
        return true;
    }
    else{
        return false;
    }

}

int OperateDB::isFriend(char *caCurName, char *caTarName)
{
    if(caCurName==NULL||caTarName==NULL){
        return -1;
    }
    qDebug()<<"查询是否是好友";
    QString sql=QString(R"(select *from friend where (
                        user_id=(select id from user_info where name='%1')
                        and
                        friend_id=(select id from user_info where name='%2')
                        or
                        friend_id=(select id from user_info where name='%1')
                        and
                        user_id=(select id from user_info where name='%2')
                                                );)").arg(caCurName).arg(caTarName);

    QSqlQuery q;
    q.exec(sql);
    if(q.next()){
        return 1;
    }
    else{
        return 0;
    }
}


