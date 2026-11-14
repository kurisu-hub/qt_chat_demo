#include "operatedb.h"
#include "passwordhash.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDateTime>
#include <algorithm>

namespace {
bool run(QSqlQuery &query, const QString &sql, const QVariantList &values = {})
{
    if (!query.prepare(sql))
        return false;
    for (const QVariant &value : values)
        query.addBindValue(value);
    return query.exec();
}
QString nameOf(const char *name) { return QString::fromUtf8(name); }
// Require exactly one account; ambiguous names fail closed.
bool userId(QSqlDatabase db, const char *name, QVariant &id, bool *storageError = nullptr)
{
    QSqlQuery q(db);
    const bool exact = name && run(q, QStringLiteral("SELECT id, name FROM user_info WHERE name = ?"), {nameOf(name)})
        && q.next() && q.value(1).toString().toUtf8() == QByteArray(name);
    if (exact) id = q.value(0);
    const bool unique = exact && !q.next();
    if (storageError) *storageError = q.lastError().isValid();
    return unique && !q.lastError().isValid();
}
int friendship(QSqlDatabase db, const QVariant &a, const QVariant &b)
{
    QSqlQuery q(db);
    if (!run(q, QStringLiteral("SELECT 1 FROM friend WHERE (user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)"), {a, b, b, a}))
        return -1;
    if (q.next())
        return 1;
    return q.lastError().isValid() ? -1 : 0;
}

// 重试以发送者和客户端消息 ID 为准，同时核对会话与正文，避免错误复用 ID。
ChatProtocol::Result findChatRetry(QSqlDatabase db, qint64 sender, qint64 low, qint64 high,
    ChatProtocol::Message &message, bool &found)
{
    QSqlQuery q(db);
    found = false;
    if (!run(q, QStringLiteral("SELECT m.id, m.body, m.created_ms, c.user_low, c.user_high "
        "FROM chat_messages m JOIN chat_conversations c ON c.id = m.conversation_id "
        "WHERE m.sender_id = ? AND m.client_id = ?"), {sender, message.clientId}))
        return ChatProtocol::StorageError;
    if (!q.next()) return q.lastError().isValid() ? ChatProtocol::StorageError : ChatProtocol::Saved;
    found = true;
    if (q.value(3).toLongLong() != low || q.value(4).toLongLong() != high
            || q.value(1).toByteArray() != message.text.toUtf8()) return ChatProtocol::Conflict;
    message.id = q.value(0).toLongLong();
    message.createdMs = q.value(2).toLongLong();
    return ChatProtocol::Saved;
}
}

OperateDB::OperateDB(QObject *parent) : QObject(parent)
{
    m_db = QSqlDatabase::addDatabase("QMYSQL");
}
OperateDB::~OperateDB() { m_db.close(); }
OperateDB &OperateDB::getInstance() { static OperateDB instance; return instance; }
bool OperateDB::ready() const { return m_schemaReady && m_db.isOpen(); }
bool OperateDB::isReady() const { return ready(); }

void OperateDB::connect()
{
    m_schemaReady = false;
    m_db.setHostName("localhost");
    m_db.setPort(3306);
    m_db.setUserName("root");
    m_db.setPassword("123456");
    m_db.setDatabaseName("mydb2503");
    // 可将新服务实例指向独立验收库。未指定时沿用现有部署参数。
    if (qEnvironmentVariableIsSet("QT_CHAT_DB_HOST")) m_db.setHostName(qEnvironmentVariable("QT_CHAT_DB_HOST"));
    if (qEnvironmentVariableIsSet("QT_CHAT_DB_PORT")) m_db.setPort(qEnvironmentVariableIntValue("QT_CHAT_DB_PORT"));
    if (qEnvironmentVariableIsSet("QT_CHAT_DB_NAME")) m_db.setDatabaseName(qEnvironmentVariable("QT_CHAT_DB_NAME"));
    if (qEnvironmentVariableIsSet("QT_CHAT_DB_USER")) m_db.setUserName(qEnvironmentVariable("QT_CHAT_DB_USER"));
    if (qEnvironmentVariableIsSet("QT_CHAT_DB_PASSWORD")) m_db.setPassword(qEnvironmentVariable("QT_CHAT_DB_PASSWORD"));
    if (!m_db.open()) {
        qWarning() << "Database connection failed";
        return;
    }
    // Read-only startup check: never perform DDL or migrate accounts here.
    QSqlQuery q(m_db);
    const bool queried = run(q, QStringLiteral(
        "SELECT DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, IS_NULLABLE "
        "FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() "
        "AND TABLE_NAME = ? AND COLUMN_NAME = ?"),
        {QStringLiteral("user_info"), QStringLiteral("password_hash")});
    const bool hashColumn = queried && q.next() && q.value(0).toString() == QStringLiteral("varchar")
        && q.value(1).toInt() == 255 && q.value(2).toString() == QStringLiteral("YES");
    q.finish();
    const bool columns = hashColumn && run(q, QStringLiteral("SELECT id, name, pwd, password_hash FROM user_info WHERE 1 = 0"));
    q.finish();
    if (!columns) {
        qCritical() << "Database disabled: apply Server/password_migration.sql before startup";
        m_db.close();
        return;
    }
    const bool nameMetadata = run(q, QStringLiteral(
        "SELECT COLLATION_NAME FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ? AND COLUMN_NAME = ?"),
        {QStringLiteral("user_info"), QStringLiteral("name")});
    const bool caseInsensitive = nameMetadata && q.next()
        && q.value(0).toString().endsWith(QStringLiteral("_ci"), Qt::CaseInsensitive);
    q.finish();
    // Group all parts of each index: a composite or prefix unique index does
    // not guarantee uniqueness of the full account name by itself.
    const bool indexMetadata = caseInsensitive && run(q, QStringLiteral(
        "SELECT INDEX_NAME FROM information_schema.STATISTICS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ? "
        "GROUP BY INDEX_NAME HAVING COUNT(*) = 1 AND MAX(NON_UNIQUE) = 0 "
        "AND MAX(COLUMN_NAME) = ? AND COUNT(SUB_PART) = 0"),
        {QStringLiteral("user_info"), QStringLiteral("name")});
    const bool uniqueName = indexMetadata && q.next();
    q.finish();
    if (!uniqueName) {
        qCritical() << "Database disabled: user_info.name requires an _ci collation and a UNIQUE single-column, non-prefix index on name."
                      " Inspect COLUMNS/STATISTICS and follow Server/password_migration.sql; metadata query failures also deny startup.";
        m_db.close();
        return;
    }
    if (!run(q, QStringLiteral("SELECT c.id, c.user_low, c.user_high, m.id, m.conversation_id, "
        "m.sender_id, m.client_id, m.body, m.created_ms FROM chat_conversations c "
        "JOIN chat_messages m ON m.conversation_id = c.id WHERE 1 = 0"))) {
        qCritical() << "Database disabled: apply Server/chat_migration.sql before startup";
        m_db.close();
        return;
    }
    m_schemaReady = true;
}

bool OperateDB::handleRegist(const char *caName, const char *caPwd)
{
    if (!ready() || !caName || !caPwd || !*caName || !*caPwd || handleFindUser(caName) != 2)
        return false;
    QSqlQuery existing(m_db);
    if (!run(existing, QStringLiteral("SELECT id FROM user_info WHERE LOWER(name) = LOWER(?)"), {nameOf(caName)})
        || existing.next() || existing.lastError().isValid())
        return false;
    existing.finish();
    const QString hash = PasswordHash::create(QByteArray(caPwd));
    if (hash.isEmpty())
        return false;
    QSqlQuery q(m_db);
    return run(q, QStringLiteral("INSERT INTO user_info (name, pwd, password_hash) VALUES (?, ?, ?)"),
               {nameOf(caName), QStringLiteral(""), hash}) && q.numRowsAffected() == 1;
}

bool OperateDB::handleLogin(const char *caName, const char *caPwd)
{
    if (!ready() || !caName || !caPwd || !*caName || !*caPwd)
        return false;
    QSqlQuery q(m_db);
    if (!run(q, QStringLiteral("SELECT id, pwd, password_hash, name FROM user_info WHERE name = ?"), {nameOf(caName)}) || !q.next())
        return false;
    const QVariant id = q.value(0);
    const QVariant legacy = q.value(1);
    const QVariant stored = q.value(2);
    if (q.value(3).toString().toUtf8() != QByteArray(caName))
        return false;
    if (q.next() || q.lastError().isValid())
        return false;
    q.finish();
    const QByteArray password(caPwd);
    // Any non-NULL hash, including a malformed/empty one, forbids plaintext fallback.
    if (!stored.isNull())
        return PasswordHash::verify(password, stored.toString());
    if (legacy.isNull() || !PasswordHash::constantTimeEquals(password, legacy.toString().toUtf8()))
        return false;
    const QString hash = PasswordHash::create(password);
    if (hash.isEmpty())
        return false;
    // Atomic compare-and-swap prevents a concurrent password change being overwritten.
    // A failed or lost migration race rejects this login; the client may retry.
    return run(q, QStringLiteral("UPDATE user_info SET password_hash = ?, pwd = ? "
               "WHERE id = ? AND password_hash IS NULL AND BINARY pwd = BINARY ?"),
               {hash, QStringLiteral(""), id, legacy}) && q.numRowsAffected() == 1;
}

void OperateDB::handleOffline(const char *caName)
{
    Q_UNUSED(caName);
    // PresenceStore owns transient online state.
}

int OperateDB::handleFindUser(const char *caName)
{
    if (!ready() || !caName)
        return -1;
    QSqlQuery q(m_db);
    if (!run(q, QStringLiteral("SELECT id, name FROM user_info WHERE name = ?"), {nameOf(caName)}))
        return -1;
    if (!q.next())
        return q.lastError().isValid() ? -1 : 2;
    const bool exact = q.value(1).toString().toUtf8() == QByteArray(caName);
    return q.next() || q.lastError().isValid() ? -1 : (exact ? 1 : 2);
}

QStringList OperateDB::handleOnlineUser()
{
    QStringList result;
    if (!ready())
        return result;
    QSqlQuery q(m_db);
    if (!run(q, QStringLiteral("SELECT name FROM user_info WHERE online = ?"), {1}))
        return result;
    while (q.next())
        result.append(q.value(0).toString());
    return q.lastError().isValid() ? QStringList() : result;
}

int OperateDB::handleAddFriend(const char *caCurName, const char *caTarName)
{
    if (!ready() || !caCurName || !caTarName)
        return -1;
    if (nameOf(caCurName) == nameOf(caTarName))
        return -3;
    QVariant a, b;
    if (!userId(m_db, caCurName, a) || !userId(m_db, caTarName, b))
        return -1;
    if (a == b)
        return -3;
    const int state = friendship(m_db, a, b);
    return state < 0 ? -1 : (state == 1 ? -2 : 1);
}

bool OperateDB::handleAddFriendAgree(const char *caCurName, const char *caTarName)
{
    if (!ready() || !caCurName || !caTarName)
        return false;
    QVariant a, b;
    if (!userId(m_db, caCurName, a) || !userId(m_db, caTarName, b) || a == b || friendship(m_db, a, b) != 0)
        return false;
    QSqlQuery q(m_db);
    return run(q, QStringLiteral("INSERT INTO friend (user_id, friend_id) VALUES (?, ?)"), {a, b})
        && q.numRowsAffected() == 1;
}

QStringList OperateDB::handleFlushFriend(const char *caName)
{
    QStringList result;
    if (!ready() || !caName)
        return result;
    QVariant id;
    if (!userId(m_db, caName, id))
        return result;
    QSqlQuery q(m_db);
    if (!run(q, QStringLiteral("SELECT name FROM user_info WHERE id IN ("
        "SELECT user_id FROM friend WHERE friend_id = ? "
        "UNION SELECT friend_id FROM friend WHERE user_id = ?)"), {id, id}))
        return result;
    while (q.next())
        result.append(q.value(0).toString());
    return q.lastError().isValid() ? QStringList() : result;
}

bool OperateDB::handleDeleteFriend(const char *caCurName, const char *caTarName)
{
    if (!ready() || !caCurName || !caTarName)
        return false;
    QVariant a, b;
    if (!userId(m_db, caCurName, a) || !userId(m_db, caTarName, b))
        return false;
    QSqlQuery q(m_db);
    return run(q, QStringLiteral("DELETE FROM friend WHERE (user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)"), {a, b, b, a});
}

int OperateDB::isFriend(const char *caCurName, const char *caTarName)
{
    if (!ready() || !caCurName || !caTarName)
        return 0;
    QVariant a, b;
    if (!userId(m_db, caCurName, a) || !userId(m_db, caTarName, b) || a == b)
        return 0;
    return friendship(m_db, a, b) == 1 ? 1 : 0;
}

// 好友可发新消息。已提交的请求即使好友关系之后解除，也能用原 ID 取回确认。
ChatProtocol::Result OperateDB::storeChatMessage(const QString &actor, const QString &peer,
    ChatProtocol::Message &message, bool &inserted)
{
    using namespace ChatProtocol;
    inserted = false;
    if (!ready()) return StorageError;
    QVariant actorId, peerId;
    bool storageError = false;
    if (!userId(m_db, actor.toUtf8().constData(), actorId, &storageError)
            || !userId(m_db, peer.toUtf8().constData(), peerId, &storageError) || actorId == peerId)
        return storageError ? StorageError : Forbidden;
    const qint64 sender = actorId.toLongLong();
    const qint64 low = qMin(sender, peerId.toLongLong());
    const qint64 high = qMax(sender, peerId.toLongLong());
    message.sender = actor;
    bool found = false;
    const Result retry = findChatRetry(m_db, sender, low, high, message, found);
    if (retry != Saved || found) return retry;
    const int relation = friendship(m_db, actorId, peerId);
    if (relation != 1) return relation == 0 ? Forbidden : StorageError;
    if (!m_db.transaction()) return StorageError;

    QSqlQuery q(m_db);
    // MySQL 对已有会话也取得行锁，使同一会话的 ID 分配与提交顺序一致。
    const QString upsert = m_db.driverName() == "QSQLITE"
        ? QStringLiteral("INSERT INTO chat_conversations (user_low, user_high) VALUES (?, ?) "
                         "ON CONFLICT(user_low, user_high) DO NOTHING")
        : QStringLiteral("INSERT INTO chat_conversations (user_low, user_high) VALUES (?, ?) "
                         "ON DUPLICATE KEY UPDATE id = id");
    qint64 conversation = 0;
    if (run(q, upsert, {low, high}) && run(q, QStringLiteral(
            "SELECT id FROM chat_conversations WHERE user_low = ? AND user_high = ?"), {low, high}) && q.next())
        conversation = q.value(0).toLongLong();
    q.finish();
    message.createdMs = QDateTime::currentMSecsSinceEpoch();
    const bool written = conversation > 0 && run(q, QStringLiteral(
        "INSERT INTO chat_messages (conversation_id, sender_id, client_id, body, created_ms) "
        "VALUES (?, ?, ?, ?, ?)"), {conversation, sender, message.clientId, message.text.toUtf8(), message.createdMs});
    message.id = written ? q.lastInsertId().toLongLong() : 0;
    q.finish();
    if (message.id > 0 && m_db.commit()) {
        inserted = true;
        return Saved;
    }

    m_db.rollback();
    // 并发重试可能先提交并触发唯一约束。结束事务后再查，读取已提交的原记录。
    const Result committed = findChatRetry(m_db, sender, low, high, message, found);
    return found ? committed : StorageError;
}

// 固定会话成员可查历史。取 31 条来判定是否还有上一页，对外按从旧到新返回 30 条。
ChatProtocol::Result OperateDB::chatHistory(const QString &actor, const QString &peer, qint64 before,
    QList<ChatProtocol::Message> &messages, bool &hasMore)
{
    using namespace ChatProtocol;
    messages.clear();
    hasMore = false;
    if (!ready()) return StorageError;
    QVariant actorId, peerId;
    bool storageError = false;
    if (!userId(m_db, actor.toUtf8().constData(), actorId, &storageError)
            || !userId(m_db, peer.toUtf8().constData(), peerId, &storageError) || actorId == peerId)
        return storageError ? StorageError : Forbidden;
    const qint64 a = actorId.toLongLong();
    const qint64 b = peerId.toLongLong();
    QSqlQuery q(m_db);
    if (!run(q, QStringLiteral("SELECT id FROM chat_conversations WHERE user_low = ? AND user_high = ?"),
             {qMin(a, b), qMax(a, b)})) return StorageError;
    if (!q.next()) {
        if (q.lastError().isValid()) return StorageError;
        q.finish();
        const int relation = friendship(m_db, actorId, peerId);
        return relation < 0 ? StorageError : (relation == 1 ? Saved : Forbidden);
    }
    const QVariant conversation = q.value(0);
    q.finish();
    QString sql = QStringLiteral("SELECT id, sender_id, client_id, body, created_ms FROM chat_messages WHERE conversation_id = ?");
    QVariantList args{conversation};
    if (before > 0) {
        sql += QStringLiteral(" AND id < ?");
        args.append(before);
    }
    sql += QStringLiteral(" ORDER BY id DESC LIMIT %1").arg(PageSize + 1);
    if (!run(q, sql, args)) return StorageError;
    while (q.next()) {
        if (messages.size() == PageSize) {
            hasMore = true;
            break;
        }
        Message message;
        message.id = q.value(0).toLongLong();
        message.sender = q.value(1).toLongLong() == a ? actor : peer;
        message.clientId = q.value(2).toString();
        message.text = QString::fromUtf8(q.value(3).toByteArray());
        message.createdMs = q.value(4).toLongLong();
        messages.append(message);
    }
    if (q.lastError().isValid()) return StorageError;
    std::reverse(messages.begin(), messages.end());
    return Saved;
}
