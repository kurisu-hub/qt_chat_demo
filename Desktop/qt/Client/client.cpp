#include "client.h"
#include "index.h"
#include "protocol.h"
#include "ui_client.h"
#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QProcess>

Client::Client(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Client)
{
    m_prh=new ResHandler();
    ui->setupUi(this);
    loadConfig();
    //调用函数来获取端口号和IP地址
    socket.connectToHost(QHostAddress(m_strIP),m_usPort);
    //用connect函数来进行信号槽连接，当socket函数发出信号时会调用showConnect函数
    connect(&socket,&QTcpSocket::connected,this,&Client::showConnect);
    connect(&socket,&QTcpSocket::readyRead,this,&Client::recvMsg);
}

Client::~Client()
{
    delete ui;
    delete m_prh;
}

PDU *Client::readMsg()
{
    qDebug()<<"socket中的总长度为:"<<socket.bytesAvailable();
    uint uiPDULen=0;
    socket.read((char*)&uiPDULen,sizeof(uint));
    uint uiMsgLen=uiPDULen-sizeof(PDU);
    PDU*pdu=mkPDU(uiMsgLen);
    socket.read((char*)pdu+sizeof(uint),uiPDULen-sizeof(uint));
    qDebug()<<"readMsg uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
    return pdu;
}

void Client::handleMsg(PDU *pdu)
{
    qDebug()<<"readMsg uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
    m_prh->pdu=pdu;
    switch(pdu->uiType)
    {
    case ENUM_MSG_TYPE_REGIST_RESPOND:
    {
       m_prh->regist();
       break;
    }
    case ENUM_MSG_TYPE_LOGIN_RESPOND:
    {
        m_prh->login();
        break;

    }
    case ENUM_MSG_TYPE_FIND_USER_RESPOND:
    {
        m_prh->findUser();
        break;
    }
    case ENUM_MSG_TYPE_ONLINE_USER_RESPOND:
    {
        m_prh->onlineUser();
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:{
        m_prh->addFriendResend();
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_RESPOND:{
        m_prh->addFriend();
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE_RESPOND:{
        m_prh->addFriendAgree();
        break;
    }
    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND:{
        m_prh->flushFriend();
        break;
    }
    case ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND:{
        m_prh->deleteFriend();
        break;
    }
    case ENUM_MSG_TYPE_CHAT_REQUEST:{
        m_prh->chat();
        break;
    }
    case ENUM_MSG_TYPE_CREATE_FILE_RESPOND:{
        m_prh->createFile();
        break;
    }
    case ENUM_MSG_TYPE_FLUSH_FILE_RESPOND:{
        m_prh->flushFile();
        break;
    }
    case ENUM_MSG_TYPE_DEL_FILE_RESPOND:{
        m_prh->delFile();
        break;
    }
    case ENUM_MSG_TYPE_RENAME_FILE_RESPOND:{
        m_prh->renameFile();
        break;
    }
    case ENUM_MSG_TYPE_UPLOAD_FILE_INIT_RESPOND:{
        m_prh->uploadFileInit();
        break;
    }
    case ENUM_MSG_TYPE_UPLOAD_FILE_DATA_RESPOND:{
        Index::getInstance().getFile()->flushFile();
        break;
    }
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND:
    {
        m_prh->downfile();
        break;
    }


    case ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_RESPOND:
    {
        m_prh->downloadFileData();
        break;
    }
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_FINISH_RESPOND:
    {
        m_prh->downloadFileFinish();
        break;
    }
    case ENUM_MSG_TYPE_SHARE_FILE_REQUEST:
    {
        m_prh->shareFileResend();
        break;
    }
    case ENUM_MSG_TYPE_SHARE_FILE_RESPOND:
    {
        m_prh->shareFileAgree();
        break;
    }
     default:
        break;
    }

}

Client &Client::getInstance()
{   //创造静态的实例，实现单例模式
    static Client Instance;
    return Instance;
}

void Client:: loadConfig(){
    //定义文件，括号里面是文件的路径
    QFile file(":/connect.config");
    //只读方式打开
    if(file.open(QIODevice::ReadOnly)){
        //创造一个QSrting类型的变量，用于读取，其中读取的类型不是QString类型所以要转
        QString str=QString(file.readAll());
       qDebug()<<"date"<<str;
       //用split来分割，用列表来储存
       QStringList stringlist=str.split("\r\n");
       m_strIP=stringlist[0];
       m_usPort=stringlist[1].toUShort();
       m_strPath=stringlist[2];
       qDebug()<<"IP:"<<m_strIP<<"usPort:"<<m_usPort<<"m_strUserPath";
    }else{
       QMessageBox::information(this,"提示","未找到信息");
    }

}

void Client::sendMsg(PDU *pdu)
{
    //发送给服务器pdu
    socket.write((char*)pdu,pdu->uiTotalLen);
    qDebug()<<"send uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
    free(pdu);
    pdu=NULL;

}

void Client::showConnect()
{
    qDebug()<<"连接服务器成功";
}

void Client::recvMsg()
{
    //接受pdu同时注意防止沾包和半包的情况，所以用data接受，读取pdu的大小然后进行处理，这样可以用于应对多个处理请求
    qDebug()<<"recvMsg消息的总长度"<<socket.bytesAvailable();
    QByteArray data=socket.readAll();
    buffer.append(data);
    while(buffer.size()>=int(sizeof(PDU))){
        PDU*pdu=(PDU*)buffer.data();
        if(buffer.size()<int(sizeof(PDU))){
            break;
        }
        handleMsg(pdu);
        //处理完了就把这一部分删除掉就行了
        buffer.remove(0,pdu->uiTotalLen);

   }

}

//void Client::on_send_PB_clicked()
//{   //通过ui来调用input_LE函数获取内容用strMsg来记录
//    QString strMsg=ui->input_LE->text();
//    qDebug()<<strMsg;
//    //用pdu的指针来记录创造的数据结构体
//    PDU *pdu=mkPDU(strMsg.toStdString().size()+1);
//    //用memcpy来复制这些具体的数据
//    memcpy(pdu->caMsg,strMsg.toStdString().c_str(),strMsg.toStdString().size());
//    //pdu的类型赋值
//    pdu->uiType=ENUM_MSG_TYPE_REGIST_REQUEST;
//    //用socket的write函数发送pdu的数据
//    socket.write((char*)pdu,pdu->uiTotalLen);
//    qDebug()<<"uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
//    free(pdu);
//    pdu=NULL;


//}

void Client::on_regist_PB_clicked()
{
    QString strName=ui->name_LE->text();
    QString strPwd=ui->pwd_LE->text();
    if(strName.isEmpty()||strPwd.isEmpty()||strName.toStdString().size()>32||strPwd.toStdString().size()>32)
    {
        QMessageBox::information(this,"提示","用户名或密码非法");
        return;
    }
    PDU *pdu=mkPDU();
    memcpy(pdu->caData,strName.toStdString().c_str(),32);
    m_strLoginName=strName;
    memcpy(pdu->caData+32,strPwd.toStdString().c_str(),32);
    pdu->uiType=ENUM_MSG_TYPE_REGIST_REQUEST;
    socket.write((char*)pdu,pdu->uiTotalLen);
    qDebug()<<"send uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
    free(pdu);
    pdu=NULL;

}




void Client::on_login_PB_clicked()
{
    QString strName=ui->name_LE->text();
    QString strPwd=ui->pwd_LE->text();
    this->m_strLoginName=strName;
    PDU*pdu=mkPDU();
    memcpy(pdu->caData,strName.toStdString().c_str(),32);
    memcpy(pdu->caData+32,strPwd.toStdString().c_str(),32);
    pdu->uiType=ENUM_MSG_TYPE_LOGIN_REQUEST;
    socket.write((char*)pdu,pdu->uiTotalLen);
    qDebug()<<"send login uiTotalLen:"<<pdu->uiTotalLen<<"uiMsgLen"<<pdu->uiMsgLen<<"caData"<<pdu->caData<<"uiType"<<pdu->uiType<<"caMsg"<<pdu->caMsg;
    free(pdu);
    pdu=NULL;
}
