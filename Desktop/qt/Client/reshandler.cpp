#include "client.h"
#include "file.h"
#include "index.h"
#include "reshandler.h"
#include "string.h"
#include <QMessageBox>



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
    }
    else
    {
         QMessageBox::information(&Client::getInstance(),"提示","登录失败");
    }
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
