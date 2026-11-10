#include "client.h"
#include "index.h"
#include "protocol.h"
#include "ui_client.h"
#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QProcess>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <memory>

Client::Client(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Client)
{
    m_prh=new ResHandler();
    ui->setupUi(this);
    loadConfig();
    //调用函数来获取端口号和IP地址
    socket.setReadBufferSize(1024 * 1024);
    connect(&socket, &QTcpSocket::disconnected, this, [this] { buffer.clear(); });
    socket.connectToHost(QHostAddress(m_strIP),m_usPort);
    //用connect函数来进行信号槽连接，当socket函数发出信号时会调用showConnect函数
    connect(&socket,&QTcpSocket::connected,this,&Client::showConnect);
    connect(&socket,&QTcpSocket::readyRead,this,&Client::recvMsg);

    //初始化心跳定时器
    m_pHeartbeatTimer = new QTimer(this);
    connect(m_pHeartbeatTimer, &QTimer::timeout, this, &Client::sendHeartbeat);

    //点击验证码图片可刷新验证码
    ui->captcha_LB->installEventFilter(this);

    //启动后自动请求一张验证码
    connect(&socket, &QTcpSocket::connected, this, &Client::requestCaptcha);
}

Client::~Client()
{
    delete ui;
    delete m_prh;
}

namespace {
bool terminated(const char *text, uint size)
{
    return size && memchr(text, 0, size) != nullptr;
}

bool validResponse(const PDU *pdu)
{
    switch (pdu->uiType) {
    case ENUM_MSG_TYPE_ONLINE_USER_RESPOND:
    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND:
    case ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_RESPOND:
        if (pdu->uiMsgLen % 32) return false;
        for (uint i = 0; i < pdu->uiMsgLen; i += 32)
            if (!terminated(pdu->caMsg + i, 32)) return false;
        return true;
    case ENUM_MSG_TYPE_FLUSH_FILE_RESPOND:
        if (pdu->uiMsgLen % sizeof(FileInfo)) return false;
        for (uint i = 0; i < pdu->uiMsgLen; i += sizeof(FileInfo))
            if (!terminated(pdu->caMsg + i, 32)) return false;
        return true;
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
        return terminated(pdu->caData, 32) && terminated(pdu->caData + 32, 32);
    case ENUM_MSG_TYPE_FRIEND_ONLINE_NOTIFY:
    case ENUM_MSG_TYPE_FRIEND_OFFLINE_NOTIFY:
        return terminated(pdu->caData, 32);
    case ENUM_MSG_TYPE_SHARE_FILE_REQUEST:
        return terminated(pdu->caData, 32) && terminated(pdu->caMsg, pdu->uiMsgLen);
    case ENUM_MSG_TYPE_CHAT_REQUEST:
    case ENUM_MSG_TYPE_CHAT_RESPOND:
        // The failure response contains only an integer zero, no text body.
        if (pdu->uiMsgLen == 0) {
            int result = -1;
            memcpy(&result, pdu->caData, sizeof(result));
            return result == 0;
        }
        return terminated(pdu->caData, 32) && terminated(pdu->caMsg, pdu->uiMsgLen);
    default:
        return true; // Other payloads are numeric flags or binary data.
    }
}
}

void Client::handleMsg(PDU *pdu)
{
    if (!validResponse(pdu)) {
        qWarning() << "Invalid response payload";
        buffer.clear();
        socket.abort();
        return;
    }
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
    case ENUM_MSG_TYPE_CAPTCHA_RESPOND:
    {
        m_prh->captcha();
        break;
    }
    case ENUM_MSG_TYPE_LOGIN_WITH_CAPTCHA_RESPOND:
    {
        m_prh->loginWithCaptcha();
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
    case ENUM_MSG_TYPE_CHAT_RESPOND:
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
        if (pdu->caData[0]) Index::getInstance().getFile()->flushFile();
        else {
            Index::getInstance().getFile()->cancelUpload();
            QMessageBox::information(&Index::getInstance(), "提示", "上传失败，文件未保存");
        }
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
    case ENUM_MSG_TYPE_HEARTBEAT_RESPOND:
    {
        qDebug()<<"收到服务器心跳响应";
        break;
    }
    case ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_RESPOND:
    {
        m_prh->friendPresenceSnapshot();
        break;
    }
    case ENUM_MSG_TYPE_FRIEND_ONLINE_NOTIFY:
    case ENUM_MSG_TYPE_FRIEND_OFFLINE_NOTIFY:
    {
        m_prh->friendPresenceNotify();
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
    free(pdu);
    pdu=NULL;

}

void Client::showConnect()
{
    qDebug()<<"连接服务器成功";
}

void Client::recvMsg()
{
    // Nested event loops may emit readyRead while a handler owns m_prh->pdu.
    // Leave queued bytes in the socket until the outer dispatch resumes.
    if (m_dispatching) return;
    QScopedValueRollback<bool> guard(m_dispatching, true);
    const uint maxFrame = 1024 * 1024;
    for (;;) {
        if (buffer.size() < int(sizeof(PDU)))
            buffer.append(socket.read(int(sizeof(PDU)) - buffer.size()));
        if (buffer.size() < int(sizeof(PDU))) return;
        uint total = 0, message = 0;
        memcpy(&total, buffer.constData(), sizeof(total));
        memcpy(&message, buffer.constData() + sizeof(uint), sizeof(message));
        if (total < sizeof(PDU) || total > maxFrame || message != total - sizeof(PDU)) {
            buffer.clear();
            socket.abort();
            qWarning() << "Invalid protocol frame";
            return;
        }
        if (buffer.size() < int(total))
            buffer.append(socket.read(int(total) - buffer.size()));
        if (buffer.size() < int(total)) return;
        std::unique_ptr<PDU, decltype(&free)> frame(mkPDU(message), &free);
        memcpy(frame.get(), buffer.constData(), total);
        buffer.remove(0, int(total));
        handleMsg(frame.get());
        m_prh->pdu = nullptr;
    }
}

void Client::on_regist_PB_clicked()
{
    QString strName=ui->name_LE->text();
    QString strPwd=ui->pwd_LE->text();
    if(!validCredential(strName) || !validCredential(strPwd))
    {
        QMessageBox::information(this,"提示","用户名或密码非法");
        return;
    }
    PDU *pdu=mkPDU();
    copyTextField(pdu->caData, strName);
    m_strLoginName=strName;
    copyTextField(pdu->caData+32, strPwd);
    pdu->uiType=ENUM_MSG_TYPE_REGIST_REQUEST;
    socket.write((char*)pdu,pdu->uiTotalLen);
    free(pdu);
    pdu=NULL;

}




void Client::on_login_PB_clicked()
{
    QString strName=ui->name_LE->text();
    QString strPwd=ui->pwd_LE->text();
    if (!validCredential(strName) || !validCredential(strPwd)) {
        QMessageBox::information(this,"提示","用户名或密码非法");
        return;
    }
    QString strCaptcha=ui->captcha_LE->text();
    if(strCaptcha.isEmpty())
    {
        QMessageBox::information(this,"提示","请输入验证码");
        return;
    }
    this->m_strLoginName=strName;
    PDU*pdu=mkPDU(strCaptcha.toStdString().size()+1);
    copyTextField(pdu->caData, strName);
    copyTextField(pdu->caData+32, strPwd);
    memcpy(pdu->caMsg,strCaptcha.toStdString().c_str(),strCaptcha.toStdString().size()+1);
    pdu->uiType=ENUM_MSG_TYPE_LOGIN_WITH_CAPTCHA_REQUEST;
    socket.write((char*)pdu,pdu->uiTotalLen);
    free(pdu);
    pdu=NULL;
}

void Client::requestCaptcha()
{
    PDU*pdu=mkPDU();
    pdu->uiType=ENUM_MSG_TYPE_CAPTCHA_REQUEST;
    sendMsg(pdu);
}

void Client::setCaptchaImage(const QPixmap &pixmap)
{
    ui->captcha_LB->setPixmap(pixmap);
    //清空输入框，要求用户输入新验证码
    ui->captcha_LE->clear();
}

void Client::clearCaptchaInput()
{
    ui->captcha_LE->clear();
}

bool Client::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->captcha_LB && event->type() == QEvent::MouseButtonRelease)
    {
        //点击验证码图片刷新验证码
        requestCaptcha();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void Client::startHeartbeat()
{
    //每30秒发送一次心跳
    m_pHeartbeatTimer->start(30000);
    qDebug()<<"心跳定时器已启动";
}

void Client::sendHeartbeat()
{
    PDU* pdu = mkPDU();
    pdu->uiType = ENUM_MSG_TYPE_HEARTBEAT_REQUEST;
    copyTextField(pdu->caData, m_strLoginName);
    sendMsg(pdu);
    qDebug()<<"发送心跳包";
}

void Client::requestFriendPresenceSnapshot()
{
    PDU *pdu = mkPDU();
    pdu->uiType = ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_REQUEST;
    sendMsg(pdu);
}
