#include "passwordhash.h"
#include <QtNetwork/qpassworddigestor.h>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    const auto check = [&failures](bool passed, const char *label) {
        if (!passed) { qCritical() << label; ++failures; }
    };
    // Published PBKDF2-HMAC-SHA256 password/salt vector, one iteration.
    check(QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
        "password", "salt", 1, 32).toHex()
        == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
        "PBKDF2 known-answer vector");
    const QByteArray password("Case'Sensitive\\Password");
    const QString first = PasswordHash::create(password);
    const QString second = PasswordHash::create(password);
    check(!first.isEmpty() && first.size() <= 255, "storage length");
    check(first != second, "independent salts");
    check(PasswordHash::verify(password, first), "round trip");
    check(!PasswordHash::verify(password.toLower(), first), "case sensitive");
    check(!PasswordHash::verify("wrong", first), "wrong password");
    check(!PasswordHash::verify(password, first + "x"), "malformed hex");
    QString weakened = first;
    weakened.replace("$600000$", "$1$");
    check(!PasswordHash::verify(password, weakened), "weak iteration count");
    QString excessive = first;
    excessive.replace("$600000$", "$2147483647$");
    check(!PasswordHash::verify(password, excessive), "unbounded iteration count");
    check(!PasswordHash::verify(password, ""), "empty record");
    check(!PasswordHash::verify(password, QString(256, 'x')), "oversize record");
    check(!PasswordHash::constantTimeEquals("secret", "Secret"), "legacy case mismatch");
    check(!PasswordHash::constantTimeEquals("secret", "secret "), "legacy trailing space");
    check(PasswordHash::constantTimeEquals("secret", "secret"), "legacy exact match");
    qInfo() << "Password hash test failures:" << failures;
    return failures ? 1 : 0;
}
