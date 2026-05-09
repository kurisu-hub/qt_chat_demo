#ifndef CHAT_H
#define CHAT_H

#include <QTextEdit>
#include <QWidget>

namespace Ui {
class Chat;
}

class Chat : public QWidget
{
    Q_OBJECT

public:
    explicit Chat(QWidget *parent = nullptr);
    ~Chat();
    QString m_strChatName;
    void updateShow_TE(QString strMsg);
    void appendAlignedColoredText(QTextEdit *textEdit,
                                  const QString &text,
                                  Qt::Alignment alignment,
                                  const QColor &color);
    QTextEdit*getshow_LE();

private slots:
    void on_send_PB_clicked();

private:
    Ui::Chat *ui;
};

#endif // CHAT_H
