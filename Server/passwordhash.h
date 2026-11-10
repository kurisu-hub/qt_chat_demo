#ifndef PASSWORDHASH_H
#define PASSWORDHASH_H

#include <QByteArray>
#include <QString>

namespace PasswordHash {
QString create(const QByteArray &password);
bool verify(const QByteArray &password, const QString &encoded);
bool constantTimeEquals(const QByteArray &left, const QByteArray &right);
}

#endif
