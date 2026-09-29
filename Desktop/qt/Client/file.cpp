#include "client.h"
#include "file.h"
#include "protocol.h"
#include "ui_file.h"
#include<QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSet>
File::File(QWidget *parent) :
    QWidget(parent),
      m_pFileList(),
    ui(new Ui::File)

{

    m_strCurPath=m_strUserPath=QString("%1/%2").arg(Client::getInstance().m_strPath).arg(Client::getInstance().m_strLoginName);
    qDebug()<<"1:"<<m_strCurPath<<"2:"<<m_strUserPath;
    ui->setupUi(this);
    m_fDownloadfile.setDirectWriteFallback(false);
    connect(&Client::getInstance().socket, &QTcpSocket::disconnected,
            this, [this] { cancelDownload(); cancelUpload(); });
    connect(&Client::getInstance().socket, &QTcpSocket::bytesWritten,
            this, [this](qint64) { pumpUpload(); });
    flushFile();
      m_pShareFile=new ShareFile;
    qDebug()<<"file构造函数";
}

File::~File()
{
    cancelDownload();
    cancelUpload();
    initFileList();
    delete ui;
}

void File::updateFileList(QList<FileInfo *> pFileList)
{
    // Retain incoming pointers even when the caller passes our own list.
       QList<FileInfo*> listCopy;
       QSet<FileInfo*> incoming;
       for (FileInfo *info : pFileList) {
           if (info && !incoming.contains(info)) {
               incoming.insert(info);
               listCopy.append(info);
           }
       }
       const QSet<FileInfo*> previous(m_pFileList.begin(), m_pFileList.end());
       for (FileInfo *info : previous)
           if (!incoming.contains(info)) delete info;
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
    if (!m_uploadPending) return;
    m_fUploadfile.setFileName(m_strUploadPath);
    if (!m_fUploadfile.open(QIODevice::ReadOnly)) {
        m_uploadPending = false;
        QMessageBox::information(this,"提示","上传文件失败");
        return;
    }
    pumpUpload();
}

void File::pumpUpload()
{
    if (!m_uploadPending || !m_fUploadfile.isOpen()) return;
    while (Client::getInstance().socket.bytesToWrite() < 256 * 1024) {
        const QByteArray chunk = m_fUploadfile.read(64 * 1024);
        if (chunk.isEmpty()) {
            if (!m_fUploadfile.atEnd()) {
                const QString error = m_fUploadfile.errorString();
                cancelUpload();
                QMessageBox::information(this, "提示", QString("上传读取失败：%1").arg(error));
                return;
            }
            m_fUploadfile.close();
            m_uploadPending = false;
            return;
        }
        PDU *pdu = mkPDU(chunk.size());
        memcpy(pdu->caMsg, chunk.constData(), chunk.size());
        pdu->uiType=ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST;
        Client::getInstance().sendMsg(pdu);
    }
}

void File::cancelUpload()
{
    if (m_fUploadfile.isOpen()) m_fUploadfile.close();
    m_uploadPending = false;
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
    if(strFileName.isEmpty()||strFileName.toUtf8().size()>31){
        QMessageBox::information(&Client::getInstance(),"提示","文件名非法");
        return;
    }
    PDU*pdu=mkPDU(m_strCurPath.toStdString().size()+1);
    copyTextField(pdu->caData, strFileName);
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
    const QString selectedName = pItem->text();
    const QString selectedPath = QString("%1/%2").arg(m_strCurPath).arg(selectedName);
    uint selectedType = 0;
    for (const FileInfo *info : m_pFileList)
        if (selectedName == info->caName) selectedType = info->uiType;
    int ret=QMessageBox::question(this,"删除文件",QString("是否删除文件%1").arg(selectedName));
    if(ret!=QMessageBox::Yes)return;
    else{
        QString strPath=selectedPath;
        PDU*pdu=mkPDU(strPath.toStdString().size()+1);
        memcpy(pdu->caMsg,strPath.toStdString().c_str(),strPath.toStdString().size());
        pdu->uiType=ENUM_MSG_TYPE_DEL_FILE_REQUEST;
        memcpy(pdu->caData, &selectedType, sizeof(selectedType));
        Client::getInstance().sendMsg(pdu);
    }
}

void File::on_rename_PB_clicked()
{
    QListWidgetItem*pItem=ui->listWidget->currentItem();
    if(!pItem){
        return;
    }
     const QString parentPath = m_strCurPath;
     const QString oldPath = QString("%1/%2").arg(parentPath).arg(pItem->text());
     QString strFileName=QInputDialog::getText(this,"创建文件","文件名");
     if(strFileName.isEmpty()||strFileName.toUtf8().size()>31){
         QMessageBox::information(&Client::getInstance(),"提示","文件名非法");
         return;
     }
     QString newPath=QString("%1/%2").arg(parentPath).arg(strFileName);
     const QByteArray oldBytes = oldPath.toUtf8();
     const QByteArray newBytes = newPath.toUtf8();
     PDU*pdu=mkPDU(oldBytes.size() + newBytes.size() + 2);
     memcpy(pdu->caMsg, oldBytes.constData(), oldBytes.size() + 1);
     memcpy(pdu->caMsg + oldBytes.size() + 1, newBytes.constData(), newBytes.size() + 1);
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
    if (m_uploadPending) return;
    m_strUploadPath = QFileDialog::getOpenFileName();
       if (m_strUploadPath.isEmpty()) return;   // 用户取消选择时直接返回

       qDebug() << "m_strUploadPath" << m_strUploadPath;
       QFile file(m_strUploadPath);
       if (!file.open(QIODevice::ReadOnly)) {
           QMessageBox::information(this, "提示", "无法打开上传文件");
           return;
       }
       qint64 iFileSize = file.size();
       if (iFileSize < 0 || iFileSize > qint64(1024) * 1024 * 1024) {
           QMessageBox::information(this, "提示", "上传文件不能超过1 GiB");
           return;
       }

       std::string strCurPath = m_strCurPath.toStdString();
       QString strFileName = QFileInfo(m_strUploadPath).fileName(); // 只要文件名，不要整个本地路径


       if (strFileName.toUtf8().size() > 31) {
           QMessageBox::information(this, "提示", "上传文件名不能超过31个UTF-8字节");
           return;
       }
       m_uploadPending = true;
       PDU* pdu = mkPDU(strCurPath.size() + 1);

       // caData 前32字节放文件名，清零后再拷贝，防止越界读
       copyTextField(pdu->caData, strFileName);
       memcpy(pdu->caData + 32, &iFileSize, sizeof(qint64));

       // caMsg 拷贝 m_strCurPath，长度用它自己的长度，不要用 m_strUploadPath 的
       memcpy(pdu->caMsg, strCurPath.c_str(), strCurPath.size());

       pdu->uiType = ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST;
       Client::getInstance().sendMsg(pdu);
}

void File::cancelDownload()
{
    if (m_fDownloadfile.isOpen()) {
        m_fDownloadfile.cancelWriting();
        // commit closes and discards a cancelled QSaveFile; it cannot replace the target.
        m_fDownloadfile.commit();
    }
    m_downloadPending = false;
}

void File::on_download_PB_clicked()
{
    if (m_downloadPending) return;
    QListWidgetItem *item = ui->listWidget->currentItem();
    if (!item || Client::getInstance().socket.state() != QAbstractSocket::ConnectedState)
        return;
    const QByteArray remotePath = QString("%1/%2").arg(m_strCurPath).arg(item->text()).toUtf8();
    // Reserve the transfer before entering the dialog's nested event loop.
    m_downloadPending = true;
    const QString localPath = QFileDialog::getSaveFileName(this, "保存下载文件");
    if (!m_downloadPending) return; // disconnected while choosing a destination
    if (localPath.isEmpty()) {
        cancelDownload();
        return;
    }
    m_fDownloadfile.setFileName(localPath);
    if (!m_fDownloadfile.open(QIODevice::WriteOnly)) {
        const QString error = m_fDownloadfile.errorString();
        cancelDownload();
        QMessageBox::information(this, "提示", QString("无法保存下载文件：%1").arg(error));
        return;
    }
    PDU *request = mkPDU(remotePath.size() + 1);
    memcpy(request->caMsg, remotePath.constData(), remotePath.size() + 1);
    request->uiType = ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST;
    Client::getInstance().sendMsg(request);
}

void File::on_share_PB_clicked()
{

   QListWidgetItem*pItem=ui->listWidget->currentItem();
   if(!pItem)
   {
       return ;
   }
   m_pShareFile->m_strFileName=pItem->text();
   m_pShareFile->updateLW();
   if(m_pShareFile->isHidden())
   {
       m_pShareFile->show();

   }
}
