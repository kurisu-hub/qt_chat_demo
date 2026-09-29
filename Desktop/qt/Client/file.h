#ifndef FILE_H
#define FILE_H

#include "protocol.h"
#include "sharefile.h"

#include <QFile>
#include <QSaveFile>
#include <QListWidget>
#include <QWidget>

namespace Ui {
class File;
}

class File : public QWidget
{
    Q_OBJECT

public:
    //用户的路径
    QString m_strUserPath;
    //当前浏览的路径
    QString m_strCurPath;
    QList<FileInfo*>m_pFileList;
    QString m_strUploadPath;
    QFile m_fUploadfile;
    bool m_uploadPending = false;
    QSaveFile m_fDownloadfile;
    bool m_downloadPending = false;
    void cancelDownload();
    ShareFile *m_pShareFile;
    explicit File(QWidget *parent = nullptr);
    ~File();
    void updateFileList(QList<FileInfo*> pFileList);
    void flushFile_LW(QList<FileInfo*>pFileList);
    void flushFile();
    void UploadFile();
    void pumpUpload();
    void cancelUpload();
    void initFileList();
private slots:
    void on_mkDir_PB_clicked();

    void on_flush_PB_clicked();

    void on_delFile_PB_clicked();

    void on_rename_PB_clicked();


    void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_return_PB_clicked();

    void on_upload_PB_clicked();

    void on_download_PB_clicked();

    void on_share_PB_clicked();

private:
    Ui::File *ui;
};

#endif // FILE_H
