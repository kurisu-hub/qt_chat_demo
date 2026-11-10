#include "protocol.h"
#include "stdlib.h"
#include "string.h"



PDU *mkPDU(uint uiMsgLen)
{   //定义uiTotalLen，就是消息的总长度
    uint uiTotalLen =sizeof(PDU)+uiMsgLen;
    //定义一个指针
    PDU*pdu=(PDU*)malloc(uiTotalLen);
    if(pdu==NULL)
    {
        exit(1);
    }
    //初始化为0
    memset(pdu,0,uiTotalLen);
    //初始化uiTotalLen和uiMsgLen的值
    pdu->uiTotalLen=uiTotalLen;
    pdu->uiMsgLen=uiMsgLen;
    return pdu;
}
