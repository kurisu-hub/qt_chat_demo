#include "client.h"
#include "index.h"

#include <QApplication>
int main(int argc, char *argv[])
{   //创建QAPPlication来支撑Qt网络模块的事件处理，可以确保tcp服务监听，客户端连接
    QApplication a(argc, argv);
    //创建单例，调用show来展示窗口
    Client::getInstance().show();
    //这个函数是确保窗口一直存在的
    return a.exec();
}
