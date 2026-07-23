#include "sharefile.h"
#include "ui_sharefile.h"
#include"client.h"
#include"index.h"
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
    QString strCurName=Client::getInstance().m_strLoginName;
    QString strCUrPath=Index::getInstance().getFile()->m_strCurPath;
    QString strSharePath=QString("%1/%2").arg(strCUrPath).arg(m_strFileName);
    QList<QListWidgetItem*>pItems=ui->listWidget->selectedItems();
    int iFriendSize=pItems.size();
    PDU*pdu=mkPDU(32*iFriendSize+strSharePath.toStdString().size()+1);
    pdu->uiType=ENUM_MSG_TYPE_SHARE_FILE_REQUEST;
    memcpy(pdu->caData,strCurName.toStdString().c_str(),32);
    memcpy(pdu->caData+32,&iFriendSize,sizeof(int));
    for(int i=0;i<iFriendSize;i++)
    {
        memcpy(pdu->caMsg+i*32,pItems[i]->text().toStdString().c_str(),32);
    }
     memcpy(pdu->caMsg+32*iFriendSize,strCUrPath.toStdString().c_str(),strCUrPath.size());
     Client::getInstance().sendMsg(pdu);
}
