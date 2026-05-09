

#include "chat.h"
#include "client.h"
#include "protocol.h"
#include "ui_chat.h"

Chat::Chat(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Chat)
{
    ui->setupUi(this);
}

Chat::~Chat()
{
    delete ui;
}

void Chat::updateShow_TE(QString strMsg)
{
    ui->show_TE->append(strMsg);
}

void Chat::appendAlignedColoredText(QTextEdit *textEdit, const QString &text, Qt::Alignment alignment, const QColor &color)
{
    QTextCursor cursor = textEdit->textCursor();

       // 文本块格式（对齐）
       QTextBlockFormat blockFormat;
       blockFormat.setAlignment(alignment);

       // 字符格式（颜色）
       QTextCharFormat charFormat;
       charFormat.setForeground(color);

       // 插入新块+应用格式+插入文本
       cursor.movePosition(QTextCursor::End);
       cursor.insertBlock(blockFormat);
       cursor.setCharFormat(charFormat);
       cursor.insertText(text);
}

QTextEdit *Chat::getshow_LE()
{
    return ui->show_TE;
}




void Chat::on_send_PB_clicked()
{
    QString strMsg=ui->input_LE->text();
    if(strMsg.isEmpty()){
        return;
    }
    ui->input_LE->clear();
    PDU*pdu=mkPDU(strMsg.toStdString().size()+1);
    //第一个32是用于记录自己
    memcpy(pdu->caData,Client::getInstance().m_strLoginName.toStdString().c_str(),32);
    //第二个32是用于记录发送目标
    memcpy(pdu->caData+32,m_strChatName.toStdString().c_str(),32);
    memcpy(pdu->caMsg,strMsg.toStdString().c_str(),strMsg.toStdString().size());
    pdu->uiType=ENUM_MSG_TYPE_CHAT_REQUEST;
    Client::getInstance().sendMsg(pdu);
    appendAlignedColoredText(ui->show_TE,QString("%1").arg(pdu->caMsg),Qt::AlignRight,Qt::green);

}
