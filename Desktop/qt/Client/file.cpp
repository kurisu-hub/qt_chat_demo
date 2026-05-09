#include "client.h"
#include "file.h"
#include "protocol.h"
#include "ui_file.h"
#include<QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
File::File(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::File)
{

    m_strCurPath=m_strUserPath=QString("%1/%2").arg(Client::getInstance().m_strPath).arg(Client::getInstance().m_strLoginName);
    qDebug()<<"1:"<<m_strCurPath<<"2:"<<m_strUserPath;
    ui->setupUi(this);
    flushFile();
    qDebug()<<"file构造函数";
}

File::~File()
{
    delete ui;
}

void File::updateFileList(QList<FileInfo *> pFileList)
{
    foreach(FileInfo*pFileInfo,m_pFileList){
            delete pFileInfo;
        }
    ui->listWidget->clear();
    m_pFileList=pFileList;
    foreach(FileInfo*pFileInfo,pFileList){
       QListWidgetItem*pItem= new QListWidgetItem;
       if(pFileInfo->uiType==0){
           pItem->setIcon(QIcon(QPixmap(":/dir.png")));
       }
       else if(pFileInfo->uiType==1){
           pItem->setIcon(QIcon(QPixmap(":/file.png")));
       }
       pItem->setText(pFileInfo->caName);
       qDebug()<<"updateFileList pFileInfo->caName"<<pFileInfo->caName;
       ui->listWidget->addItem(pItem);
    }
}

void File::flushFile_LW(QList<FileInfo *> pFileList)
{
    ui->listWidget->clear();
    updateFileList(pFileList);
}

void File::flushFile()
{
    PDU*pdu=mkPDU(m_strCurPath.toStdString().size()+1);
    memcpy(pdu->caMsg,m_strCurPath.toStdString().c_str(),m_strCurPath.toStdString().size());
    pdu->uiType=ENUM_MSG_TYPE_FLUSH_FILE_REQUEST;
    Client::getInstance().sendMsg(pdu);
}


void File::UploadFile()
{
    QFile file(m_strUploadPath);
    file.open(QIODevice::ReadOnly);
    while(true){
        PDU*pdu=mkPDU(4096);
        qint64 ret=file.read(pdu->caMsg,4096);
        if(ret<0){
            QMessageBox::information(this,"提示","上传文件失败");
            return;
        }
        if(ret==0)break;
        pdu->uiType=ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST;
        pdu->uiMsgLen=ret;
        pdu->uiTotalLen=ret+sizeof(PDU);
        Client::getInstance().sendMsg(pdu);
    }
}



void File::on_mkDir_PB_clicked()
{
    QString strFileName=QInputDialog::getText(this,"创建文件","文件名");
    qDebug()<<"strFileName"<<strFileName;
    if(strFileName.isEmpty()||strFileName.toStdString().size()>32){
        QMessageBox::information(&Client::getInstance(),"提示","文件名非法");
        return;
    }
    PDU*pdu=mkPDU(m_strCurPath.toStdString().size()+1);
    memcpy(pdu->caData,strFileName.toStdString().c_str(),32);
    memcpy(pdu->caMsg,m_strCurPath.toStdString().c_str(),m_strCurPath.toStdString().size());
    pdu->uiType=ENUM_MSG_TYPE_CREATE_FILE_REQUEST;
    Client::getInstance().sendMsg(pdu);

}

void File::on_flush_PB_clicked()
{
    flushFile();
}

void File::on_delFile_PB_clicked()
{
    QListWidgetItem*pItem=ui->listWidget->currentItem();
    if(!pItem){
        return;
    }
    int ret=QMessageBox::question(this,"删除文件",QString("是否删除文件%1").arg(pItem->text()));
    if(ret!=QMessageBox::Yes)return;
    else{
        QString strPath=QString("%1/%2").arg(this->m_strCurPath).arg(pItem->text());
        PDU*pdu=mkPDU(strPath.toStdString().size()+1);
        memcpy(pdu->caMsg,strPath.toStdString().c_str(),strPath.toStdString().size());
        pdu->uiType=ENUM_MSG_TYPE_DEL_FILE_REQUEST;
        foreach(FileInfo*pFileInfo,m_pFileList){
            if(pItem->text()==pFileInfo->caName){
                qDebug()<<"pFileInfo->caName"<<pFileInfo->caName;
                memcpy(pdu->caData,&pFileInfo->uiType,sizeof(uint));
            }
        }
        Client::getInstance().sendMsg(pdu);
    }
}

void File::on_rename_PB_clicked()
{
    QListWidgetItem*pItem=ui->listWidget->currentItem();
    if(!pItem){
        return;
    }
     QString strFileName=QInputDialog::getText(this,"创建文件","文件名");
     if(strFileName.isEmpty()||strFileName.toStdString().size()>32){
         QMessageBox::information(&Client::getInstance(),"提示","文件名非法");
         return;
     }
     QString oldPath=QString("%1/%2").arg(this->m_strCurPath).arg(pItem->text());
     QString newPath=QString("%1/%2").arg(this->m_strCurPath).arg(strFileName);
     PDU*pdu=mkPDU();
     memcpy(pdu->caData,oldPath.toStdString().c_str(),32);
     memcpy(pdu->caData+32,newPath.toStdString().c_str(),32);
     pdu->uiType=ENUM_MSG_TYPE_RENAME_FILE_REQUEST;
     Client::getInstance().sendMsg(pdu);
}

void File::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
    //我们只能获取用户名，还要判断其是否是文件夹，所以foreach遍历来获取类型
    foreach(FileInfo*pFile,m_pFileList){
        if(item->text()==pFile->caName&&pFile->uiType!=0)return;
    }
    m_strCurPath=QString("%1/%2").arg(m_strCurPath).arg(item->text());
    flushFile();
}

void File::on_return_PB_clicked()
{
    if(m_strCurPath==m_strUserPath)return;
    int index=m_strCurPath.lastIndexOf('/');
    m_strCurPath.remove(index,m_strCurPath.size()-index);
    flushFile();

}

void File::on_upload_PB_clicked()
{
    m_strUploadPath=QFileDialog::getOpenFileName();
    qDebug()<<"m_strUploadPath"<<m_strUploadPath;
    int index=m_strUploadPath.lastIndexOf('/');
    qDebug()<<"index"<<index;
    QString strFileName=m_strUploadPath.right(m_strUploadPath.size()-index-1);
    QFile file(m_strUploadPath);
    qint64 iFileSize=file.size();
    qDebug()<<"strFileName:"<<strFileName;
    PDU*pdu=mkPDU(m_strCurPath.toStdString().size()+1);
    memcpy(pdu->caData,strFileName.toStdString().c_str(),strFileName.toStdString().size());
    memcpy(pdu->caData+32,&iFileSize,sizeof(qint64));
    memcpy(pdu->caMsg,m_strCurPath.toStdString().c_str(),m_strUploadPath.toStdString().size());
    pdu->uiType=ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST;
    Client::getInstance().sendMsg(pdu);
}
