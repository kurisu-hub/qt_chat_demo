#ifndef SHAREFILE_H
#define SHAREFILE_H

#include <QDialog>

namespace Ui {
class ShareFile;
}

class ShareFile : public QDialog
{
    Q_OBJECT

public:

    explicit ShareFile(QWidget *parent = nullptr);
    ~ShareFile();
    QString m_strFileName;
    void updateLW();
private slots:
    void on_allSelect_PB_clicked();

    void on_cancelSelect_PB_clicked();

    void on_ok_PB_clicked();

private:
    Ui::ShareFile *ui;
};

#endif // SHAREFILE_H
