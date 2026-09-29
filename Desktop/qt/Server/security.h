#ifndef SECURITY_H
#define SECURITY_H

#include "protocol.h"
#include <QString>
#include <QByteArray>

namespace Security {
constexpr uint MaxFrame = 1024 * 1024;
constexpr qint64 MaxFileSize = 1024LL * 1024 * 1024;
bool validHeader(uint total, uint payload);
bool validRequest(const PDU *pdu);
bool validName(const QString &name);
QString field(const char *data, int size = 32);
void putField(char *data, const QString &text, int size = 32);
QString userRoot(const QString &root, const QString &user, bool create = false);
QString resolvePath(const QString &root, const QString &user,
                    const QString &input, bool allowRoot = true);
bool safeTree(const QString &path);
}
#endif
