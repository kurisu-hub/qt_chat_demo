/********************************************************************************
** Form generated from reading UI file 'sharefile.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SHAREFILE_H
#define UI_SHAREFILE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ShareFile
{
public:
    QHBoxLayout *horizontalLayout;
    QListWidget *listWidget;
    QVBoxLayout *verticalLayout_2;
    QPushButton *allSelect_PB;
    QPushButton *cancelSelect_PB;
    QSpacerItem *verticalSpacer;
    QPushButton *ok_PB;

    void setupUi(QDialog *ShareFile)
    {
        if (ShareFile->objectName().isEmpty())
            ShareFile->setObjectName(QString::fromUtf8("ShareFile"));
        ShareFile->resize(400, 300);
        horizontalLayout = new QHBoxLayout(ShareFile);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        listWidget = new QListWidget(ShareFile);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));

        horizontalLayout->addWidget(listWidget);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        allSelect_PB = new QPushButton(ShareFile);
        allSelect_PB->setObjectName(QString::fromUtf8("allSelect_PB"));

        verticalLayout_2->addWidget(allSelect_PB);

        cancelSelect_PB = new QPushButton(ShareFile);
        cancelSelect_PB->setObjectName(QString::fromUtf8("cancelSelect_PB"));

        verticalLayout_2->addWidget(cancelSelect_PB);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);

        ok_PB = new QPushButton(ShareFile);
        ok_PB->setObjectName(QString::fromUtf8("ok_PB"));

        verticalLayout_2->addWidget(ok_PB);


        horizontalLayout->addLayout(verticalLayout_2);


        retranslateUi(ShareFile);

        QMetaObject::connectSlotsByName(ShareFile);
    } // setupUi

    void retranslateUi(QDialog *ShareFile)
    {
        ShareFile->setWindowTitle(QCoreApplication::translate("ShareFile", "Dialog", nullptr));
        allSelect_PB->setText(QCoreApplication::translate("ShareFile", "\345\205\250\351\200\211", nullptr));
        cancelSelect_PB->setText(QCoreApplication::translate("ShareFile", "\345\217\226\346\266\210\351\200\211\344\270\255", nullptr));
        ok_PB->setText(QCoreApplication::translate("ShareFile", "\347\241\256\345\256\232", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ShareFile: public Ui_ShareFile {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SHAREFILE_H
