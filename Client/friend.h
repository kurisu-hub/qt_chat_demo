#ifndef FRIEND_H
#define FRIEND_H

#include "chat.h"
#include "onlineuser.h"

#include <QWidget>
#include <QSet>

namespace Ui {
class Friend;
}

class Friend : public QWidget
{
    Q_OBJECT

public:
    ~Friend();
    OnlineUser* m_pOnlineUser;
    void flushFriend_LW(QStringList friendList);
    void flushfriend();
    void applyPresenceSnapshot(const QStringList &onlineFriends);
    void updateFriendPresence(const QString &friendName, bool online);
    explicit Friend(QWidget *parent = nullptr);
    QListWidget* getFriend_LW();
    Chat*m_pChat;
private slots:
    void on_findUser_PB_clicked();

    void on_onlieUser_PB_clicked();

    void on_flush_PB_clicked();
    void on_del_PB_clicked();


    void on_chat_PB_clicked();

private:
    Ui::Friend *ui;
    QSet<QString> m_onlineFriends;
    void refreshFriendItem(QListWidgetItem *item);
};

#endif // FRIEND_H
