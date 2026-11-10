#include "captchacode.h"

#include <QPainter>
#include <QPen>
#include <QRandomGenerator>
#include <QFont>
#include <QColor>

QString CaptchaCode::generateText()
{
    //去掉易混淆字符 O、0、1、I，方便用户辨认
    const QString chars = QStringLiteral("ABCDEFGHJKMNPQRSTUVWXYZ23456789");
    QString text;
    for (int i = 0; i < 4; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.size());
        text.append(chars.at(index));
    }
    return text;
}

QPixmap CaptchaCode::drawCaptcha(const QString &text)
{
    const int width = 120;
    const int height = 40;
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);
    QRandomGenerator *rng = QRandomGenerator::global();

    //干扰线
    painter.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < 5; ++i) {
        painter.setPen(QPen(QColor(rng->bounded(256), rng->bounded(256), rng->bounded(256)), 1));
        int x1 = rng->bounded(width);
        int y1 = rng->bounded(height);
        int x2 = rng->bounded(width);
        int y2 = rng->bounded(height);
        painter.drawLine(x1, y1, x2, y2);
    }

    //逐个字符绘制，随机旋转和颜色，字符间留出空隙防止粘连
    int perChar = width / (text.size() + 1);
    for (int i = 0; i < text.size(); ++i) {
        painter.save();
        QFont font("Arial", rng->bounded(18, 24), rng->bounded(2) ? QFont::Bold : QFont::Normal);
        painter.setFont(font);
        painter.setPen(QColor(rng->bounded(80), rng->bounded(80), rng->bounded(200)));
        int x = perChar + i * perChar + rng->bounded(5);
        int y = height / 2 + rng->bounded(6) - 3;
        painter.translate(x, y);
        painter.rotate(rng->bounded(-25, 25));
        painter.drawText(-10, 8, QString(text.at(i)));
        painter.restore();
    }

    painter.end();
    return pixmap;
}
