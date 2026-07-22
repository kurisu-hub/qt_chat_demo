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
      m_pFileList(),
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
    initFileList();
    delete ui;
}

void File::updateFileList(QList<FileInfo *> pFileList)
{
    // 如果传进来的就是 m_pFileList 本身，先做一份浅拷贝，避免 initFileList 把它删空
       QList<FileInfo*> listCopy = pFileList;

       initFileList();          // 这里会 delete 掉 m_pFileList 里的旧对象
       ui->listWidget->clear();

       foreach (FileInfo* pFileInfo, listCopy) {
           QListWidgetItem* pItem = new QListWidgetItem;
           if (pFileInfo->uiType == 0) {
               pItem->setIcon(QIcon(QPixmap(":/dir.png")));
           } else if (pFileInfo->uiType == 1) {
               pItem->setIcon(QIcon(QPixmap(":/file.png")));
           }
           pItem->setText(pFileInfo->caName);
           ui->listWidget->addItem(pItem);
       }

       m_pFileList = listCopy;
}

void File::flushFile_LW(QList<FileInfo *> pFileList)
{
    qDebug()<<"数量:"<<pFileList.size();

    foreach(FileInfo*p,pFileList)
    {
        qDebug()<<p<<"名字"<<p->caName;
    }
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
    file.close();
}

void File::initFileList()
{
    foreach(FileInfo* pFileInfo,m_pFileList)
    {
        if(pFileInfo)
        {
            delete pFileInfo;
        }
    }
    m_pFileList.clear();
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
    char caName[32] = {0};
    strncpy(caName, strFileName.toStdString().c_str(), sizeof(caName) - 1);
    memcpy(pdu->caData, caName, 32);
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
     char caOld[32] = {0}, caNew[32] = {0};
     strncpy(caOld, oldPath.toStdString().c_str(), sizeof(caOld) - 1);
     strncpy(caNew, newPath.toStdString().c_str(), sizeof(caNew) - 1);
     memcpy(pdu->caData, caOld, 32);
     memcpy(pdu->caData + 32, caNew, 32);
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
    m_strUploadPath = QFileDialog::getOpenFileName();
       if (m_strUploadPath.isEmpty()) return;   // 用户取消选择时直接返回

       qDebug() << "m_strUploadPath" << m_strUploadPath;
       QFile file(m_strUploadPath);
       qint64 iFileSize = file.size();

       std::string strCurPath = m_strCurPath.toStdString();
       QString strFileName = QFileInfo(m_strUploadPath).fileName(); // 只要文件名，不要整个本地路径
       std::string strFileNameStd = strFileName.toStdString();

       PDU* pdu = mkPDU(strCurPath.size() + 1);

       // caData 前32字节放文件名，清零后再拷贝，防止越界读
       char caFileName[32] = {0};
       strncpy(caFileName, strFileNameStd.c_str(), sizeof(caFileName) - 1);
       memcpy(pdu->caData, caFileName, 32);
       memcpy(pdu->caData + 32, &iFileSize, sizeof(qint64));

       // caMsg 拷贝 m_strCurPath，长度用它自己的长度，不要用 m_strUploadPath 的
       memcpy(pdu->caMsg, strCurPath.c_str(), strCurPath.size());

       pdu->uiType = ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST;
       Client::getInstance().sendMsg(pdu);
}
