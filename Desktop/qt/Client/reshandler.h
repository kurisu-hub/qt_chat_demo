#ifndef RESHANDLER_H
#define RESHANDLER_H

#include "protocol.h"

#include <QString>



class ResHandler
{
public:
    PDU*pdu;
    void regist();
    void login();
    void findUser();
    void onlineUser();
    void addFriend();
    void addFriendResend();
    void addFriendAgree();
    void flushFriend();
    void deleteFriend();
    void chat();
    void createFile();
    void flushFile();
    void delFile();
    void renameFile();
    void uploadFileInit();
    ResHandler();
};

#endif // RESHANDLER_H
