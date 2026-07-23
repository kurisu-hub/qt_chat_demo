#ifndef MSGHANDLER_H
#define MSGHANDLER_H

#include "protocol.h"

#include <QFile>
#include <QString>



class MsgHandler
{
public:
    MsgHandler();
    QFile m_fUploadFile;
    qint64 m_iUploadFileSize;
    qint64 m_iUploadReceivedSize;
    QFile m_fDownloadFile;
    PDU *pdu;
    PDU *regist();
    PDU *login(QString&strName);
    PDU *findUser();
    PDU*onlineUser();
    PDU*addFriend();
    PDU*addfriendAgree();
    PDU*flushFriend();
    PDU*deleteFriend();
    PDU*chat();
    PDU*createFile();
    PDU*flushFile();
    PDU*delFile();
    PDU*renameFile();
    PDU*uploadFileInit();
    PDU*uploadFileData();
    PDU*downloadFile();
    PDU*downloadFileData();
    PDU*shareFile();
    PDU*shareFileAgree();
};

#endif // MSGHANDLER_H
