#ifndef OPERATEDB_H
#define OPERATEDB_H
#include <QObject>
#include <QSqlDatabase>
#include <QStringList>

class OperateDB : public QObject
{
    Q_OBJECT
public:
    QSqlDatabase m_db;
    ~OperateDB();
    void connect();
    bool isReady() const;
    static OperateDB &getInstance();
    bool handleRegist(const char *caName, const char *caPwd);
    bool handleLogin(const char *caName, const char *caPwd);
    void handleOffline(const char *caName);
    // Found=1, missing=2, error=-1; never returns an ID.
    int handleFindUser(const char *caName);
    QStringList handleOnlineUser();
    // Eligible=1, missing/error=-1, already friends=-2, self=-3.
    int handleAddFriend(const char *caCurName, const char *caTarName);
    bool handleAddFriendAgree(const char *caCurName, const char *caTarName);
    QStringList handleFlushFriend(const char *caName);
    bool handleDeleteFriend(const char *caCurName, const char *caTarName);
    int isFriend(const char *caCurName, const char *caTarName);
private:
    explicit OperateDB(QObject *parent = nullptr);
    OperateDB(const OperateDB &) = delete;
    OperateDB &operator=(const OperateDB &) = delete;
    bool m_schemaReady = false;
    bool ready() const;
};
#endif
