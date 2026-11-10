#include "presencestore.h"

PresenceStore &PresenceStore::getInstance() { static PresenceStore instance; return instance; }
bool PresenceStore::login(const QString &userName, const QString &sessionId)
{
    QSet<QString> &sessions = m_sessions[userName];
    const bool becameOnline = sessions.isEmpty();
    sessions.insert(sessionId);
    return becameOnline;
}
void PresenceStore::heartbeat(const QString &userName, const QString &sessionId)
{
    if (!userName.isEmpty() && !sessionId.isEmpty()) m_sessions[userName].insert(sessionId);
}
bool PresenceStore::logout(const QString &userName, const QString &sessionId)
{
    QHash<QString, QSet<QString> >::iterator it = m_sessions.find(userName);
    if (it == m_sessions.end()) return false;
    it->remove(sessionId);
    if (!it->isEmpty()) return false;
    m_sessions.erase(it);
    return true;
}
bool PresenceStore::isOnline(const QString &userName) const
{
    return m_sessions.contains(userName) && !m_sessions.value(userName).isEmpty();
}
QStringList PresenceStore::onlineUsers() const { return m_sessions.keys(); }
