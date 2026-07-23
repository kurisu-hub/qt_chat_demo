#include "msghandler.h"
#include "mytcpserver.h"
#include "operatedb.h"
#include "server.h"
#include <QDebug>
#include <QDir>
MsgHandler::MsgHandler()
{

}

PDU *MsgHandler::regist()
{
    //通过两个char来接受
    char caName[32]={'\0'};
    char caPwd[32]={'\0'};
    //用memcpy函数读取pdu中caData的内容
    memcpy(caName,pdu->caData,32);
    memcpy(caPwd,pdu->caData+32,32);
    //通过单例来构造类引用handleRegist函数，传入caName和caPwd
    bool ret=OperateDB::getInstance().handleRegist(caName,caPwd);
    //单独打印ret检查是否为true
    qDebug()<<"regist ret"<<ret;
    if(ret){
        QDir dir;
        dir.mkdir(QString("%1/%2").arg(Server::getInstance().m_strRootPath).arg(caName));
        qDebug()<<"创造用户目录"<<QString("%1/%2").arg(Server::getInstance().m_strRootPath).arg(caName);

    }

    //这里创建一个新的PDU的对象respdu，用于发送给客户端
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,&ret,sizeof(bool));
    //将respdu的类型定义为注册反映类型
    respdu->uiType=ENUM_MSG_TYPE_REGIST_RESPOND;
    return respdu;
}

PDU *MsgHandler::login(QString &strName)
{
    char caName[32]={'\0'};
    char caPwd[32]={'\0'};
    memcpy(caName,pdu->caData,32);
    memcpy(caPwd,pdu->caData+32,32);
    bool ret =OperateDB::getInstance().handleLogin(caName,caPwd);
    qDebug()<<"login ret"<<ret;
    if (ret)
    {   //将登录名赋值给caName
        strName=caName;
        qDebug()<<"登录名赋值:"<<strName;
    }
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,&ret,sizeof(bool));
    respdu->uiType=ENUM_MSG_TYPE_LOGIN_RESPOND;
    return respdu;
}

PDU *MsgHandler::findUser()
{   //服务器接受并调用处理数据库类的函数查询是否存在，返回值为bool类型
    char caName[32]={'\0'};
    memcpy(caName,pdu->caData,32);
    int ret=OperateDB::getInstance().handleFindUser(caName);
    qDebug()<<"find user ret "<<ret;
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,&ret,sizeof(int));
    respdu->uiType=ENUM_MSG_TYPE_FIND_USER_RESPOND;
    return respdu;
}

PDU *MsgHandler::onlineUser()
{
    QStringList ret=OperateDB::getInstance().handleOnlineUser();
    PDU*respdu=mkPDU(ret.size()*32);
    qDebug()<<"ret"<<ret.size();
    for(int i=0;i<ret.size();i++)
    {
        qDebug()<<"name:"<<ret[i];
        memcpy(respdu->caMsg+32*i,ret[i].toStdString().c_str(),32);

    }
    respdu->uiType=ENUM_MSG_TYPE_ONLINE_USER_RESPOND;
    return respdu;
}

PDU *MsgHandler::addFriend()
{
    char caCurName[32]={'\0'};
    char caTurName[32]={'\0'};
    qDebug()<<"pdu->caData"<<pdu->caData;
    qDebug()<<"pdu->caData+32"<<pdu->caData+32;
    memcpy(caCurName,pdu->caData,32);
    memcpy(caTurName,pdu->caData+32,32);
    int ret =OperateDB::getInstance().handleAddFriend(caCurName,caTurName);
    qDebug()<<"addFriend ret"<<ret;
    if (ret==1)
    {
       MyTcpServer::getInstance().resend(caTurName,pdu);
       return NULL;
    }
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,&ret,sizeof(int));
    respdu->uiType=ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
    return respdu;
}

PDU *MsgHandler::addfriendAgree()
{
    char caCurName[32]={'\0'};
    char caTurName[32]={'\0'};
    memcpy(caCurName,pdu->caData,32);
    memcpy(caTurName,pdu->caData+32,32);
    qDebug()<<"pdu->caData:"<<pdu->caData;
    qDebug()<<"pdu->caData+32"<<pdu->caData+32;
    qDebug()<<"caCurName:"<<caCurName;
    qDebug()<<"caTurName:"<<caTurName;
    bool ret =OperateDB::getInstance().handleAddFriendAgree(caCurName,caTurName);
    qDebug()<<"addFriendAgree ret"<<ret;
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,&ret,sizeof(bool));
    respdu->uiType=ENUM_MSG_TYPE_ADD_FRIEND_AGREE_RESPOND;
    MyTcpServer::getInstance().resend(caCurName,respdu);
    return respdu;
}

PDU *MsgHandler::flushFriend()
{   //接受登录名
    char caName[32]={'\0'};
    memcpy(caName,pdu->caData,32);
    //在数据库中查询该好友的friend表，用res接收
    QStringList res=OperateDB::getInstance().handleFlushFriend(caName);
    //创造pdu的大小为res的大小乘以32，每次的信息量都是32位字节
    PDU*respdu=mkPDU(res.size()*32);
    respdu->uiType=ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;
    //每次的信息都是32位字节，所以每次都偏移32位就行了，接着返回respdu发送回客户端
    for(int i=0;i<res.size();i++){
        memcpy(respdu->caMsg+32*i,res[i].toStdString().c_str(),32);
    }
    return respdu;

}

PDU *MsgHandler::deleteFriend()
{
    char caCurName[32]={'\0'};
    char caTurName[32]={'\0'};
    memcpy(caCurName,pdu->caData,32);
    memcpy(caTurName,pdu->caData+32,32);
    bool ret=OperateDB::getInstance().handleDeleteFriend(caCurName,caTurName);
    PDU*respdu=mkPDU();
    respdu->uiType=ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;
    memcpy(respdu->caData,&ret,sizeof(ret));
    return respdu;
}

PDU *MsgHandler::chat()
{   char caCurName[32]={'\0'};
    char caTurName[32]={'\0'};
    memcpy(caCurName,pdu->caData,32);
    memcpy(caTurName,pdu->caData+32,32);
    int ret=OperateDB::getInstance().isFriend(caCurName,caTurName);
    if(ret){
        MyTcpServer::getInstance().resend(caTurName,pdu);
        return NULL;
    }
    PDU*respdu=mkPDU();
    respdu->uiType=ENUM_MSG_TYPE_CHAT_REQUEST;
    memcpy(respdu->caData,&ret,sizeof(int));
    return respdu;
}

PDU *MsgHandler::createFile()
{
    char caFileName[32]={'\0'};
    memcpy(caFileName,pdu->caData,32);
    QDir dir;
    //在更换电脑了之后发现其实有一些bug，发现创建文件夹失败，原因是文件夹读取内容时无主即账号目录名导致读取失败，采用mkpath递归构建好一些
     bool ret = dir.mkpath(QString("%1/%2").arg(pdu->caMsg).arg(caFileName));
    qDebug()<<"ret"<<ret;
    PDU*respdu=mkPDU();
    respdu->uiType=ENUM_MSG_TYPE_CREATE_FILE_RESPOND;
    memcpy(respdu->caData,&ret,sizeof(ret));
    return respdu;
}


PDU *MsgHandler::flushFile()
{   qDebug()<<"服务器接受消息";
    QDir dir(pdu->caMsg);
    QFileInfoList fileInfoList=dir.entryInfoList();
    PDU*respdu=mkPDU(sizeof(FileInfo)*(fileInfoList.size()-2));
    respdu->uiType=ENUM_MSG_TYPE_FLUSH_FILE_RESPOND;
    for(int i=0,j=0;i<fileInfoList.size();i++){
        if(fileInfoList[i].fileName() == QString(".")|| fileInfoList[i].fileName() == QString("..")){
            continue;
        }
        FileInfo*pFileInfo=(FileInfo*)respdu->caMsg+j++;
        memcpy(pFileInfo->caName,fileInfoList[i].fileName().toStdString().c_str(),32);
        if(fileInfoList[i].isDir()){
            pFileInfo->uiType=0;
        }
        else{
            pFileInfo->uiType=1;
        }
        qDebug()<<"caName"<<pFileInfo->caName<<"uiType"<<pFileInfo->uiType;
    }
    return respdu;
}

PDU *MsgHandler::delFile()
{
    char*pPath=pdu->caMsg;
    uint uiType=0;
    memcpy(&uiType,pdu->caData,sizeof(uint));
    bool ret;
    if(uiType==0){
        QDir dir(pPath);
        ret=dir.removeRecursively();
    }
    else {
        QFile file(pPath);
        ret=file.remove();
    }
    PDU*respdu=mkPDU();
    qDebug()<<"ret::"<<ret;
    respdu->uiType=ENUM_MSG_TYPE_DEL_FILE_RESPOND;
    memcpy(respdu->caData,&ret,sizeof(bool));
    return respdu;
}

PDU *MsgHandler::renameFile()
{
    char oldPath[32]={'\0'};
    char newPath[32]={'\0'};
    memcpy(oldPath,pdu->caData,32);
    memcpy(newPath,pdu->caData+32,32);
    QDir dir;
    bool ret;
    ret=dir.rename(oldPath,newPath);
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,&ret,sizeof(bool));
    respdu->uiType=ENUM_MSG_TYPE_RENAME_FILE_RESPOND;
    return respdu;
}

PDU *MsgHandler::uploadFileInit()
{
    char caFileName[32]={'\0'};
    m_iUploadReceivedSize=0;
    memcpy(caFileName,pdu->caData,32);
    memcpy(&m_iUploadFileSize,pdu->caData+32,sizeof(qint64));
    QString strPath=QString("%1/%2").arg(pdu->caMsg).arg(caFileName);
    qDebug()<<"strPath:"<<strPath;
    m_fUploadFile.setFileName(strPath);
    bool ret=m_fUploadFile.open(QIODevice::WriteOnly);
    qDebug()<<"ret:"<<ret;
    if (!ret) {
        // 打印错误码和错误描述
        qDebug() << "Open failed. Error code:" << m_fUploadFile.error();
        qDebug() << "Error info:" << m_fUploadFile.errorString();
        return NULL;
    }
    PDU*respdu=mkPDU();
    memcpy(respdu->caData,&ret,sizeof(bool));
    respdu->uiType=ENUM_MSG_TYPE_UPLOAD_FILE_INIT_RESPOND;
    return respdu;
}

PDU *MsgHandler::uploadFileData()
{
    m_fUploadFile.write(pdu->caMsg,pdu->uiMsgLen);
    m_iUploadReceivedSize+=pdu->uiMsgLen;
    if(m_iUploadReceivedSize <m_iUploadFileSize){
        return NULL;
    }
    m_fUploadFile.close();
    PDU*respdu=mkPDU();
    respdu->uiType=ENUM_MSG_TYPE_UPLOAD_FILE_DATA_RESPOND;
    return respdu;
}

PDU *MsgHandler::downloadFile()
{
    char caPath[128]={0};

      memcpy(caPath,
             pdu->caMsg,
             pdu->uiMsgLen);
      m_fDownloadFile.setFileName(caPath);


      bool ret =
      m_fDownloadFile.open(QIODevice::ReadOnly);



      PDU*respdu=mkPDU();


      memcpy(respdu->caData,
             &ret,
             sizeof(bool));


      respdu->uiType=
      ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND;



      return respdu;
}

PDU *MsgHandler::downloadFileData()
{
    PDU*respdu =
       mkPDU(4096);



       qint64 ret =
       m_fDownloadFile.read(
               respdu->caMsg,
               4096);



       if(ret<=0)
       {

           m_fDownloadFile.close();


           PDU*respdu=mkPDU();


           respdu->uiType=
           ENUM_MSG_TYPE_DOWNLOAD_FILE_FINISH_RESPOND;


           return respdu;
       }



       respdu->uiMsgLen=ret;


       respdu->uiType=
       ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_RESPOND;



       return respdu;
}

PDU *MsgHandler::shareFile()
{
    char strCurName[32]={'\0'};
    int iFriendSize=0;
    memcpy(strCurName,pdu->caData,32);
    memcpy(&iFriendSize,pdu->caData+32,32);
    PDU*resendpdu=mkPDU(pdu->uiMsgLen-iFriendSize*32);
    resendpdu->uiType=pdu->uiType;
    memcpy(resendpdu->caData,strCurName,32);
    memcpy(resendpdu->caMsg,pdu->caMsg+iFriendSize*32,pdu->uiMsgLen-iFriendSize*32);
    char caTmp[32]={'\0'};
    for(int i=0;i<iFriendSize;i++)
    {
        memcpy(caTmp,pdu->caMsg+i*32,32);
        MyTcpServer::getInstance().resend(caTmp,resendpdu);

    }
    free(resendpdu);
    resendpdu=nullptr;
    PDU*respdu=mkPDU();
    respdu->uiType=ENUM_MSG_TYPE_SHARE_FILE_RESPOND;
    return respdu;

}

PDU *MsgHandler::shareFileAgree()
{
    QString strShareFilePath=pdu->caMsg;
    int index=strShareFilePath.lastIndexOf('/');
    QString strFileName=strShareFilePath.right(strShareFilePath.size()-index-1);
    QString strTarPath=QString("%1/%2/%3").arg(Server::getInstance().m_strRootPath).arg(pdu->caData).arg(strFileName);
    bool ret=QFile::copy(strShareFilePath,strTarPath);
    PDU*respdu=mkPDU();
    respdu->uiType=ENUM_MSG_TYPE_SHARE_FILE_RESPOND;
    memcpy(respdu->caData,&ret,sizeof(bool));
    return respdu;
}


