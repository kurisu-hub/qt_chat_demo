#ifndef FRIEND_H
#define FRIEND_H

#include "chat.h"
#include "onlineuser.h"

#include <QWidget>
#include <QSet>
#include <QHash>

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
    void startChatSession(const QString &owner);
    void chatDisconnected();
    void receiveChat(uint type, const QJsonObject &reply);
private slots:
    void on_findUser_PB_clicked();

    void on_onlieUser_PB_clicked();

    void on_flush_PB_clicked();
    void on_del_PB_clicked();


    void on_chat_PB_clicked();

private:
    Ui::Friend *ui;
    QString m_chatOwner;
    QHash<QString, Chat *> m_chats;
    QSet<QString> m_onlineFriends;
    void refreshFriendItem(QListWidgetItem *item);
    Chat *chatFor(const QString &peer);
};

#endif // FRIEND_H
