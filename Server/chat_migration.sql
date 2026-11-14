-- 在目标库中手工执行。原有用户、好友与文件数据不参与迁移。
CREATE TABLE IF NOT EXISTS chat_conversations (
    id BIGINT NOT NULL AUTO_INCREMENT,
    user_low BIGINT NOT NULL,
    user_high BIGINT NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_chat_pair (user_low, user_high)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS chat_messages (
    id BIGINT NOT NULL AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL,
    sender_id BIGINT NOT NULL,
    client_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    body VARBINARY(4095) NOT NULL,
    created_ms BIGINT NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_chat_send (sender_id, client_id),
    KEY ix_chat_history (conversation_id, id),
    CONSTRAINT fk_chat_conversation FOREIGN KEY (conversation_id) REFERENCES chat_conversations (id)
) ENGINE=InnoDB;
