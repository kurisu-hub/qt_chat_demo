#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtNetwork>
#include <QtSql>
#include <QtTest>
#include <memory>
#include <cstring>
#include <limits>
// Only the database readiness seam is exposed; Qt headers are already parsed.
#define private public
#include "operatedb.h"
#undef private
#include "server.h"
#include "security.h"
#include "passwordhash.h"
#include "presencestore.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Never loads configuration, opens MySQL, or listens on the configured port.
Server::Server(QWidget *parent) : QWidget(parent), m_usPort(0) {}
Server::~Server() = default;
Server &Server::getInstance() { static Server instance; return instance; }
void Server::loadConfig() {}

using Packet = std::unique_ptr<PDU, decltype(&free)>;
static Packet packet(uint type, const QByteArray &body = {})
{
    Packet p(mkPDU(body.size()), &free);
    p->uiType = type;
    if (!body.isEmpty()) memcpy(p->caMsg, body.constData(), body.size());
    return p;
}
static QByteArray textBody(const QString &s) { return s.toUtf8() + '\0'; }
static QByteArray wire(const Packet &p) { return QByteArray(reinterpret_cast<const char *>(p.get()), p->uiTotalLen); }
static bool succeeded(const Packet &p) { return p && p->caData[0] == 1; }
static bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}
static QByteArray readFile(const QString &path)
{
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll();
}

class SecurityTest : public QObject
{
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> root;
    QString alice, bob;
    Packet call(MsgHandler &h, Packet &p, PDU *(MsgHandler::*method)())
    {
        h.pdu = p.get(); return Packet((h.*method)(), &free);
    }
    Packet upload(MsgHandler &h, const QString &dir, const QString &name, qint64 size)
    {
        auto p = packet(ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST, textBody(dir));
        Security::putField(p->caData, name); memcpy(p->caData + 32, &size, sizeof(size));
        return call(h, p, &MsgHandler::uploadFileInit);
    }
private slots:
    void initTestCase()
    {
        auto &db = OperateDB::getInstance();
        db.m_db = QSqlDatabase::addDatabase("QSQLITE", "security-tests");
        db.m_db.setDatabaseName(":memory:");
        QVERIFY2(db.m_db.open(), qPrintable(db.m_db.lastError().text()));
        db.m_schemaReady = true;
        QSqlQuery q(db.m_db);
        QVERIFY(q.exec("CREATE TABLE user_info(id INTEGER PRIMARY KEY, name TEXT UNIQUE COLLATE NOCASE, pwd TEXT, password_hash TEXT)"));
        QVERIFY(q.exec("CREATE TABLE friend(user_id INTEGER, friend_id INTEGER, UNIQUE(user_id, friend_id))"));
        QVERIFY(!MyTcpServer::getInstance().isListening());
    }
    void init()
    {
        root.reset(new QTemporaryDir);
        QVERIFY(root->isValid());
        Server::getInstance().m_strRootPath = root->path();
        alice = Security::userRoot(root->path(), "alice", true);
        bob = Security::userRoot(root->path(), "bob", true);
        QVERIFY(!alice.isEmpty()); QVERIFY(!bob.isEmpty());
        QSqlQuery q(OperateDB::getInstance().m_db);
        QVERIFY(q.exec("DELETE FROM friend")); QVERIFY(q.exec("DELETE FROM user_info"));
        QVERIFY(q.exec("INSERT INTO user_info VALUES (2,'alice','','invalid'),(7,'bob','','invalid'),(9,'carol','','invalid')"));
    }
    void cleanup()
    {
        auto &server = MyTcpServer::getInstance();
        for (auto *s : server.findChildren<MyTcpSocket *>()) s->abort();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        server.close();
    }
    void headerBoundaries()
    {
        QVERIFY(Security::validHeader(sizeof(PDU), 0));
        QVERIFY(Security::validHeader(Security::MaxFrame, Security::MaxFrame - sizeof(PDU)));
        QVERIFY(!Security::validHeader(sizeof(PDU) - 1, 0));
        QVERIFY(!Security::validHeader(sizeof(PDU), 1));
        QVERIFY(!Security::validHeader(Security::MaxFrame + 1, Security::MaxFrame + 1 - sizeof(PDU)));
        QVERIFY(!Security::validHeader(0, std::numeric_limits<uint>::max()));
    }
    void requestValidation()
    {
        auto p = packet(ENUM_MSG_TYPE_REGIST_REQUEST);
        Security::putField(p->caData, "alice"); Security::putField(p->caData + 32, "secret");
        QVERIFY(Security::validRequest(p.get()));
        memset(p->caData, 'a', 32); QVERIFY(!Security::validRequest(p.get()));
        Security::putField(p->caData, "../bob"); QVERIFY(!Security::validRequest(p.get()));
        auto path = packet(ENUM_MSG_TYPE_DEL_FILE_REQUEST, QByteArray("abc"));
        QVERIFY(!Security::validRequest(path.get()));
        path = packet(ENUM_MSG_TYPE_DEL_FILE_REQUEST, QByteArray("a\0b\0", 4));
        QVERIFY(!Security::validRequest(path.get()));
        auto chunk = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST, QByteArray(65536, 'x'));
        QVERIFY(Security::validRequest(chunk.get()));
        chunk = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST, QByteArray(65537, 'x'));
        QVERIFY(!Security::validRequest(chunk.get()));
        chunk = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST); QVERIFY(!Security::validRequest(chunk.get()));
        auto share = packet(ENUM_MSG_TYPE_SHARE_FILE_REQUEST, textBody(alice));
        int count = 257; memcpy(share->caData + 32, &count, sizeof(count));
        QVERIFY(!Security::validRequest(share.get()));
        auto unknown = packet(123456); QVERIFY(!Security::validRequest(unknown.get()));
    }
    void namesAndIsolation()
    {
        for (const QString &name : QStringList{"", ".", "..", "a/b", "a\\b", "NUL", "CON.txt", "a.", "a ", QString::fromLatin1("a\0b", 3)})
            QVERIFY2(!Security::validName(name), qPrintable(name));
        QVERIFY(Security::validName("alice"));
        QVERIFY(Security::userRoot(root->path(), QString(32, 'a'), true).isEmpty());
        QCOMPARE(Security::resolvePath(root->path(), "alice", alice), alice);
        QCOMPARE(Security::resolvePath(root->path(), "alice", alice + "/new.txt", false), alice + "/new.txt");
        QVERIFY(Security::resolvePath(root->path(), "alice", alice, false).isEmpty());
        for (const QString &path : QStringList{root->path(), bob, alice + "/../bob", alice + "-other/file", alice + "/NUL"})
            QVERIFY(Security::resolvePath(root->path(), "alice", path).isEmpty());
    }
    void reparsePointIsolation()
    {
#ifdef Q_OS_WIN
        const QString link = alice + "/escape";
        auto createLink = reinterpret_cast<BOOLEAN (WINAPI *)(LPCWSTR,LPCWSTR,DWORD)>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateSymbolicLinkW"));
        if (!createLink) QSKIP("CreateSymbolicLinkW unavailable");
        const QString nativeLink = QDir::toNativeSeparators(link), target = QDir::toNativeSeparators(bob);
        if (!createLink(reinterpret_cast<LPCWSTR>(nativeLink.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()), 3)
            && !createLink(reinterpret_cast<LPCWSTR>(nativeLink.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()), 1))
            QSKIP("Directory symlink privilege/developer mode unavailable");
        // Remove the link before temporary-directory cleanup, even on assertion failure.
        struct Unlink { QString p; ~Unlink() { RemoveDirectoryW(reinterpret_cast<LPCWSTR>(p.utf16())); } } guard{nativeLink};
        QVERIFY(writeFile(bob + "/secret", "secret"));
        QVERIFY(Security::resolvePath(root->path(), "alice", link + "/secret").isEmpty());
        QVERIFY(!Security::safeTree(alice));
        MsgHandler h; h.actor = "alice";
        auto p = packet(ENUM_MSG_TYPE_DEL_FILE_REQUEST, textBody(link));
        QVERIFY(!succeeded(call(h, p, &MsgHandler::delFile)));
        QCOMPARE(readFile(bob + "/secret"), QByteArray("secret"));
#else
        QSKIP("Windows reparse-point regression");
#endif
    }
    void exactHomeNamesAndShortAliases()
    {
        QVERIFY(Security::userRoot(root->path(), "ALICE", false).isEmpty());
        QVERIFY(Security::userRoot(root->path(), "ALICE", true).isEmpty());
        QCOMPARE(Security::userRoot(root->path(), "alice"), alice);
#ifdef Q_OS_WIN
        const QString home = Security::userRoot(root->path(), "LongAccountDirectory", true);
        QVERIFY(!home.isEmpty());
        const QString native = QDir::toNativeSeparators(home);
        wchar_t buffer[32768] = {};
        const DWORD length = GetShortPathNameW(reinterpret_cast<LPCWSTR>(native.utf16()), buffer, 32768);
        if (!length || length >= 32768) QSKIP("NTFS short path unavailable; exact case assertions passed");
        const QString alias = QFileInfo(QDir::fromNativeSeparators(QString::fromWCharArray(buffer))).fileName();
        if (alias == "LongAccountDirectory") QSKIP("Volume has no distinct 8.3 alias; exact case assertions passed");
        QVERIFY(Security::userRoot(root->path(), alias, false).isEmpty());
        QVERIFY(Security::userRoot(root->path(), alias, true).isEmpty());
#endif
    }
    void uploadCommitAndDownload()
    {
        MsgHandler h; h.actor = "alice";
        QVERIFY(succeeded(upload(h, alice, "data", 6)));
        auto first = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST, "abc");
        QVERIFY(!call(h, first, &MsgHandler::uploadFileData));
        QVERIFY(!QFile::exists(alice + "/data"));
        auto last = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST, "def");
        QVERIFY(succeeded(call(h, last, &MsgHandler::uploadFileData)));
        QCOMPARE(readFile(alice + "/data"), QByteArray("abcdef"));
        auto get = packet(ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST, textBody(alice + "/data"));
        QVERIFY(succeeded(call(h, get, &MsgHandler::downloadFile)));
        auto next = packet(ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_REQUEST);
        auto bytes = call(h, next, &MsgHandler::downloadFileData);
        QCOMPARE(bytes->uiType, uint(ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_RESPOND));
        QCOMPARE(QByteArray(bytes->caMsg, bytes->uiMsgLen), QByteArray("abcdef"));
        auto end = call(h, next, &MsgHandler::downloadFileData);
        QCOMPARE(end->uiType, uint(ENUM_MSG_TYPE_DOWNLOAD_FILE_FINISH_RESPOND));
        QVERIFY(succeeded(end)); QVERIFY(!h.m_fDownloadFile.isOpen());
    }
    void uploadAbortPreservesOriginal()
    {
        QVERIFY(writeFile(alice + "/data", "original"));
        {
            MsgHandler h; h.actor = "alice";
            QVERIFY(succeeded(upload(h, alice, "data", 6)));
            auto chunk = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST, "abc");
            QVERIFY(!call(h, chunk, &MsgHandler::uploadFileData));
            QCOMPARE(readFile(alice + "/data"), QByteArray("original"));
        }
        QCOMPARE(readFile(alice + "/data"), QByteArray("original"));
        MsgHandler h; h.actor = "alice";
        QVERIFY(succeeded(upload(h, alice, "data", 2)));
        auto tooMuch = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST, "abc");
        QVERIFY(!succeeded(call(h, tooMuch, &MsgHandler::uploadFileData)));
        QVERIFY(!h.m_fUploadFile);
        QCOMPARE(readFile(alice + "/data"), QByteArray("original"));
    }
    void uploadLimitsAndIsolation()
    {
        MsgHandler h; h.actor = "alice";
        QVERIFY(!succeeded(upload(h, alice, "data", -1)));
        QVERIFY(!succeeded(upload(h, alice, "data", Security::MaxFileSize + 1)));
        QVERIFY(!succeeded(upload(h, bob, "data", 0)));
        QVERIFY(!QFile::exists(bob + "/data"));
        QVERIFY(succeeded(upload(h, alice, "empty", 0)));
        QVERIFY(QFile::exists(alice + "/empty")); QCOMPARE(QFileInfo(alice + "/empty").size(), qint64(0));
        QVERIFY(succeeded(upload(h, alice, "pending", 5)));
        QVERIFY(succeeded(upload(h, alice, "second", 0)));
        QVERIFY(QFile::exists(alice + "/second"));
        QVERIFY(!QFile::exists(alice + "/pending"));
    }
    void uploadRestartAndExpiry()
    {
        QVERIFY(writeFile(alice + "/old", "original"));
        MsgHandler h; h.actor = "alice";
        QVERIFY(succeeded(upload(h, alice, "old", 6)));
        auto part = packet(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST, "abc");
        QVERIFY(!call(h, part, &MsgHandler::uploadFileData));
        QVERIFY(succeeded(upload(h, alice, "replacement", 6)));
        QCOMPARE(readFile(alice + "/old"), QByteArray("original"));
        QCOMPARE(h.m_iUploadReceivedSize, qint64(0));
        QVERIFY(!call(h, part, &MsgHandler::uploadFileData));
        h.expireUpload(); QVERIFY(h.m_fUploadFile);
        // A fresh init is the deterministic cancellation path; the same helper
        // is also called by the server's periodic 30-second expiry check.
        QVERIFY(succeeded(upload(h, alice, "empty", 0)));
        QVERIFY(!h.m_fUploadFile);
        QVERIFY(!QFile::exists(alice + "/replacement"));
        QCOMPARE(readFile(alice + "/old"), QByteArray("original"));
    }
    void fileRenameListDelete()
    {
        MsgHandler h; h.actor = "alice";
        auto create = packet(ENUM_MSG_TYPE_CREATE_FILE_REQUEST, textBody(alice));
        Security::putField(create->caData, "folder");
        QVERIFY(succeeded(call(h, create, &MsgHandler::createFile)));
        QVERIFY(writeFile(alice + "/old", "payload"));
        auto rename = packet(ENUM_MSG_TYPE_RENAME_FILE_REQUEST, textBody(alice + "/old") + textBody(alice + "/new"));
        QVERIFY(Security::validRequest(rename.get()));
        QVERIFY(succeeded(call(h, rename, &MsgHandler::renameFile)));
        QVERIFY(!QFile::exists(alice + "/old")); QCOMPARE(readFile(alice + "/new"), QByteArray("payload"));
        auto list = packet(ENUM_MSG_TYPE_FLUSH_FILE_REQUEST, textBody(alice));
        auto result = call(h, list, &MsgHandler::flushFile);
        QCOMPARE(result->uiMsgLen, uint(2 * sizeof(FileInfo)));
        QMap<QString, uint> entries;
        for (uint i = 0; i < result->uiMsgLen / sizeof(FileInfo); ++i) {
            const auto *entry = reinterpret_cast<FileInfo *>(result->caMsg) + i;
            entries.insert(Security::field(entry->caName), entry->uiType);
        }
        QCOMPARE(entries.value("folder", 99), uint(0)); QCOMPARE(entries.value("new", 99), uint(1));
        auto del = packet(ENUM_MSG_TYPE_DEL_FILE_REQUEST, textBody(alice + "/new"));
        QVERIFY(succeeded(call(h, del, &MsgHandler::delFile))); QVERIFY(!QFile::exists(alice + "/new"));
    }
    void fileOperationsRejectOtherAndRoot()
    {
        QVERIFY(writeFile(bob + "/secret", "private"));
        QVERIFY(writeFile(alice + "/mine", "mine"));
        MsgHandler h; h.actor = "alice";
        for (const QString &path : QStringList{bob + "/secret", root->path(), alice}) {
            auto p = packet(ENUM_MSG_TYPE_DEL_FILE_REQUEST, textBody(path));
            QVERIFY(!succeeded(call(h, p, &MsgHandler::delFile)));
            p = packet(ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST, textBody(path));
            QVERIFY(!succeeded(call(h, p, &MsgHandler::downloadFile)));
        }
        auto list = packet(ENUM_MSG_TYPE_FLUSH_FILE_REQUEST, textBody(bob));
        QCOMPARE(call(h, list, &MsgHandler::flushFile)->uiMsgLen, uint(0));
        auto rename = packet(ENUM_MSG_TYPE_RENAME_FILE_REQUEST, textBody(alice + "/mine") + textBody(bob + "/stolen"));
        QVERIFY(!succeeded(call(h, rename, &MsgHandler::renameFile)));
        QCOMPARE(readFile(alice + "/mine"), QByteArray("mine")); QCOMPARE(readFile(bob + "/secret"), QByteArray("private"));
    }
    void bogusShareAcceptance()
    {
        QVERIFY(writeFile(bob + "/secret", "private"));
        MsgHandler h; h.actor = "alice";
        auto p = packet(ENUM_MSG_TYPE_SHARE_FILE_RESPOND, textBody(bob + "/secret"));
        Security::putField(p->caData, "bob"); int yes = 1; memcpy(p->caData + 32, &yes, sizeof(yes));
        QVERIFY(Security::validRequest(p.get()));
        QVERIFY(!succeeded(call(h, p, &MsgHandler::shareFileAgree)));
        QVERIFY(!QFile::exists(alice + "/secret")); QCOMPARE(readFile(bob + "/secret"), QByteArray("private"));
    }
    void friendReturnCodesAndIds()
    {
        auto &db = OperateDB::getInstance();
        QCOMPARE(db.handleFindUser("alice"), 1); QCOMPARE(db.handleFindUser("bob"), 1);
        QCOMPARE(db.handleFindUser("missing"), 2);
        QCOMPARE(db.handleAddFriend("alice", "bob"), 1);
        // Account names are exact on the wire; a case variant is not an alias.
        QCOMPARE(db.handleAddFriend("alice", "ALICE"), -1);
        QCOMPARE(db.handleAddFriend("alice", "missing"), -1);
        QVERIFY(db.handleAddFriendAgree("alice", "bob"));
        QCOMPARE(db.handleAddFriend("bob", "alice"), -2);
        QVERIFY(!db.handleAddFriendAgree("bob", "alice"));
        QCOMPARE(db.isFriend("bob", "alice"), 1);
        QCOMPARE(db.handleFlushFriend("alice"), QStringList{"bob"});
        QVERIFY(db.handleDeleteFriend("bob", "alice")); QCOMPARE(db.isFriend("alice", "bob"), 0);
    }
    void pendingFriendRequiresIntendedActor()
    {
        auto &presence = PresenceStore::getInstance();
        presence.login("bob", "security-bob");
        MsgHandler sender; sender.actor = "alice";
        auto invite = packet(ENUM_MSG_TYPE_ADD_FRIEND_REQUEST);
        Security::putField(invite->caData, "carol"); Security::putField(invite->caData + 32, "bob");
        QVERIFY(!call(sender, invite, &MsgHandler::addFriend));
        QCOMPARE(Security::field(invite->caData), QString("alice"));
        MsgHandler receiver; receiver.actor = "carol";
        auto accept = packet(ENUM_MSG_TYPE_ADD_FRIEND_AGREE_REQUEST); Security::putField(accept->caData, "alice");
        QVERIFY(!succeeded(call(receiver, accept, &MsgHandler::addfriendAgree)));
        QCOMPARE(OperateDB::getInstance().isFriend("alice", "carol"), 0);
        receiver.actor = "bob";
        QVERIFY(succeeded(call(receiver, accept, &MsgHandler::addfriendAgree)));
        QCOMPARE(OperateDB::getInstance().isFriend("alice", "bob"), 1);
        QVERIFY(!succeeded(call(receiver, accept, &MsgHandler::addfriendAgree)));
        presence.logout("bob", "security-bob");
    }
    void sqlInjectionIsBound()
    {
        auto &db = OperateDB::getInstance();
        QVERIFY(db.handleAddFriendAgree("alice", "bob"));
        for (const QByteArray &attack : QList<QByteArray>{"' OR 1=1 --", "'; DROP TABLE user_info; --", "alice' UNION SELECT 7 --"}) {
            QCOMPARE(db.handleFindUser(attack.constData()), 2);
            QVERIFY(!db.handleLogin(attack.constData(), "anything"));
            QCOMPARE(db.handleAddFriend("alice", attack.constData()), -1);
            QVERIFY(!db.handleAddFriendAgree("alice", attack.constData()));
            QVERIFY(!db.handleDeleteFriend(attack.constData(), "bob"));
            QVERIFY(db.handleFlushFriend(attack.constData()).isEmpty());
            QCOMPARE(db.isFriend("alice", "bob"), 1);
        }
        // Registration also binds values: punctuation is stored literally, never executed.
        const char *literal = "x'; DROP TABLE friend; --";
        QVERIFY(db.handleRegist(literal, "secret")); QCOMPARE(db.handleFindUser(literal), 1);
        QSqlQuery q(db.m_db); QVERIFY(q.exec("SELECT COUNT(*) FROM user_info")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 4);
        QVERIFY(q.exec("SELECT COUNT(*) FROM friend")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 1);
    }
    void hashedLoginAndCaseCollision()
    {
        auto &db = OperateDB::getInstance();
        QVERIFY(db.handleRegist("dave", "correct"));
        QVERIFY(db.handleLogin("dave", "correct")); QVERIFY(!db.handleLogin("dave", "wrong"));
        QVERIFY(!db.handleLogin("DAVE", "correct")); QVERIFY(!db.handleRegist("DAVE", "other"));
        QSqlQuery q(db.m_db); QVERIFY(q.exec("SELECT pwd,password_hash FROM user_info WHERE name='dave'")); QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QString()); QVERIFY(PasswordHash::verify("correct", q.value(1).toString()));
        QVERIFY(q.exec("UPDATE user_info SET pwd='plaintext',password_hash='' WHERE name='alice'"));
        QVERIFY(!db.handleLogin("alice", "plaintext"));
    }
    void actorOverridesSpoofedFriendName()
    {
        auto &db = OperateDB::getInstance();
        QVERIFY(db.handleAddFriendAgree("alice", "bob")); QVERIFY(db.handleAddFriendAgree("bob", "carol"));
        MyTcpSocket socket; socket.m_authenticated = true; socket.m_strLoginName = "alice";
        auto p = packet(ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST); Security::putField(p->caData, "bob");
        Packet reply(socket.handleMsg(p.get()), &free); QVERIFY(reply);
        QCOMPARE(reply->uiMsgLen, uint(32)); QCOMPARE(Security::field(reply->caMsg), QString("bob"));
        p = packet(ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST); Security::putField(p->caData, "bob"); Security::putField(p->caData + 32, "carol");
        reply.reset(socket.handleMsg(p.get())); QVERIFY(reply);
        QCOMPARE(db.isFriend("bob", "carol"), 1); QCOMPARE(db.isFriend("alice", "bob"), 1);
        Security::putField(p->caData + 32, "bob"); reply.reset(socket.handleMsg(p.get())); QVERIFY(succeeded(reply));
        QCOMPARE(db.isFriend("alice", "bob"), 0); QCOMPARE(db.isFriend("bob", "carol"), 1);
    }
    void captchaIsRequiredAndSingleUse()
    {
        MyTcpSocket socket;
        QVERIFY(!socket.verifyCaptcha(""));
        socket.generateCaptcha(); const QString first = socket.m_captchaText; QVERIFY(!first.isEmpty());
        QVERIFY(socket.verifyCaptcha(first.toLower())); QVERIFY(!socket.verifyCaptcha(first));
        socket.generateCaptcha(); const QString second = socket.m_captchaText;
        QVERIFY(!socket.verifyCaptcha("")); QVERIFY(!socket.verifyCaptcha(second));
        auto login = packet(ENUM_MSG_TYPE_LOGIN_WITH_CAPTCHA_REQUEST, textBody("bogus"));
        Security::putField(login->caData, "alice"); Security::putField(login->caData + 32, "secret");
        Packet reply(socket.handleMsg(login.get()), &free); QVERIFY(reply);
        int result = 0; memcpy(&result, reply->caData, sizeof(result)); QCOMPARE(result, -1); QVERIFY(!socket.m_authenticated);
    }
    void tcpFragmentedAndCoalesced()
    {
        auto &server = MyTcpServer::getInstance(); QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(server.findChildren<MyTcpSocket *>().size(), 1);
        auto *peer = server.findChild<MyTcpSocket *>(); peer->m_authenticated = true; peer->m_strLoginName = "alice";
        const auto heartbeat = packet(ENUM_MSG_TYPE_HEARTBEAT_REQUEST); const QByteArray bytes = wire(heartbeat);
        QCOMPARE(client.write(bytes.left(5)), qint64(5)); client.flush();
        QTRY_COMPARE(peer->buffer.size(), 5); QCOMPARE(client.bytesAvailable(), qint64(0));
        // Complete one fragmented frame and append two complete frames in a single write.
        const QByteArray tail = bytes.mid(5) + bytes + bytes;
        QCOMPARE(client.write(tail), qint64(tail.size())); client.flush();
        QTRY_COMPARE(client.bytesAvailable(), qint64(3 * sizeof(PDU)));
        const QByteArray replies = client.readAll();
        for (int i = 0; i < 3; ++i) {
            PDU header = {}; memcpy(&header, replies.constData() + i * sizeof(PDU), sizeof(PDU));
            QCOMPARE(header.uiType, uint(ENUM_MSG_TYPE_HEARTBEAT_RESPOND)); QVERIFY(Security::validHeader(header.uiTotalLen, header.uiMsgLen));
        }
        QCOMPARE(peer->buffer.size(), 0);
    }
    void tcpRejectsMalformedAndUnauthorized_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("short-total") << 0; QTest::newRow("payload-mismatch") << 1;
        QTest::newRow("oversized") << 2; QTest::newRow("invalid-request-body") << 3;
        QTest::newRow("unauth-business") << 4; QTest::newRow("legacy-login") << 5;
    }
    void tcpRejectsMalformedAndUnauthorized()
    {
        QFETCH(int, kind);
        auto &server = MyTcpServer::getInstance(); QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(server.findChildren<MyTcpSocket *>().size(), 1);
        auto *peer = server.findChild<MyTcpSocket *>();
        peer->m_authenticated = kind < 4; peer->m_strLoginName = kind < 4 ? "alice" : "";
        auto p = packet(kind == 3 ? ENUM_MSG_TYPE_HEARTBEAT_REQUEST : ENUM_MSG_TYPE_DEL_FILE_REQUEST,
                        kind == 3 ? QByteArray("x") : textBody(alice + "/victim"));
        QVERIFY(writeFile(alice + "/victim", "keep"));
        if (kind == 5) {
            p = packet(ENUM_MSG_TYPE_LOGIN_REQUEST); Security::putField(p->caData, "alice"); Security::putField(p->caData + 32, "secret");
        }
        QByteArray bytes = wire(p);
        uint value = 0;
        if (kind == 0) { value = sizeof(PDU) - 1; memcpy(bytes.data(), &value, sizeof(value)); }
        if (kind == 1) { value = 999; memcpy(bytes.data() + sizeof(uint), &value, sizeof(value)); }
        if (kind == 2) { value = Security::MaxFrame + 1; memcpy(bytes.data(), &value, sizeof(value)); }
        QCOMPARE(client.write(bytes), qint64(bytes.size())); client.flush();
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(client.readAll().size(), 0); QCOMPARE(readFile(alice + "/victim"), QByteArray("keep"));
    }
};

QTEST_MAIN(SecurityTest)
#include "security_test.moc"
