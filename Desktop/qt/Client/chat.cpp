

#include "chat.h"
#include "client.h"
#include "protocol.h"
#include "ui_chat.h"
#include <QMessageBox>

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
    if (strMsg.toUtf8().size() > 4095 || strMsg.contains(QChar(0))) {
        QMessageBox::information(this, "提示", "消息过长，最多支持 4095 字节 UTF-8 文本");
        return;
    }
    ui->input_LE->clear();
    PDU*pdu=mkPDU(strMsg.toStdString().size()+1);
    //第一个32是用于记录自己
    copyTextField(pdu->caData, Client::getInstance().m_strLoginName);
    //第二个32是用于记录发送目标
    copyTextField(pdu->caData+32, m_strChatName);
    memcpy(pdu->caMsg,strMsg.toStdString().c_str(),strMsg.toStdString().size());
    pdu->uiType=ENUM_MSG_TYPE_CHAT_REQUEST;
    Client::getInstance().sendMsg(pdu);
    appendAlignedColoredText(ui->show_TE,strMsg,Qt::AlignRight,Qt::green);

}
