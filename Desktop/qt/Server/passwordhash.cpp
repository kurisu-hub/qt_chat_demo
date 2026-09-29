#include "passwordhash.h"
#include <QtNetwork/qpassworddigestor.h>
#include <QRandomGenerator>
#include <QStringList>

namespace {
const int Iterations = 600000;
const int SaltBytes = 16;
const int KeyBytes = 32;
QByteArray derive(const QByteArray &password, const QByteArray &salt, int rounds)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                            password, salt, rounds, KeyBytes);
}
}

bool PasswordHash::constantTimeEquals(const QByteArray &left, const QByteArray &right)
{
    if (left.size() != right.size())
        return false;
    volatile unsigned char difference = 0;
    for (int i = 0; i < left.size(); ++i)
        difference |= static_cast<unsigned char>(left.at(i) ^ right.at(i));
    return difference == 0;
}

QString PasswordHash::create(const QByteArray &password)
{
    QByteArray salt(SaltBytes, '\0');
    for (int i = 0; i < SaltBytes; i += 4) {
        const quint32 random = QRandomGenerator::system()->generate();
        for (int j = 0; j < 4; ++j)
            salt[i + j] = static_cast<char>((random >> (8 * j)) & 0xff);
    }
    const QByteArray key = derive(password, salt, Iterations);
    if (key.size() != KeyBytes)
        return QString();
    return QStringLiteral("pbkdf2-sha256$1$%1$%2$%3")
        .arg(Iterations).arg(QString::fromLatin1(salt.toHex()), QString::fromLatin1(key.toHex()));
}

bool PasswordHash::verify(const QByteArray &password, const QString &encoded)
{
    if (encoded.size() > 255)
        return false;
    const QStringList parts = encoded.split('$');
    if (parts.size() != 5 || parts[0] != QStringLiteral("pbkdf2-sha256")
        || parts[1] != QStringLiteral("1"))
        return false;
    bool ok = false;
    const int rounds = parts[2].toInt(&ok);
    // Bound work and reject noncanonical or weakened records.
    if (!ok || rounds < Iterations || rounds > 2000000 || QString::number(rounds) != parts[2])
        return false;
    const QByteArray salt = QByteArray::fromHex(parts[3].toLatin1());
    const QByteArray key = QByteArray::fromHex(parts[4].toLatin1());
    if (salt.size() != SaltBytes || key.size() != KeyBytes
        || QString::fromLatin1(salt.toHex()) != parts[3]
        || QString::fromLatin1(key.toHex()) != parts[4])
        return false;
    return constantTimeEquals(derive(password, salt, rounds), key);
}
