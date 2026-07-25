/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkPanel.h"
#include "TalkOcsApi.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QSplitter>

Q_LOGGING_CATEGORY(lcTalkPanel, "souvera.talk.panel")

namespace OCC {

TalkPanel::TalkPanel(QWidget *parent)
    : QWidget(parent)
{
    _ocsApi = new TalkOcsApi(this);
    setupUi();

    _pollTimer = new QTimer(this);
    connect(_pollTimer, &QTimer::timeout, this, &TalkPanel::pollMessages);

    // Demo: populate conversation list
    _conversationList->addItem(QStringLiteral("Allgemein"));
    _conversationList->addItem(QStringLiteral("Projekt Souvera"));
    _conversationList->addItem(QStringLiteral("Entwicklungsteam"));
}

void TalkPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);

    auto *title = new QLabel(QStringLiteral("Talk – Nachrichten"), toolbar);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();

    layout->addWidget(toolbar);

    _splitter = new QSplitter(Qt::Horizontal, this);

    _conversationList = new QListWidget(_splitter);
    _conversationList->setFixedWidth(250);
    connect(_conversationList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *, QListWidgetItem *) { onConversationSelected(); });

    auto *chatWidget = new QWidget(_splitter);
    auto *chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(8, 8, 8, 8);

    _messageHistory = new QTextEdit(chatWidget);
    _messageHistory->setReadOnly(true);
    _messageHistory->setPlaceholderText(QStringLiteral("Wählen Sie eine Unterhaltung…"));
    chatLayout->addWidget(_messageHistory, 1);

    auto *inputBar = new QWidget(chatWidget);
    auto *inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(0, 4, 0, 0);

    _messageInput = new QTextEdit(inputBar);
    _messageInput->setFixedHeight(60);
    _messageInput->setPlaceholderText(QStringLiteral("Nachricht eingeben…"));
    inputLayout->addWidget(_messageInput, 1);

    _sendBtn = new QPushButton(QStringLiteral("Senden"), inputBar);
    _sendBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4a90d9; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #357abd; }"));
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
    auto *item = _conversationList->currentItem();
    if (!item) return;

    qCInfo(lcTalkPanel) << "Conversation selected:" << item->text();
    _messageHistory->clear();
    _messageHistory->append(QStringLiteral("--- Unterhaltung: %1 ---\n").arg(item->text()));

    _pollTimer->start(5000);
}

void TalkPanel::sendMessage()
{
    const auto text = _messageInput->toPlainText().trimmed();
    if (text.isEmpty()) return;

    auto *item = _conversationList->currentItem();
    if (!item) return;

    _messageHistory->append(QStringLiteral("Ich: %1").arg(text));
    _messageInput->clear();

    qCInfo(lcTalkPanel) << "Sending message to" << item->text() << ":" << text;
}

void TalkPanel::pollMessages()
{
    qCInfo(lcTalkPanel) << "Polling for new messages…";
}

} // namespace OCC
