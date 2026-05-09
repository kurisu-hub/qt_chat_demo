#include "server.h"
#include "operatedb.h"
#include <QApplication>

int main(int argc, char *argv[])
{   //一样的不多解释了
    QApplication a(argc, argv);
    OperateDB::getInstance().connect();
    //创造一个实例，同时会调用构造函数
    Server::getInstance();
    //w.show();
    return a.exec();
}
