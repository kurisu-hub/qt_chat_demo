#include "sharefile.h"
#include "ui_sharefile.h"
#include"client.h"
#include"index.h"

#include <QMessageBox>
ShareFile::ShareFile(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShareFile)
{
    ui->setupUi(this);
}

ShareFile::~ShareFile()
{
    delete ui;
}

void ShareFile::updateLW()
{
    ui->listWidget->clear();
    QListWidget*friend_LW=Index::getInstance().getFriend()->getFriend_LW();
    for(int i=0;i<friend_LW->count();i++)
    {
        QListWidgetItem*friendItem=friend_LW->item(i);
        QListWidgetItem*newItem=new QListWidgetItem(*friendItem);
        ui->listWidget->addItem(newItem);

    }
}

void ShareFile::on_allSelect_PB_clicked()
{
    for(int i=0;i<ui->listWidget->count();i++)
    {
        ui->listWidget->item(i)->setSelected(true);

    }
}

void ShareFile::on_cancelSelect_PB_clicked()
{
    for (int i=0;i<ui->listWidget->count();i++) {
        ui->listWidget->item(i)->setSelected(false);
    }
}

void ShareFile::on_ok_PB_clicked()
{
    QString strCurName = Client::getInstance().m_strLoginName;
       QString strCurPath = Index::getInstance().getFile()->m_strCurPath;
       QString strSharePath = QString("%1/%2").arg(strCurPath).arg(m_strFileName);

       // 获取所有选中好友
       QList<QListWidgetItem*> pItems = ui->listWidget->selectedItems();
       int iFriendSize = pItems.size();
       if(iFriendSize <= 0)
       {
           qDebug() << "未选中好友";
           return;
       }
       // 计算caMsg总长度：好友数组总长度 + 路径长度+1
       std::string stdSharePath = strSharePath.toStdString();
       int caMsgTotalLen = 32 * iFriendSize + stdSharePath.size() + 1;
       PDU* pdu = mkPDU(caMsgTotalLen);
       pdu->uiType = ENUM_MSG_TYPE_SHARE_FILE_REQUEST;
       // 先清零整个caData，避免脏数据
       memset(pdu->caData, 0, 64);
       // 0~31：发送者用户名
       memcpy(pdu->caData, strCurName.toStdString().c_str(), 31);
       // 32~35：好友数量（int）
       memcpy(pdu->caData + 32, &iFriendSize, sizeof(int));
       // 前半段：好友名称数组
       for(int i=0; i<iFriendSize; i++)
       {
           std::string friendName = pItems[i]->text().toStdString();
           memset(pdu->caMsg + i*32, 0, 32);
           memcpy(pdu->caMsg + i*32, friendName.c_str(), 31);
       }
       // 完整文件路径
       memcpy(pdu->caMsg + 32*iFriendSize, stdSharePath.c_str(), stdSharePath.size() + 1);

       qDebug() << "发送文件分享请求，好友数：" << iFriendSize << " 路径：" << strSharePath;
       Client::getInstance().sendMsg(pdu);
       this->close();
}
