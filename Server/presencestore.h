#ifndef PRESENCESTORE_H
#define PRESENCESTORE_H

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class PresenceStore
{
public:
    static PresenceStore &getInstance();
    bool login(const QString &userName, const QString &sessionId);
    void heartbeat(const QString &userName, const QString &sessionId);
    bool logout(const QString &userName, const QString &sessionId);
    bool isOnline(const QString &userName) const;
    QStringList onlineUsers() const;
private:
    PresenceStore() = default;
    QHash<QString, QSet<QString> > m_sessions;
};

#endif
