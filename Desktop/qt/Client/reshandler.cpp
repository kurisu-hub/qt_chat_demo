#include "client.h"
#include "file.h"
#include "index.h"
#include "reshandler.h"
#include "string.h"
#include <QMessageBox>
#include <QBuffer>
#include <QPixmap>



void ResHandler::regist()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(bool));
    if(ret)
    {
         QMessageBox::information(&Client::getInstance(),"提示","注册成功");
    }
    else
    {
        QMessageBox::information(&Client::getInstance(),"提示","注册失败");

    }
}

void ResHandler::login()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(bool));
    if (ret)
    {
        Index::getInstance().show();
        //隐藏登录界面
        Client::getInstance().hide();
        //登录成功后启动心跳检测
        Client::getInstance().startHeartbeat();
        Client::getInstance().requestFriendPresenceSnapshot();
    }
    else
    {
         QMessageBox::information(&Client::getInstance(),"提示","登录失败");
    }
}


void ResHandler::captcha()
{
    //把服务器返回的 PNG 数据解码成图片显示到验证码标签上
    QByteArray imgData(pdu->caMsg, pdu->uiMsgLen);
    QPixmap pixmap;
    pixmap.loadFromData(imgData, "PNG");
    Client::getInstance().setCaptchaImage(pixmap);
}

void ResHandler::loginWithCaptcha()
{
    int ret;
    memcpy(&ret,pdu->caData,sizeof(int));
    if (ret == 1)
    {
        Index::getInstance().show();
        //隐藏登录界面
        Client::getInstance().hide();
        //登录成功后启动心跳检测
        Client::getInstance().startHeartbeat();
        Client::getInstance().requestFriendPresenceSnapshot();
    }
    else
    {
        //验证码一次性失效，密码错误时同样需要刷新验证码
        if (ret == -1)
        {
            QMessageBox::information(&Client::getInstance(),"提示","验证码错误");
        }
        else
        {
            QMessageBox::information(&Client::getInstance(),"提示","用户名或密码错误");
        }
        Client::getInstance().requestCaptcha();
    }
}

void ResHandler::friendPresenceSnapshot()
{
    QStringList onlineFriends;
    const int count = pdu->uiMsgLen / 32;
    for (int i = 0; i < count; ++i) {
        char name[32] = {0};
        memcpy(name, pdu->caMsg + i * 32, 32);
        onlineFriends.append(QString::fromUtf8(name));
    }
    Index::getInstance().getFriend()->applyPresenceSnapshot(onlineFriends);
}

void ResHandler::friendPresenceNotify()
{
    char name[32] = {0};
    memcpy(name, pdu->caData, 32);
    const bool online = pdu->uiType == ENUM_MSG_TYPE_FRIEND_ONLINE_NOTIFY;
    Index::getInstance().getFriend()->updateFriendPresence(QString::fromUtf8(name), online);
}

void ResHandler::findUser()
{
    int ret;
    memcpy(&ret,pdu->caData,sizeof(int));
    if(ret==2)
    {
        QMessageBox::information(&Client::getInstance(),"提示","该用户不存在");
    }
    else if(ret==1)
    {
        QMessageBox::information(&Client::getInstance(),"提示","该用户在线");
    }
    else if(ret==0)
    {
        QMessageBox::information(&Client::getInstance(),"提示","该用户不在线");
    }
}

void ResHandler::onlineUser()
{
    int iSize=pdu->uiMsgLen/32;
    QStringList nameList;
    char caTmp[32]={'\0'};
    for(int i=0;i<iSize;i++)
    {
        memcpy(caTmp,pdu->caMsg+i*32,32);
        qDebug()<<"caTmp"<<caTmp;
        nameList.append(caTmp);
    }
    Index::getInstance().getFriend()->m_pOnlineUser->updateOnlineUser(nameList);
}

void ResHandler::addFriend()
{   int ret;

    memcpy(&ret,pdu->caData,sizeof(int));
    qDebug()<<"addFriend的ret"<<ret;
    if(ret==-1)
    {
        QMessageBox::information(&Client::getInstance(),"提示","该用户不在线");
    }
    else if(ret==-2){
        QMessageBox::information(&Client::getInstance(),"提示","该用户已是好友");
    }
    else if(ret==-3){
        QMessageBox::information(&Client::getInstance(),"提示","不可添加自己为好友");
    }
    else if(ret==1){
        QMessageBox::information(&Client::getInstance(),"提示","添加好友成功");
    }
}

void ResHandler::addFriendResend()
{
    char caName[32]={'\0'};
    memcpy(caName,pdu->caData,32);
    int ret=QMessageBox::question(&Index::getInstance(),"添加好友",QString("是否同意'%1'的好友添加请求").arg(caName));
    if(ret!=QMessageBox::Yes){
        return;
    }
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,pdu->caData,64);
    respdu->uiType=ENUM_MSG_TYPE_ADD_FRIEND_AGREE_REQUEST;
    Client::getInstance().sendMsg(respdu);
}

void ResHandler::addFriendAgree()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(bool));
    if(ret){
        Index::getInstance().getFriend()->flushfriend();
    }
    else{
        QMessageBox::information(&Index::getInstance(),"提示","添加好友失败");
    }
}

void ResHandler::flushFriend()
{
    QStringList nameList;
    //这里要获取总共有多少个用户名，为总字节数除以32，然后添加到QStringList里面，最后传参到刷新函数
    int iSize=pdu->uiMsgLen/32;
    char caTmp[32]={'\0'};
    for(int i=0;i<iSize;i++){
        memcpy(caTmp,pdu->caMsg+32*i,32);
        nameList.append(caTmp);
    }
    //调用刷新函数进行刷新就行了
    Index::getInstance().getFriend()->flushFriend_LW(nameList);

}

void ResHandler::deleteFriend()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(ret));
    qDebug()<<"bool ret为:"<<ret;
    if(ret){
        Index::getInstance().getFriend()->flushfriend();
    }
    else{
        QMessageBox::information(&Index::getInstance(),"提示","删除好友失败");
    }

}

void ResHandler::chat()
{
    int ret;
    memcpy(&ret,pdu->caData,sizeof(int));
    qDebug()<<"登录名"<<Client::getInstance().m_strLoginName<<"ret"<<ret;
    qDebug() << "ret的数值：" << static_cast<int>(ret);
    if(ret==0){
        qDebug()<<"登录名"<<Client::getInstance().m_strLoginName;
        QMessageBox::information(&Index::getInstance(),"提示","对方不是你好友，请添加好友再聊天");
        Index::getInstance().getFriend()->m_pChat->isHidden();
        flushFriend();
        return;
    }
    char caChatName[32]={'\0'};
    memcpy(caChatName,pdu->caData,32);
    Chat* c=Index::getInstance().getFriend()->m_pChat;
    c->appendAlignedColoredText(c->getshow_LE(),QString("%1:%2").arg(caChatName).arg(pdu->caMsg),Qt::AlignLeft,Qt::black);
    //c->updateShow_TE(QString("%1:%2").arg(caChatName).arg(pdu->caMsg));
    if(c->isHidden()){
        c->show();
    }
    c->m_strChatName=caChatName;
}

void ResHandler::createFile()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(ret));
    if(ret){
        QMessageBox::information(&Index::getInstance(),"提示","文件创造成功");
        Index::getInstance().getFile()->flushFile();
    }
    else{
        QMessageBox::information(&Index::getInstance(),"提示","文件创造失败");
    }
}

void ResHandler::flushFile()
{
    int iCount=pdu->uiMsgLen/sizeof(FileInfo);
    QList<FileInfo*>pFileList;
    for(int i=0;i<iCount;i++){
        FileInfo *pFileInfo=new FileInfo;
        memcpy(pFileInfo,pdu->caMsg+i*sizeof(FileInfo),sizeof(FileInfo));
        qDebug()<<"flushFile pFileInfo->caName"<<pFileInfo->caName;
        pFileList.append(pFileInfo);
    }
    Index::getInstance().getFile()->updateFileList(pFileList);
}

void ResHandler::delFile()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(bool));
    if(ret==true){
       Index::getInstance().getFile()->flushFile();
    }
    else{
        QMessageBox::information(&Index::getInstance(),"提示","删除文件失败");
    }
}

void ResHandler::renameFile()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(bool));
    if(ret){
        Index::getInstance().getFile()->flushFile();
    }
    else{
        QMessageBox::information(&Index::getInstance(),"提示","文件命名失败");
    }
}

void ResHandler::uploadFileInit()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(bool));
    qDebug()<<"ret"<<ret;
    if(ret){
        Index::getInstance().getFile()->UploadFile();

    }
    else{
        QMessageBox::information(&Index::getInstance(),"提示","文件上传失败");
    }
}

ResHandler::ResHandler()
{

}

void ResHandler::downfile()
{
    bool ret;

     memcpy(&ret,
            pdu->caData,
            sizeof(bool));


     if(ret)
     {
         QMessageBox::information(
                     &Index::getInstance(),
                     "提示",
                     "开始下载");


         Index::getInstance()
                 .getFile()
                 ->m_fDownloadfile.open(QIODevice::WriteOnly);


         //请求第一块数据

         PDU*pdu=mkPDU();


         pdu->uiType=
         ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_REQUEST;


         Client::getInstance().sendMsg(pdu);

     }
     else
     {
         QMessageBox::information(
                     &Index::getInstance(),
                     "提示",
                     "下载失败");
     }

}

void ResHandler::downloadFileData()
{
    Index::getInstance()
               .getFile()
               ->m_fDownloadfile.write(
                   pdu->caMsg,
                   pdu->uiMsgLen
               );


       //继续请求下一块


       PDU*pdu=mkPDU();


       pdu->uiType=
       ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_REQUEST;


       Client::getInstance().sendMsg(pdu);

}

void ResHandler::downloadFileFinish()
{

    Index::getInstance()
            .getFile()
            ->m_fDownloadfile.close();


    QMessageBox::information(
                &Index::getInstance(),
                "提示",
                "下载完成");

}

void ResHandler::shareFileResend()
{
    char strSendName[32] = {'\0'};
    memcpy(strSendName, pdu->caData, 32);
    QString strSharePath = QString(pdu->caMsg);

    //从路径提取纯文件名
    int index = strSharePath.lastIndexOf('/');
    QString strFileName = strSharePath.right(strSharePath.size() - index - 1);

    //弹窗确认
    QString strMsg = QString("%1分享文件：%2\n是否接受？").arg(strSendName).arg(strFileName);
    int ret = QMessageBox::question(&Index::getInstance(), "分享文件", strMsg);
    // 同意标记用int：1=接受 0=拒绝，固定4字节，无歧义
    int iAcceptFlag = (ret == QMessageBox::Yes) ? 1 : 0;

    //构造并发送 SHARE_FILE_RESPOND 响应包
    std::string stdPath = strSharePath.toStdString();
    PDU* respdu = mkPDU(stdPath.size() + 1);
    respdu->uiType = ENUM_MSG_TYPE_SHARE_FILE_RESPOND;

    // caData 整体清零
    memset(respdu->caData, 0, 64);
    // 0~31字节：接收者（当前用户）用户名
    std::string strMyName = Client::getInstance().m_strLoginName.toStdString();
    memcpy(respdu->caData, strMyName.c_str(), std::min((size_t)31, strMyName.size()));
    // 32~35字节：int类型同意标记，固定4字节，和服务器读取位置严格对齐
    memcpy(respdu->caData + 32, &iAcceptFlag, sizeof(int));

    // caMsg：原文件完整路径（带结束符）
    memcpy(respdu->caMsg, stdPath.c_str(), stdPath.size() + 1);

    Client::getInstance().sendMsg(respdu);

}

void ResHandler::shareFileAgree()
{
    bool ret;
    memcpy(&ret,pdu->caData,sizeof(bool));
    if(ret)
    {
        QMessageBox::information(&Index::getInstance(),"提示","分享文件完成");

    }
    else
    {
        QMessageBox::information(&Index::getInstance(),"提示","分享文件失败");
    }
}
