/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkPanel.h"
#include "TalkConversationModel.h"
#include "TalkOcsApi.h"
#include "TalkMessageWidget.h"

#include "accountstate.h"
#include "account.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QScrollBar>
#include <QJsonObject>

Q_LOGGING_CATEGORY(lcTalkPanel, "souvera.talk.panel")

namespace OCC {

TalkPanel::TalkPanel(QWidget *parent)
    : QWidget(parent)
{
    _ocsApi = new TalkOcsApi(this);
    _conversationModel = new TalkConversationModel(this);

    setupUi();

    _pollTimer = new QTimer(this);
    _pollTimer->setInterval(5000);
    connect(_pollTimer, &QTimer::timeout, this, &TalkPanel::pollMessages);

    connect(_ocsApi, &TalkOcsApi::conversationsReceived,
            this, &TalkPanel::onConversationsReceived);
    connect(_ocsApi, &TalkOcsApi::messagesReceived,
            this, &TalkPanel::onMessagesReceived);
    connect(_ocsApi, &TalkOcsApi::messageSent,
            this, [this](const QString &token) {
        if (token == _currentToken) {
            _lastKnownId = 0;
            _ocsApi->fetchMessages(_currentToken);
        }
    });
}

void TalkPanel::setAccountState(AccountState *state)
{
    _ocsApi->setAccountState(state);
    if (state && state->account()) {
        _currentUserId = state->account()->davUser();
    }
    _ocsApi->fetchConversations();
}

void TalkPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);

    auto *title = new QLabel(QStringLiteral("Talk"), toolbar);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();

    auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"), toolbar);
    refreshBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4a90d9; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #357abd; }"));
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        _ocsApi->fetchConversations();
        if (!_currentToken.isEmpty()) {
            _lastKnownId = 0;
            _ocsApi->fetchMessages(_currentToken);
        }
    });
    toolbarLayout->addWidget(refreshBtn);

    layout->addWidget(toolbar);

    _splitter = new QSplitter(Qt::Horizontal, this);

    _conversationList = new QListView(_splitter);
    _conversationList->setModel(_conversationModel);
    _conversationList->setFixedWidth(260);
    _conversationList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _conversationList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(_conversationList, &QListView::clicked, this, [this]() { onConversationSelected(); });

    auto *chatWidget = new QWidget(_splitter);
    auto *chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(0, 0, 0, 0);

    _chatScroll = new QScrollArea(chatWidget);
    _chatScroll->setWidgetResizable(true);
    _chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _chatScroll->setStyleSheet(QStringLiteral("QScrollArea { border: none; background: #f5f5f5; }"));

    _chatContainer = new QWidget(_chatScroll);
    auto *containerLayout = new QVBoxLayout(_chatContainer);
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(4);
    containerLayout->addStretch();

    auto *placeholder = new QLabel(QStringLiteral("Select a conversation"), _chatContainer);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral("color: #999; font-size: 16px;"));
    containerLayout->addWidget(placeholder);

    _chatScroll->setWidget(_chatContainer);
    chatLayout->addWidget(_chatScroll, 1);

    auto *inputBar = new QWidget(chatWidget);
    auto *inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(8, 4, 8, 8);

    _messageInput = new QLineEdit(inputBar);
    _messageInput->setPlaceholderText(QStringLiteral("Type a message…"));
    _messageInput->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 1px solid #ccc; border-radius: 18px;"
        "  padding: 8px 16px; font-size: 13px; background: white; }"));
    connect(_messageInput, &QLineEdit::returnPressed, this, &TalkPanel::sendMessage);
    inputLayout->addWidget(_messageInput, 1);

    _sendBtn = new QPushButton(QStringLiteral("Send"), inputBar);
    _sendBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4a90d9; color: white; border: none;"
        "  border-radius: 18px; padding: 8px 20px; font-weight: bold; }"
        "QPushButton:hover { background-color: #357abd; }"
        "QPushButton:disabled { background-color: #ccc; }"));
    connect(_sendBtn, &QPushButton::clicked, this, &TalkPanel::sendMessage);
    inputLayout->addWidget(_sendBtn);

    chatLayout->addWidget(inputBar);

    _splitter->addWidget(_conversationList);
    _splitter->addWidget(chatWidget);
    _splitter->setStretchFactor(0, 1);
    _splitter->setStretchFactor(1, 3);

    layout->addWidget(_splitter, 1);
}

void TalkPanel::onConversationSelected()
{
    const auto idx = _conversationList->currentIndex();
    if (!idx.isValid()) return;

    const auto token = idx.data(TalkConversationModel::TokenRole).toString();
    const auto displayName = idx.data(TalkConversationModel::DisplayNameRole).toString();
    if (token.isEmpty()) return;

    qCInfo(lcTalkPanel) << "Conversation selected:" << displayName << token;

    _currentToken = token;
    _lastKnownId = 0;

    _ocsApi->fetchMessages(_currentToken);
    _pollTimer->start();
}

void TalkPanel::sendMessage()
{
    const auto text = _messageInput->text().trimmed();
    if (text.isEmpty() || _currentToken.isEmpty()) return;

    qCInfo(lcTalkPanel) << "Sending message to" << _currentToken << ":" << text;

    _messageInput->clear();
    _ocsApi->sendMessage(_currentToken, text);
}

void TalkPanel::pollMessages()
{
    if (_currentToken.isEmpty()) return;
    qCInfo(lcTalkPanel) << "Polling for messages in" << _currentToken;
    _ocsApi->fetchMessages(_currentToken, _lastKnownId);
}

void TalkPanel::onConversationsReceived(const QJsonArray &conversations)
{
    _conversationModel->setConversations(conversations);
    qCInfo(lcTalkPanel) << "Conversations updated:" << conversations.size();
}

void TalkPanel::onMessagesReceived(const QJsonArray &messages, const QString &token)
{
    if (token != _currentToken) return;

    qCInfo(lcTalkPanel) << "Messages received for" << token << ":" << messages.size();

    if (messages.isEmpty()) return;

    rebuildChatArea(messages);

    for (const auto &msgVal : messages) {
        const auto msgObj = msgVal.toObject();
        const auto id = static_cast<qint64>(msgObj.value(QStringLiteral("id")).toDouble());
        if (id > _lastKnownId) {
            _lastKnownId = id;
        }
    }

    auto *scrollBar = _chatScroll->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void TalkPanel::rebuildChatArea(const QJsonArray &messages)
{
    auto *old = _chatScroll->takeWidget();
    if (old) {
        old->deleteLater();
    }

    _chatContainer = new QWidget(_chatScroll);
    auto *layout = new QVBoxLayout(_chatContainer);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    layout->addStretch();

    for (const auto &msgVal : messages) {
        const auto msgObj = msgVal.toObject();
        const auto actorId = msgObj.value(QStringLiteral("actorId")).toString();
        const auto isOwn = (actorId == _currentUserId);
        auto *msgWidget = new TalkMessageWidget(isOwn, _chatContainer);
        msgWidget->setMessage(msgObj);
        layout->addWidget(msgWidget);
    }

    _chatScroll->setWidget(_chatContainer);
}

} // namespace OCC
