#ifndef CHAT_H
#define CHAT_H

#include "../Shared/chatprotocol.h"
#include <QElapsedTimer>
#include <QList>
#include <QWidget>

namespace Ui { class Chat; }
class QListWidgetItem;
class QTimer;

class Chat : public QWidget
{
    Q_OBJECT

public:
    explicit Chat(const QString &peer, const QString &owner, QWidget *parent = nullptr);
    ~Chat();
    void open();
    void receive(uint type, const QJsonObject &reply);
    void connectionChanged();

private slots:
    void on_send_PB_clicked();

private:
    struct Entry {
        ChatProtocol::Message message;
        QString status;
        bool retryable = false;
        qint64 deadline = 0;
        QListWidgetItem *item = nullptr;
    };

    void requestHistory(qint64 before);
    void merge(const ChatProtocol::Message &message);
    void updateEntry(Entry &entry);
    void updateControls();
    void checkTimeouts();
    void send(Entry &entry);
    void retrySelected();

    Ui::Chat *ui;
    const QString m_peer;
    const QString m_owner;
    QTimer *m_timer;
    QElapsedTimer m_clock;
    QList<Entry> m_entries;
    QString m_historyRequest;
    qint64 m_historyDeadline = 0;
    qint64 m_requestedBefore = 0;
    qint64 m_before = 0;
    bool m_historyLoaded = false;
    bool m_hasMore = true;
};

#endif
