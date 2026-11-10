#include "security.h"
#include <QDir>
#include <QFileInfo>
#include <cstring>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
bool text(const char *data, uint size, bool empty = false)
{
    const char *end = static_cast<const char *>(memchr(data, 0, size));
    if (!end || (!empty && end == data)) return false;
    const QByteArray bytes(data, int(end - data));
    return QString::fromUtf8(bytes).toUtf8() == bytes;
}
bool bodyText(const PDU *p, uint offset = 0)
{
    if (offset >= p->uiMsgLen) return false;
    const uint len = p->uiMsgLen - offset;
    return len <= 4096 && text(p->caMsg + offset, len)
        && p->caMsg[p->uiMsgLen - 1] == 0
        && strlen(p->caMsg + offset) + 1 == len;
}
bool linkOrReparse(const QString &path)
{
    if (QFileInfo(path).isSymLink()) return true;
#ifdef Q_OS_WIN
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return true;
#endif
    return false;
}
bool inside(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + '/');
}
bool plainComponents(const QString &base, const QString &path)
{
    if (!inside(path, base) || linkOrReparse(base)) return false;
    QString cursor = base;
    const QString relative = QDir(base).relativeFilePath(path);
    if (relative == ".") return true;
    for (const QString &part : relative.split('/')) {
        if (!Security::validName(part)) return false;
        cursor += '/' + part;
        if (linkOrReparse(cursor)) return false;
    }
    return true;
}
}

bool Security::validHeader(uint total, uint payload)
{
    return total >= sizeof(PDU) && total <= MaxFrame && payload == total - sizeof(PDU);
}
QString Security::field(const char *data, int size)
{
    const char *end = static_cast<const char *>(memchr(data, 0, size));
    return end ? QString::fromUtf8(data, int(end - data)) : QString();
}
void Security::putField(char *data, const QString &value, int size)
{
    memset(data, 0, size);
    const QByteArray bytes = value.toUtf8();
    if (bytes.size() < size) memcpy(data, bytes.constData(), bytes.size());
}
bool Security::validName(const QString &name)
{
    if (name.isEmpty() || name == "." || name == ".." || name.endsWith('.') || name.endsWith(' ')) return false;
    for (QChar c : name) {
        if (c.unicode() < 32 || QStringLiteral("/\\:*?\"<>|").contains(c)) return false;
    }
    const QString stem = name.section('.', 0, 0).toUpper();
    if (QStringList({"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"}).contains(stem)) return false;
    if ((stem.startsWith("COM") || stem.startsWith("LPT")) && stem.size() == 4
            && (stem[3].isDigit() || QString::fromUtf8("¹²³").contains(stem[3]))) return false;
    return true;
}

bool Security::validRequest(const PDU *p)
{
    if (!validHeader(p->uiTotalLen, p->uiMsgLen)) return false;
    const auto name = [](const char *s) { return text(s, 32) && Security::validName(Security::field(s)); };
    switch (p->uiType) {
    case ENUM_MSG_TYPE_CAPTCHA_REQUEST:
    case ENUM_MSG_TYPE_ONLINE_USER_REQUEST:
    case ENUM_MSG_TYPE_FRIEND_PRESENCE_SNAPSHOT_REQUEST:
    case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST:
    case ENUM_MSG_TYPE_HEARTBEAT_REQUEST:
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_REQUEST:
        return p->uiMsgLen == 0;
    case ENUM_MSG_TYPE_REGIST_REQUEST:
    case ENUM_MSG_TYPE_LOGIN_REQUEST:
        return p->uiMsgLen == 0 && name(p->caData) && text(p->caData + 32, 32);
    case ENUM_MSG_TYPE_LOGIN_WITH_CAPTCHA_REQUEST:
        return name(p->caData) && text(p->caData + 32, 32) && p->uiMsgLen <= 16 && bodyText(p);
    case ENUM_MSG_TYPE_FIND_USER_REQUEST:
        return p->uiMsgLen == 0 && name(p->caData);
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:
        return p->uiMsgLen == 0 && name(p->caData + 32);
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE_REQUEST:
        return p->uiMsgLen == 0 && name(p->caData);
    case ENUM_MSG_TYPE_CHAT_REQUEST:
        return name(p->caData + 32) && bodyText(p);
    case ENUM_MSG_TYPE_CREATE_FILE_REQUEST:
    case ENUM_MSG_TYPE_UPLOAD_FILE_INIT_REQUEST:
        return name(p->caData) && bodyText(p);
    case ENUM_MSG_TYPE_FLUSH_FILE_REQUEST:
    case ENUM_MSG_TYPE_DEL_FILE_REQUEST:
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST:
        return bodyText(p);
    case ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST:
        return p->uiMsgLen > 0 && p->uiMsgLen <= 64 * 1024;
    case ENUM_MSG_TYPE_RENAME_FILE_REQUEST: {
        if (!p->uiMsgLen) return text(p->caData, 32) && text(p->caData + 32, 32);
        if (!text(p->caMsg, p->uiMsgLen)) return false;
        const uint offset = uint(strlen(p->caMsg)) + 1;
        return offset <= 4096 && bodyText(p, offset);
    }
    case ENUM_MSG_TYPE_SHARE_FILE_REQUEST: {
        int count = 0;
        memcpy(&count, p->caData + 32, sizeof(count));
        if (count < 1 || count > 256 || !bodyText(p, uint(count) * 32)) return false;
        for (int i = 0; i < count; ++i) if (!name(p->caMsg + i * 32)) return false;
        return true;
    }
    case ENUM_MSG_TYPE_SHARE_FILE_RESPOND: {
        int accepted = 0;
        memcpy(&accepted, p->caData + 32, sizeof(accepted));
        return (accepted == 0 || accepted == 1) && bodyText(p);
    }
    default: return false;
    }
}

QString Security::userRoot(const QString &root, const QString &user, bool create)
{
    if (!validName(user) || user.toUtf8().size() > 31) return {};
    const QString base = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    if (linkOrReparse(base) || !QFileInfo(base).isDir()) return {};
    const QString home = base + '/' + user;
    if (!plainComponents(base, home)) return {};
    // QFileInfo may resolve an NTFS 8.3 name or a case alias to another user's
    // existing directory. Only the actual directory entry is a valid account home.
    if (QFileInfo::exists(home) && !QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)
            .contains(user, Qt::CaseSensitive)) return {};
    if (create && !QDir().mkpath(home)) return {};
    if (!QFileInfo(home).isDir()) return {};
    const QString canonicalBase = QFileInfo(base).canonicalFilePath();
    const QString canonicalHome = QFileInfo(home).canonicalFilePath();
    if (canonicalHome.isEmpty() || !inside(canonicalHome, canonicalBase)) return {};
    return home;
}

QString Security::resolvePath(const QString &root, const QString &user, const QString &input, bool allowRoot)
{
    const QString home = userRoot(root, user);
    if (home.isEmpty() || input.isEmpty()) return {};
    const QString normalized = QDir::fromNativeSeparators(input);
    if (normalized.split('/').contains("..")) return {};
    const QString candidate = QDir::cleanPath(QFileInfo(normalized).absoluteFilePath());
    if (!inside(candidate, home) || (!allowRoot && candidate == home) || !plainComponents(home, candidate)) return {};
    QFileInfo ancestor(candidate);
    while (!ancestor.exists()) {
        const QString parent = ancestor.absolutePath();
        if (parent == ancestor.absoluteFilePath()) return {};
        ancestor.setFile(parent);
    }
    if (!inside(ancestor.canonicalFilePath(), QFileInfo(home).canonicalFilePath())) return {};
    return candidate;
}

bool Security::safeTree(const QString &path)
{
    if (linkOrReparse(path)) return false;
    const QFileInfo info(path);
    if (!info.isDir()) return info.isFile();
    const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries) if (!safeTree(entry.absoluteFilePath())) return false;
    return true;
}
