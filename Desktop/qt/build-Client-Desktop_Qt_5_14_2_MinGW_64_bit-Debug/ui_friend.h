/********************************************************************************
** Form generated from reading UI file 'friend.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FRIEND_H
#define UI_FRIEND_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Friend
{
public:
    QHBoxLayout *horizontalLayout;
    QListWidget *listwidget;
    QVBoxLayout *verticalLayout;
    QPushButton *chat_PB;
    QSpacerItem *verticalSpacer;
    QPushButton *flush_PB;
    QSpacerItem *verticalSpacer_2;
    QPushButton *del_PB;
    QSpacerItem *verticalSpacer_3;
    QPushButton *findUser_PB;
    QSpacerItem *verticalSpacer_4;
    QPushButton *onlieUser_PB;

    void setupUi(QWidget *Friend)
    {
        if (Friend->objectName().isEmpty())
            Friend->setObjectName(QString::fromUtf8("Friend"));
        Friend->resize(749, 555);
        horizontalLayout = new QHBoxLayout(Friend);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        listwidget = new QListWidget(Friend);
        listwidget->setObjectName(QString::fromUtf8("listwidget"));

        horizontalLayout->addWidget(listwidget);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        chat_PB = new QPushButton(Friend);
        chat_PB->setObjectName(QString::fromUtf8("chat_PB"));
        QFont font;
        font.setFamily(QString::fromUtf8("Arial"));
        font.setPointSize(18);
        chat_PB->setFont(font);

        verticalLayout->addWidget(chat_PB);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        flush_PB = new QPushButton(Friend);
        flush_PB->setObjectName(QString::fromUtf8("flush_PB"));
        flush_PB->setFont(font);

        verticalLayout->addWidget(flush_PB);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        del_PB = new QPushButton(Friend);
        del_PB->setObjectName(QString::fromUtf8("del_PB"));
        del_PB->setFont(font);

        verticalLayout->addWidget(del_PB);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_3);

        findUser_PB = new QPushButton(Friend);
        findUser_PB->setObjectName(QString::fromUtf8("findUser_PB"));
        findUser_PB->setFont(font);

        verticalLayout->addWidget(findUser_PB);

        verticalSpacer_4 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_4);

        onlieUser_PB = new QPushButton(Friend);
        onlieUser_PB->setObjectName(QString::fromUtf8("onlieUser_PB"));
        onlieUser_PB->setFont(font);

        verticalLayout->addWidget(onlieUser_PB);


        horizontalLayout->addLayout(verticalLayout);


        retranslateUi(Friend);

        QMetaObject::connectSlotsByName(Friend);
    } // setupUi

    void retranslateUi(QWidget *Friend)
    {
        Friend->setWindowTitle(QCoreApplication::translate("Friend", "Form", nullptr));
        chat_PB->setText(QCoreApplication::translate("Friend", "\350\201\212\345\244\251", nullptr));
        flush_PB->setText(QCoreApplication::translate("Friend", "\345\210\267\346\226\260\345\245\275\345\217\213", nullptr));
        del_PB->setText(QCoreApplication::translate("Friend", "\345\210\240\351\231\244\345\245\275\345\217\213", nullptr));
        findUser_PB->setText(QCoreApplication::translate("Friend", "\346\237\245\346\211\276\347\224\250\346\210\267", nullptr));
        onlieUser_PB->setText(QCoreApplication::translate("Friend", "\345\234\250\347\272\277\347\224\250\346\210\267", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Friend: public Ui_Friend {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FRIEND_H
