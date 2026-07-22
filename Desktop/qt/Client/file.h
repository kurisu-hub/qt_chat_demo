#ifndef FILE_H
#define FILE_H

#include "protocol.h"

#include <QListWidget>
#include <QWidget>

namespace Ui {
class File;
}

class File : public QWidget
{
    Q_OBJECT

public:
    QString m_strUserPath;
    QString m_strCurPath;
    QList<FileInfo*>m_pFileList;
    QString m_strUploadPath;
    explicit File(QWidget *parent = nullptr);
    ~File();
    void updateFileList(QList<FileInfo*> pFileList);
    void flushFile_LW(QList<FileInfo*>pFileList);
    void flushFile();
    void UploadFile();
    void initFileList();
private slots:
    void on_mkDir_PB_clicked();

    void on_flush_PB_clicked();

    void on_delFile_PB_clicked();

    void on_rename_PB_clicked();


    void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_return_PB_clicked();

    void on_upload_PB_clicked();

private:
    Ui::File *ui;
};

#endif // FILE_H
