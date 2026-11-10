#ifndef MSGHANDLER_H
#define MSGHANDLER_H

#include "protocol.h"

#include <QFile>
#include <QSaveFile>
#include <QElapsedTimer>
#include <memory>
#include <QString>



class MsgHandler
{
public:
    MsgHandler();
    QString actor;
    std::unique_ptr<QSaveFile> m_fUploadFile;
    qint64 m_iUploadFileSize = 0;
    qint64 m_iUploadReceivedSize = 0;
    QElapsedTimer m_uploadActivity;
    void expireUpload();
    QFile m_fDownloadFile;
    PDU *pdu = nullptr;
    PDU *regist();
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
