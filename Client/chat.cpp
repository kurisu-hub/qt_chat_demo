#include "chat.h"
#include "client.h"
#include "ui_chat.h"

#include <QDateTime>
#include <QScrollBar>
#include <algorithm>

using namespace ChatProtocol;

namespace {
constexpr int ReplyTimeoutMs = 5000;

QString resultText(int result)
{
    switch (result) {
    case Forbidden: return QStringLiteral("没有权限，请确认好友关系");
    case Conflict: return QStringLiteral("消息编号与已有记录冲突");
    default: return QStringLiteral("数据库暂时不可用");
    }
}
}

Chat::Chat(const QString &peer, const QString &owner, QWidget *parent)
    : QWidget(parent, Qt::Window), ui(new Ui::Chat), m_peer(peer), m_owner(owner), m_timer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("与 %1 的聊天 · %2").arg(peer, owner));
    // 好友页关闭时一并关闭聊天窗口，避免窗口隐藏后应用仍在后台保持登录。
    setAttribute(Qt::WA_QuitOnClose, false);
    ui->show_TE->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->show_TE->setWordWrap(true);
    connect(ui->history_PB, &QPushButton::clicked, this, [this] { requestHistory(m_before); });
    connect(ui->retry_PB, &QPushButton::clicked, this, &Chat::retrySelected);
    connect(ui->input_LE, &QLineEdit::returnPressed, this, &Chat::on_send_PB_clicked);
    connect(ui->show_TE, &QListWidget::currentItemChanged, this, &Chat::updateControls);
    connect(m_timer, &QTimer::timeout, this, &Chat::checkTimeouts);
    m_timer->setInterval(500);
    m_clock.start();
    updateControls();
}

Chat::~Chat() { delete ui; }

void Chat::open()
{
    showNormal();
    raise();
    activateWindow();
    ui->input_LE->setFocus();
    if (!m_historyLoaded) requestHistory(0);
}

// 保留输入草稿和未确认消息。断线时把确认结果交给重新登录后的同 ID 重试。
void Chat::connectionChanged()
{
    if (Client::getInstance().isAuthenticated()) {
        // 重连后从最新页重新开始，避免旧页和最新页之间出现未标出的缺口。
        for (int i = m_entries.size() - 1; i >= 0; --i) {
            if (m_entries[i].message.id == 0) continue;
            delete m_entries[i].item;
            m_entries.removeAt(i);
        }
        m_historyLoaded = false;
        m_before = 0;
        m_hasMore = true;
        if (isVisible()) requestHistory(0);
    } else {
        for (Entry &entry : m_entries) {
            if (!entry.deadline) continue;
            entry.deadline = 0;
            entry.status = QStringLiteral("连接中断，结果待确认");
            entry.retryable = true;
            updateEntry(entry);
        }
        m_historyRequest.clear();
        m_timer->stop();
        ui->status_LB->setText(QStringLiteral("已断开，重新登录后可重试"));
    }
    updateControls();
}

void Chat::requestHistory(qint64 before)
{
    auto &client = Client::getInstance();
    if (!client.isAuthenticated() || !m_historyRequest.isEmpty()) return;
    m_historyRequest = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_requestedBefore = before;
    m_historyDeadline = m_clock.elapsed() + ReplyTimeoutMs;
    ui->status_LB->setText(QStringLiteral("正在加载历史…"));
    if (client.sendMsg(pack(History, {{"peer", m_peer}, {"requestId", m_historyRequest},
                                      {"before", QString::number(before)}}))) {
        m_timer->start();
    } else {
        m_historyRequest.clear();
        ui->status_LB->setText(QStringLiteral("历史请求发送失败"));
    }
    updateControls();
}

void Chat::on_send_PB_clicked()
{
    const QString text = ui->input_LE->text();
    if (text.trimmed().isEmpty() || !Client::getInstance().isAuthenticated()) return;
    if (!validText(text)) {
        ui->status_LB->setText(QStringLiteral("消息最多支持 %1 字节 UTF-8 文本").arg(MaxTextBytes));
        return;
    }
    Entry entry;
    entry.message.clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.message.sender = m_owner;
    entry.message.text = text;
    entry.item = new QListWidgetItem(ui->show_TE);
    m_entries.append(entry);
    ui->input_LE->clear();
    send(m_entries.last());
    ui->show_TE->scrollToBottom();
}

void Chat::send(Entry &entry)
{
    entry.status = QStringLiteral("发送中");
    entry.retryable = false;
    entry.deadline = m_clock.elapsed() + ReplyTimeoutMs;
    if (Client::getInstance().sendMsg(pack(Send, {{"peer", m_peer},
        {"requestId", entry.message.clientId}, {"text", entry.message.text}}))) {
        m_timer->start();
    } else {
        entry.status = QStringLiteral("发送失败，可重试");
        entry.retryable = true;
        entry.deadline = 0;
    }
    updateEntry(entry);
    updateControls();
}

void Chat::retrySelected()
{
    if (!Client::getInstance().isAuthenticated()) return;
    for (Entry &entry : m_entries) {
        if (entry.item == ui->show_TE->currentItem() && entry.retryable) {
            send(entry);
            return;
        }
    }
}

// 推送、ACK 和历史统一合并。历史若已包含本机待确认记录，直接替换该条的发送状态。
void Chat::merge(const Message &message)
{
    if (message.sender != m_owner && message.sender != m_peer) return;
    const auto existing = std::find_if(m_entries.cbegin(), m_entries.cend(),
        [&message](const Entry &entry) { return entry.message.id == message.id; });
    if (existing != m_entries.cend()) return;
    const auto pending = std::find_if(m_entries.begin(), m_entries.end(),
        [&message, this](const Entry &entry) {
            return message.sender == m_owner && entry.message.id == 0
                && entry.message.clientId == message.clientId && entry.message.text == message.text;
        });
    if (pending != m_entries.end()) {
        delete pending->item;
        m_entries.erase(pending);
    }
    const auto position = std::lower_bound(m_entries.cbegin(), m_entries.cend(), message.id,
        [](const Entry &entry, qint64 id) { return entry.message.id != 0 && entry.message.id < id; });
    const int row = int(position - m_entries.cbegin());
    Entry entry;
    entry.message = message;
    entry.status = message.sender == m_owner ? QStringLiteral("已发送") : QString();
    entry.item = new QListWidgetItem;
    updateEntry(entry);
    ui->show_TE->insertItem(row, entry.item);
    m_entries.insert(row, entry);
}

void Chat::receive(uint type, const QJsonObject &reply)
{
    const QString requestId = reply.value("requestId").toString();
    const int result = reply.value("result").toInt(StorageError);
    if (type == Push || (type == Ack && result == Saved)) {
        Message message;
        if (!decode(reply.value("message").toObject(), message)) return;
        if (type == Ack && (message.clientId != requestId || message.sender != m_owner)) return;
        const bool atBottom = ui->show_TE->verticalScrollBar()->value() == ui->show_TE->verticalScrollBar()->maximum();
        merge(message);
        if (atBottom) ui->show_TE->scrollToBottom();
    } else if (type == Ack) {
        for (Entry &entry : m_entries) {
            if (entry.message.id || entry.message.clientId != requestId) continue;
            entry.deadline = 0;
            entry.status = result == StorageError ? QStringLiteral("保存未确认，可重试") : resultText(result);
            entry.retryable = result == StorageError;
            updateEntry(entry);
            break;
        }
    } else if (type == HistoryPage && requestId == m_historyRequest && !requestId.isEmpty()) {
        m_historyRequest.clear();
        if (result == Saved) {
            const QJsonArray rows = reply.value("messages").toArray();
            if (rows.size() > PageSize || !reply.value("messages").isArray() || !reply.value("hasMore").isBool()) {
                ui->status_LB->setText(QStringLiteral("历史响应无效，可以重试"));
                updateControls();
                return;
            }
            QList<Message> page;
            for (const QJsonValue &value : rows) {
                Message message;
                if (!decode(value.toObject(), message) || (message.sender != m_owner && message.sender != m_peer)
                        || (m_requestedBefore && message.id >= m_requestedBefore)
                        || (!page.isEmpty() && message.id <= page.last().id)) {
                    ui->status_LB->setText(QStringLiteral("历史响应无效，可以重试"));
                    updateControls();
                    return;
                }
                page.append(message);
            }
            // 游标只由历史页推进。实时消息先到时，翻页仍以服务端返回的页边界为准。
            const QPersistentModelIndex anchor(ui->show_TE->indexAt(QPoint(1, 1)));
            for (const Message &message : page) merge(message);
            m_historyLoaded = true;
            m_hasMore = !page.isEmpty() && reply.value("hasMore").toBool();
            if (!page.isEmpty()) m_before = page.first().id;
            if (m_requestedBefore == 0) ui->show_TE->scrollToBottom();
            else if (anchor.isValid()) ui->show_TE->scrollToItem(ui->show_TE->item(anchor.row()), QAbstractItemView::PositionAtTop);
            ui->status_LB->setText(m_hasMore ? QString() : QStringLiteral("已到最早的消息"));
        } else {
            ui->status_LB->setText(resultText(result));
        }
    }
    updateControls();
}

void Chat::updateEntry(Entry &entry)
{
    const Message &message = entry.message;
    const QString time = message.createdMs
        ? QDateTime::fromMSecsSinceEpoch(message.createdMs).toString(QStringLiteral("MM-dd HH:mm:ss")) : QString();
    const QString header = QStringList{message.sender, time, entry.status}.join(QStringLiteral("  ")).trimmed();
    entry.item->setText(header + '\n' + message.text);
    entry.item->setTextAlignment(message.sender == m_owner ? Qt::AlignRight : Qt::AlignLeft);
}

void Chat::updateControls()
{
    const bool online = Client::getInstance().isAuthenticated();
    const auto selected = ui->show_TE->currentItem();
    ui->send_PB->setEnabled(online);
    ui->history_PB->setEnabled(online && m_hasMore && m_historyRequest.isEmpty());
    ui->history_PB->setText(m_historyLoaded ? QStringLiteral("加载更早消息") : QStringLiteral("加载历史"));
    ui->retry_PB->setEnabled(online && selected && std::any_of(m_entries.cbegin(), m_entries.cend(),
        [selected](const Entry &entry) { return entry.item == selected && entry.retryable; }));
}

void Chat::checkTimeouts()
{
    const qint64 now = m_clock.elapsed();
    if (!m_historyRequest.isEmpty() && now >= m_historyDeadline) {
        m_historyRequest.clear();
        ui->status_LB->setText(QStringLiteral("历史请求超时，可以重试"));
    }
    bool waiting = !m_historyRequest.isEmpty();
    for (Entry &entry : m_entries) {
        if (entry.deadline && now >= entry.deadline) {
            entry.deadline = 0;
            entry.status = QStringLiteral("确认超时，可重试");
            entry.retryable = true;
            updateEntry(entry);
        }
        waiting |= entry.deadline != 0;
    }
    if (!waiting) m_timer->stop();
    updateControls();
}
