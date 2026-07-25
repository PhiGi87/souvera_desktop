/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKPANEL_H
#define TALKPANEL_H

#include <QWidget>
#include <QJsonArray>
#include <QSplitter>
#include <QListView>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

namespace OCC {

class TalkConversationModel;
class TalkOcsApi;
class AccountState;

class TalkPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TalkPanel(QWidget *parent = nullptr);
    ~TalkPanel() override = default;

    void setAccountState(AccountState *state);

private:
    void setupUi();
    void onConversationSelected();
    void sendMessage();
    void pollMessages();
    void onConversationsReceived(const QJsonArray &conversations);
    void onMessagesReceived(const QJsonArray &messages, const QString &token);
    void rebuildChatArea(const QJsonArray &messages);

    QSplitter *_splitter = nullptr;
    QListView *_conversationList = nullptr;
    QScrollArea *_chatScroll = nullptr;
    QWidget *_chatContainer = nullptr;
    QLineEdit *_messageInput = nullptr;
    QPushButton *_sendBtn = nullptr;
    QTimer *_pollTimer = nullptr;

    TalkConversationModel *_conversationModel = nullptr;
    TalkOcsApi *_ocsApi = nullptr;

    QString _currentToken;
    qint64 _lastKnownId = 0;
    QString _currentUserId;
};

} // namespace OCC

#endif // TALKPANEL_H
