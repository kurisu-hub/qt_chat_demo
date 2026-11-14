#ifndef CHATPROTOCOL_H
#define CHATPROTOCOL_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <cstring>
#include "protocol.h"

namespace ChatProtocol {
// 使用现有 PDU 外壳，独立编号避免改变好友和文件协议。64 位 ID 用十进制字符串传输。
enum Type : uint { Send = 100, Ack, Push, History, HistoryPage };
enum Result : int { Saved = 0, Forbidden, Conflict, StorageError };
constexpr int MaxTextBytes = 4095;
constexpr int PageSize = 30;

struct Message {
    qint64 id = 0;
    QString clientId;
    QString sender;
    QString text;
    qint64 createdMs = 0;
};

inline PDU *pack(Type type, const QJsonObject &object)
{
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    PDU *pdu = mkPDU(bytes.size());
    pdu->uiType = type;
    std::memcpy(pdu->caMsg, bytes.constData(), bytes.size());
    return pdu;
}

// 上限在 JSON 解析前检查，UTF-8 往返校验避免把损坏的正文替换为其他字符。
inline QJsonObject unpack(const PDU *pdu, uint maxBytes)
{
    if (!pdu->uiMsgLen || pdu->uiMsgLen > maxBytes) return {};
    const QByteArray bytes(pdu->caMsg, int(pdu->uiMsgLen));
    if (QString::fromUtf8(bytes).toUtf8() != bytes) return {};
    return QJsonDocument::fromJson(bytes).object();
}

inline bool validText(const QString &text)
{
    return !text.isEmpty() && !text.contains(QChar(0)) && text.toUtf8().size() <= MaxTextBytes;
}

inline bool validClientId(const QString &id)
{
    const QUuid uuid(id);
    return id.size() == 36 && !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == id;
}

// 0 表示第一页，-1 表示非法游标。禁止浮点数及带符号、前导零的表示。
inline qint64 readId(const QJsonValue &value)
{
    const QString text = value.toString();
    bool ok = false;
    const qint64 number = text.toLongLong(&ok);
    return ok && number >= 0 && QString::number(number) == text ? number : -1;
}

inline QJsonObject encode(const Message &message)
{
    return {{"id", QString::number(message.id)}, {"clientId", message.clientId},
            {"sender", message.sender}, {"text", message.text},
            {"createdMs", QString::number(message.createdMs)}};
}

inline bool decode(const QJsonObject &object, Message &message)
{
    message.id = readId(object.value("id"));
    message.clientId = object.value("clientId").toString();
    message.sender = object.value("sender").toString();
    message.text = object.value("text").toString();
    message.createdMs = readId(object.value("createdMs"));
    return message.id > 0 && validClientId(message.clientId) && !message.sender.isEmpty()
        && validText(message.text) && message.createdMs > 0;
}
}

#endif
