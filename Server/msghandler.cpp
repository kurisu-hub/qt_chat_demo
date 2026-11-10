#include "msghandler.h"
#include "mytcpserver.h"
#include "operatedb.h"
#include "presencestore.h"
#include "security.h"
#include "server.h"
#include <QDir>
#include <QDateTime>
#include <cstring>

namespace {
PDU *response(uint type, bool ok)
{
    PDU *p = mkPDU(); p->uiType = type;
    memcpy(p->caData, &ok, sizeof(ok)); return p;
}
PDU *numberResponse(uint type, int result)
{
    PDU *p = mkPDU(); p->uiType = type;
    memcpy(p->caData, &result, sizeof(result)); return p;
}
PDU *namesResponse(uint type, const QStringList &names)
{
    const int count = qMin(names.size(), int((Security::MaxFrame - sizeof(PDU)) / 32));
    PDU *p = mkPDU(count * 32); p->uiType = type;
    for (int i = 0; i < count; ++i) Security::putField(p->caMsg + i * 32, names[i]);
    return p;
}
void forward(const QString &user, PDU *p)
{
    QByteArray target = user.toUtf8();
    MyTcpServer::getInstance().resend(target.data(), p);
}
QString pathFor(const QString &actor, const QString &input, bool rootAllowed = true)
{
    return Security::resolvePath(Server::getInstance().m_strRootPath, actor, input, rootAllowed);
}
QString body(const PDU *p) { return QString::fromUtf8(p->caMsg); }
bool friends(const QString &a, const QString &b)
{
    return OperateDB::getInstance().isFriend(a.toUtf8().constData(), b.toUtf8().constData()) == 1;
}
struct Invitation { QString sender; QString receiver; QString path; qint64 expires; };
QList<Invitation> pendingFriends;
QList<Invitation> pendingShares;
void prune(QList<Invitation> &items)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = items.size() - 1; i >= 0; --i) if (items[i].expires <= now) items.removeAt(i);
}
bool remember(QList<Invitation> &items, const QString &sender, const QString &receiver, const QString &path = {})
{
    prune(items);
    for (int i = items.size() - 1; i >= 0; --i) {
        if (items[i].sender == sender && items[i].receiver == receiver && items[i].path == path) items.removeAt(i);
    }
    if (items.size() >= 4096) return false;
    items.append({sender, receiver, path, QDateTime::currentMSecsSinceEpoch() + 300000});
    return true;
}
bool take(QList<Invitation> &items, const QString &receiver, const QString &key, Invitation &result, bool share)
{
    prune(items);
    for (int i = 0; i < items.size(); ++i) {
        if (items[i].receiver == receiver && (share ? items[i].path : items[i].sender) == key) {
            result = items.takeAt(i); return true;
        }
    }
    return false;
}
}
MsgHandler::MsgHandler() = default;
void MsgHandler::expireUpload()
{
    if (m_fUploadFile && m_uploadActivity.isValid() && m_uploadActivity.elapsed() > 30000)
        m_fUploadFile.reset();
}
PDU *MsgHandler::regist()
{
    const QString user = Security::field(pdu->caData);
    const bool ok = OperateDB::getInstance().handleRegist(pdu->caData, pdu->caData + 32);
    if (ok) Security::userRoot(Server::getInstance().m_strRootPath, user, true);
    return response(ENUM_MSG_TYPE_REGIST_RESPOND, ok);
}
PDU *MsgHandler::findUser()
{
    int result = OperateDB::getInstance().handleFindUser(pdu->caData);
    if (result == 1) result = PresenceStore::getInstance().isOnline(Security::field(pdu->caData)) ? 1 : 0;
    return numberResponse(ENUM_MSG_TYPE_FIND_USER_RESPOND, result);
}
PDU *MsgHandler::onlineUser()
{
    return namesResponse(ENUM_MSG_TYPE_ONLINE_USER_RESPOND, PresenceStore::getInstance().onlineUsers());
}
PDU *MsgHandler::addFriend()
{
    const QString target = Security::field(pdu->caData + 32);
    int result = OperateDB::getInstance().handleAddFriend(actor.toUtf8().constData(), target.toUtf8().constData());
    if (result == 1 && (!PresenceStore::getInstance().isOnline(target) || !remember(pendingFriends, actor, target))) result = -1;
    if (result == 1) {
        Security::putField(pdu->caData, actor); forward(target, pdu); return nullptr;
    }
    return numberResponse(ENUM_MSG_TYPE_ADD_FRIEND_RESPOND, result);
}
PDU *MsgHandler::addfriendAgree()
{
    const QString sender = Security::field(pdu->caData);
    Invitation invitation;
    const bool pending = take(pendingFriends, actor, sender, invitation, false);
    const bool ok = pending && OperateDB::getInstance().handleAddFriendAgree(sender.toUtf8().constData(), actor.toUtf8().constData());
    PDU *reply = response(ENUM_MSG_TYPE_ADD_FRIEND_AGREE_RESPOND, ok);
    if (pending) forward(sender, reply);
    return reply;
}
PDU *MsgHandler::flushFriend()
{
    return namesResponse(ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND,
        OperateDB::getInstance().handleFlushFriend(actor.toUtf8().constData()));
}
PDU *MsgHandler::deleteFriend()
{
    return response(ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND,
        OperateDB::getInstance().handleDeleteFriend(actor.toUtf8().constData(), pdu->caData + 32));
}
PDU *MsgHandler::chat()
{
    const QString target = Security::field(pdu->caData + 32);
    if (friends(actor, target)) {
        Security::putField(pdu->caData, actor); forward(target, pdu); return nullptr;
    }
    return numberResponse(ENUM_MSG_TYPE_CHAT_REQUEST, 0);
}
PDU *MsgHandler::createFile()
{
    const QString path = pathFor(actor, body(pdu) + '/' + Security::field(pdu->caData), false);
    return response(ENUM_MSG_TYPE_CREATE_FILE_RESPOND, !path.isEmpty() && QDir().mkdir(path));
}
PDU *MsgHandler::flushFile()
{
    const QString path = pathFor(actor, body(pdu));
    QFileInfoList entries;
    if (!path.isEmpty() && QFileInfo(path).isDir()) {
        for (const QFileInfo &entry : QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (entry.fileName().toUtf8().size() <= 31 && !pathFor(actor, entry.absoluteFilePath()).isEmpty()) entries.append(entry);
            if (entries.size() >= int((Security::MaxFrame - sizeof(PDU)) / sizeof(FileInfo))) break;
        }
    }
    PDU *reply = mkPDU(entries.size() * sizeof(FileInfo)); reply->uiType = ENUM_MSG_TYPE_FLUSH_FILE_RESPOND;
    for (int i = 0; i < entries.size(); ++i) {
        FileInfo *info = reinterpret_cast<FileInfo *>(reply->caMsg) + i;
        Security::putField(info->caName, entries[i].fileName()); info->uiType = entries[i].isDir() ? 0 : 1;
    }
    return reply;
}
PDU *MsgHandler::delFile()
{
    const QString path = pathFor(actor, body(pdu), false);
    bool ok = !path.isEmpty() && Security::safeTree(path);
    if (ok) ok = QFileInfo(path).isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
    return response(ENUM_MSG_TYPE_DEL_FILE_RESPOND, ok);
}
PDU *MsgHandler::renameFile()
{
    const QString oldInput = pdu->uiMsgLen ? body(pdu) : Security::field(pdu->caData);
    const QString newInput = pdu->uiMsgLen ? QString::fromUtf8(pdu->caMsg + strlen(pdu->caMsg) + 1) : Security::field(pdu->caData + 32);
    const QString from = pathFor(actor, oldInput, false), to = pathFor(actor, newInput, false);
    return response(ENUM_MSG_TYPE_RENAME_FILE_RESPOND,
        !from.isEmpty() && !to.isEmpty() && Security::safeTree(from) && QDir().rename(from, to));
}
PDU *MsgHandler::uploadFileInit()
{
    // A new init explicitly abandons an incomplete transfer on this connection.
    // QSaveFile destruction cancels its temporary data and preserves the old target.
    m_fUploadFile.reset();
    qint64 size = 0; memcpy(&size, pdu->caData + 32, sizeof(size));
    const QString path = pathFor(actor, body(pdu) + '/' + Security::field(pdu->caData), false);
    bool ok = !path.isEmpty() && size >= 0 && size <= Security::MaxFileSize;
    if (ok) {
        m_fUploadFile.reset(new QSaveFile(path)); m_fUploadFile->setDirectWriteFallback(false);
        ok = m_fUploadFile->open(QIODevice::WriteOnly);
        m_iUploadFileSize = size; m_iUploadReceivedSize = 0;
        m_uploadActivity.start();
    }
    if (!ok) m_fUploadFile.reset();
    if (ok && size == 0) { ok = m_fUploadFile->commit(); m_fUploadFile.reset(); }
    return response(ENUM_MSG_TYPE_UPLOAD_FILE_INIT_RESPOND, ok);
}
PDU *MsgHandler::uploadFileData()
{
    expireUpload();
    if (!m_fUploadFile || qint64(pdu->uiMsgLen) > m_iUploadFileSize - m_iUploadReceivedSize) {
        m_fUploadFile.reset(); return response(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_RESPOND, false);
    }
    if (m_fUploadFile->write(pdu->caMsg, pdu->uiMsgLen) != pdu->uiMsgLen) {
        m_fUploadFile.reset(); return response(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_RESPOND, false);
    }
    m_iUploadReceivedSize += pdu->uiMsgLen;
    m_uploadActivity.start();
    if (m_iUploadReceivedSize < m_iUploadFileSize) return nullptr;
    const bool ok = !pathFor(actor, m_fUploadFile->fileName(), false).isEmpty() && m_fUploadFile->commit();
    m_fUploadFile.reset(); return response(ENUM_MSG_TYPE_UPLOAD_FILE_DATA_RESPOND, ok);
}
PDU *MsgHandler::downloadFile()
{
    const QString path = pathFor(actor, body(pdu), false);
    m_fDownloadFile.close();
    bool ok = !path.isEmpty() && QFileInfo(path).isFile();
    if (ok) { m_fDownloadFile.setFileName(path); ok = m_fDownloadFile.open(QIODevice::ReadOnly); }
    return response(ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND, ok);
}
PDU *MsgHandler::downloadFileData()
{
    if (!m_fDownloadFile.isOpen()) return response(ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND, false);
    const QByteArray bytes = m_fDownloadFile.read(4096);
    if (bytes.isEmpty()) {
        const bool ok = m_fDownloadFile.error() == QFile::NoError; m_fDownloadFile.close();
        return response(ok ? ENUM_MSG_TYPE_DOWNLOAD_FILE_FINISH_RESPOND : ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND, ok);
    }
    PDU *reply = mkPDU(bytes.size()); reply->uiType = ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_RESPOND;
    memcpy(reply->caMsg, bytes.constData(), bytes.size()); return reply;
}
PDU *MsgHandler::shareFile()
{
    int count = 0; memcpy(&count, pdu->caData + 32, sizeof(count));
    const QString source = pathFor(actor, QString::fromUtf8(pdu->caMsg + count * 32), false);
    if (source.isEmpty() || !QFileInfo(source).isFile()) return response(ENUM_MSG_TYPE_SHARE_FILE_RESPOND, false);
    const QByteArray bytes = source.toUtf8();
    PDU *notice = mkPDU(bytes.size() + 1); notice->uiType = ENUM_MSG_TYPE_SHARE_FILE_REQUEST;
    Security::putField(notice->caData, actor); memcpy(notice->caMsg, bytes.constData(), bytes.size());
    bool sent = false;
    for (int i = 0; i < count; ++i) {
        const QString target = Security::field(pdu->caMsg + i * 32);
        if (friends(actor, target) && PresenceStore::getInstance().isOnline(target) && remember(pendingShares, actor, target, source)) {
            forward(target, notice); sent = true;
        }
    }
    free(notice); return sent ? nullptr : response(ENUM_MSG_TYPE_SHARE_FILE_RESPOND, false);
}
PDU *MsgHandler::shareFileAgree()
{
    Invitation invitation;
    if (!take(pendingShares, actor, body(pdu), invitation, true)) return response(ENUM_MSG_TYPE_SHARE_FILE_RESPOND, false);
    int accepted = 0; memcpy(&accepted, pdu->caData + 32, sizeof(accepted));
    const QString source = pathFor(invitation.sender, invitation.path, false);
    const QString home = Security::userRoot(Server::getInstance().m_strRootPath, actor);
    const QString destination = pathFor(actor, home + '/' + QFileInfo(source).fileName(), false);
    bool ok = accepted == 1 && friends(invitation.sender, actor) && !source.isEmpty()
        && !destination.isEmpty() && QFileInfo(source).isFile() && source != destination;
    if (ok) {
        QFile input(source); QSaveFile output(destination); output.setDirectWriteFallback(false);
        ok = input.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly);
        while (ok && !input.atEnd()) {
            const QByteArray bytes = input.read(64 * 1024);
            if (bytes.isEmpty() || output.write(bytes) != bytes.size()) ok = false;
        }
        ok = ok && input.error() == QFile::NoError && output.commit();
    }
    PDU *reply = response(ENUM_MSG_TYPE_SHARE_FILE_RESPOND, ok);
    forward(invitation.sender, reply); free(reply); return nullptr;
}
