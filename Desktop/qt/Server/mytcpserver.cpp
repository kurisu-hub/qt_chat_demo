#include "mytcpserver.h"
#include "mytcpsocket.h"

MyTcpServer::MyTcpServer()
{

}

void MyTcpServer::incomingConnection(qintptr handle)
{
    //重写函数，其中hadle参数是代表一个连接的客户的标志
    qDebug()<<"新客户端连接";
    //创造MyTcpSocket的指针，用于接收handle标志
    MyTcpSocket*pTSocket=new MyTcpSocket;
    pTSocket->setSocketDescriptor(handle);
    //用列表来存储这个地址
    m_tcpSocketList.append(pTSocket);
    for(int i=0;i<m_tcpSocketList.size();i++)
    {
     qDebug()<<m_tcpSocketList[i];
    }
}

void MyTcpServer::removeSocket(MyTcpSocket *mysocket)
{
    m_tcpSocketList.removeOne(mysocket);
    mysocket->deleteLater();
    mysocket=NULL;
    for(int i=0;i<m_tcpSocketList.size();i++)
    {
        qDebug()<<m_tcpSocketList[i]->m_strLoginName;
    }
}

MyTcpServer &MyTcpServer::getInstance()
{   //创造单例
    static MyTcpServer instance;
    return instance;
}

void MyTcpServer::resend(char *turcaName, PDU *pdu)
{
    if(turcaName==nullptr||pdu==nullptr){
        return;
    }
    for(int i=0;i<m_tcpSocketList.size();i++)
    {
        if(turcaName==m_tcpSocketList[i]->m_strLoginName){
            m_tcpSocketList[i]->write((char*)pdu,pdu->uiTotalLen);
            qDebug()<<"resend uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;

        }
    }
}
