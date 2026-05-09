//头文件保护宏，防止文件被重复包含
#ifndef CLIENT_H
#define CLIENT_H
//包含QManinWindow类的定义，用于创建主窗口应用程序的类
#include "protocol.h"
#include "reshandler.h"

#include <QMainWindow>
//包含QTcpSocket
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class Client; }
QT_END_NAMESPACE

class Client : public QMainWindow
{   //信号槽
    Q_OBJECT

public:
    ~Client();
    //添加ip和端口号在Client中
    QString m_strIP;
    QString m_strPath;
    quint16 m_usPort;
    QString m_strLoginName;
    ResHandler* m_prh;
    QByteArray buffer;
    PDU* readMsg();
    void handleMsg(PDU*pdu);
    //使用静态的函数，引用防止被拷贝构造
    static Client& getInstance();
    //加载配置函数
    void loadConfig();
    //创建一个实列socket
    QTcpSocket socket;
    void sendMsg(PDU*pdu);
//声明一个槽函数，和connect函数用的，用于启动调用showConnect这个函数
public slots:
    void showConnect();
    void recvMsg();
//private slots:
//    void on_send_PB_clicked();

private slots:
    void on_regist_PB_clicked();
    void on_login_PB_clicked();

private:
    Ui::Client *ui;
    //将构造函数私有化，这是为了实现单例
    Client(QWidget *parent = nullptr);
    //禁用拷贝构造和拷贝构造运算符也是为了实现单例
    Client(const Client&Instance )=delete;
    Client&operator=(const Client&)=delete;

};

#endif // CLIENT_H
