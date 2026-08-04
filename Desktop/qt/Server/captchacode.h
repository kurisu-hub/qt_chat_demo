#ifndef CAPTCHACODE_H
#define CAPTCHACODE_H

#include <QString>
#include <QPixmap>

class CaptchaCode
{
public:
    //生成 4 位随机验证码文本（去掉易混淆字符 O/0/1/I）
    static QString generateText();
    //根据验证码文本绘制一张带干扰线的图片
    static QPixmap drawCaptcha(const QString &text);
};

#endif // CAPTCHACODE_H
