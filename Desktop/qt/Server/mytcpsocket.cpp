#include "mytcpsocket.h"
#include "operatedb.h"
#include "mytcpserver.h"
#include "presencestore.h"
#include "captchacode.h"
#include "security.h"
#include "server.h"
#include <QUuid>
#include <QBuffer>
#include <QScopedValueRollback>
#include <memory>
#include <cstring>

MyTcpSocket::MyTcpSocket()
{
    connect(this, &QTcpSocket::readyRead, this, &MyTcpSocket::recvMsg);
    connect(this, &QTcpSocket::disconnected, this, &MyTcpSocket::clientOffline);
    setReadBufferSize(Security::MaxFrame);
    m_pmh = new MsgHandler;
    updateActiveTime();
}
MyTcpSocket::~MyTcpSocket() { delete m_pmh; }

PDU *MyTcpSocket::handleMsg(PDU *pdu)
{
    if (!Security::validRequest(pdu)) { abort(); return nullptr; }
    const bool publicRequest = pdu->uiType == ENUM_MSG_TYPE_CAPTCHA_REQUEST
        || pdu->uiType == ENUM_MSG_TYPE_REGIST_REQUEST
        || pdu->uiType == ENUM_MSG_TYPE_LOGIN_WITH_CAPTCHA_REQUEST;
    // Authentication belongs to this connection, never to a name in the packet.
    // Re-authentication on a live session would leave stale presence entries.
    if ((!m_authenticated && !publicRequest) || (m_authenticated && publicRequest)) {
        abort(); return nullptr;
    }
    if (publicRequest) {
        QElapsedTimer &last = pdu->uiType == ENUM_MSG_TYPE_CAPTCHA_REQUEST ? m_lastCaptchaRequest : m_lastAuthRequest;
        if (last.isValid() && last.elapsed() < 1000) { abort(); return nullptr; }
        last.start();
    }
    m_pmh->pdu = pdu;
    m_pmh->actor = m_strLoginName;
    switch (pdu->uiType) {
    case ENUM_MSG_TYPE_CAPTCHA_REQUEST: {
        generateCaptcha();
        const QPixmap pixmap = CaptchaCode::drawCaptcha(m_captchaText);
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        pixmap.save(&buffer, "PNG");
        PDU *reply = mkPDU(bytes.size()); reply->uiType = ENUM_MSG_TYPE_CAPTCHA_RESPOND;
        memcpy(reply->caMsg, bytes.constData(), bytes.size()); return reply;
    }
    case ENUM_MSG_TYPE_LOGIN_WITH_CAPTCHA_REQUEST: {
        int result = -1;
        if (verifyCaptcha(QString::fromUtf8(pdu->caMsg))) {
            result = 0;
            const QString user = Security::field(pdu->caData);
            if (OperateDB::getInstance().handleLogin(pdu->caData, pdu->caData + 32)
                    && !Security::userRoot(Server::getInstance().m_strRootPath, user, true).isEmpty()) {
                result = 1; m_strLoginName = user;
                m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                m_authenticated = true; m_offlineHandled = false;
                if (PresenceStore::getInstance().login(user, m_sessionId))
                    MyTcpServer::getInstance().notifyFriendsPresence(user, true, 0);
            }
        }
        PDU *reply = mkPDU(); reply->uiType = ENUM_MSG_TYPE_LOGIN_WITH_CAPTCHA_RESPOND;
        memcpy(reply->caData, &result, sizeof(result)); return reply;
    }
    case ENUM_MSG_TYPE_REGIST_REQUEST: return m_pmh->regist();
    case ENUM_MSG_TYPE_FIND_USER_REQUEST: return m_pmh->findUser();
    case ENUM_MSG_TYPE_ONLINE_USER_REQUEST: return m_pmh->onlineUser();
    case ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_REQUEST: {
        const QStringList friends = OperateDB::getInstance().handleFlushFriend(m_strLoginName.toUtf8().constData());
        QStringList online;
        for (const QString &name : friends) {
            if (PresenceStore::getInstance().isOnline(name)) online.append(name);
            if (online.size() >= int((Security::MaxFrame - sizeof(PDU)) / 32)) break;
        }
        PDU *reply = mkPDU(online.size() * 32); reply->uiType = ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_RESPOND;
        for (int i = 0; i < online.size(); ++i) Security::putField(reply->caMsg + i * 32, online[i]);
        return reply;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST: return m_pmh->addFriend();
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE_REQUEST: return m_pmh->addfriendAgree();
    case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST: return m_pmh->flushFriend();
    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST: return m_pmh->deleteFriend();
    case ENUM_MSG_TYPE_CHAT_REQUEST: return m_pmh->chat();
    case ENUM_MSG_TYPE_CREATE_FILE_REQUEST: return m_pmh->createFile();
    case ENUM_MSG_TYPE_FLUSH_FILE_REQUEST: return m_pmh->flushFile();
    case ENUM_MSG_TYPE_DEL_FILE_REQUEST: return m_pmh->delFile();
    case ENUM_MSG_TYPE_RENAME_FILE_REQUEST: return m_pmh->renameFile();
    case ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST: return m_pmh->uploadFileInit();
    case ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST: return m_pmh->uploadFileData();
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST: return m_pmh->downloadFile();
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_REQUEST: return m_pmh->downloadFileData();
    case ENUM_MSG_TYPE_SHARE_FILE_REQUEST: return m_pmh->shareFile();
    case ENUM_MSG_TYPE_SHARE_FILE_RESPOND: return m_pmh->shareFileAgree();
    case ENUM_MSG_TYPE_HEARTBEAT_REQUEST: {
        PresenceStore::getInstance().heartbeat(m_strLoginName, m_sessionId);
        PDU *reply = mkPDU(); reply->uiType = ENUM_MSG_TYPE_HEARTBEAT_RESPOND; return reply;
    }
    default: abort(); return nullptr;
    }
}

void MyTcpSocket::recvMsg()
{
    if (m_dispatching) return;
    QScopedValueRollback<bool> guard(m_dispatching, true);
    while (state() == QAbstractSocket::ConnectedState) {
        // Read only enough for the header/frame; a burst cannot grow our buffer unboundedly.
        if (buffer.size() < int(sizeof(PDU))) buffer.append(read(int(sizeof(PDU)) - buffer.size()));
        if (buffer.size() < int(sizeof(PDU))) return;
        uint total = 0, payload = 0;
        memcpy(&total, buffer.constData(), sizeof(total));
        memcpy(&payload, buffer.constData() + sizeof(uint), sizeof(payload));
        if (!Security::validHeader(total, payload)) { buffer.clear(); abort(); return; }
        if (buffer.size() < int(total)) buffer.append(read(int(total) - buffer.size()));
        if (buffer.size() < int(total)) return;
        std::unique_ptr<PDU, decltype(&free)> request(mkPDU(payload), &free);
        memcpy(request.get(), buffer.constData(), total); buffer.clear();
        std::unique_ptr<PDU, decltype(&free)> reply(handleMsg(request.get()), &free);
        m_pmh->pdu = nullptr;
        if (state() != QAbstractSocket::ConnectedState) return;
        updateActiveTime();
        sendMsg(reply.get());
        if (request->uiType == ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST && reply && reply->caData[0]) {
            qint64 size = 0; memcpy(&size, request->caData + 32, sizeof(size));
            if (size == 0) {
                PDU completion = {}; completion.uiTotalLen = sizeof(PDU);
                completion.uiType = ENUM_MSG_TYPE_UPLOAD_FILE_DATA_RESPOND; completion.caData[0] = 1;
                sendMsg(&completion);
            }
        }
    }
}
void MyTcpSocket::clientOffline()
{
    MyTcpServer::getInstance().userOffline(this, 0);
    MyTcpServer::getInstance().removeSocket(this);
}
// Borrows pdu; caller owns it. QTcpSocket copies into its write buffer.
void MyTcpSocket::sendMsg(PDU *pdu)
{
    if (!pdu) return;
    if (!Security::validHeader(pdu->uiTotalLen, pdu->uiMsgLen)
            || bytesToWrite() + pdu->uiTotalLen > 4LL * Security::MaxFrame) { abort(); return; }
    if (write(reinterpret_cast<const char *>(pdu), pdu->uiTotalLen) < 0) abort();
}
void MyTcpSocket::updateActiveTime() { m_lastActiveTime = QDateTime::currentDateTime(); m_activity.start(); }
void MyTcpSocket::generateCaptcha() { m_captchaText = CaptchaCode::generateText(); m_captchaAge.start(); }
bool MyTcpSocket::verifyCaptcha(const QString &input)
{
    const bool ok = !m_captchaText.isEmpty() && !input.isEmpty() && m_captchaAge.isValid()
        && m_captchaAge.elapsed() <= 120000 && input.compare(m_captchaText, Qt::CaseInsensitive) == 0;
    m_captchaText.clear(); m_captchaAge.invalidate(); return ok;
}
bool MyTcpSocket::isTimeout() { return m_activity.elapsed() > (m_authenticated ? 90000 : 30000); }
