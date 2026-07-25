/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKPANEL_H
#define TALKPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>

class QSplitter;

namespace OCC {

class TalkOcsApi;

class TalkPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TalkPanel(QWidget *parent = nullptr);
    ~TalkPanel() override = default;

private:
    void setupUi();
    void onConversationSelected();
    void sendMessage();
    void pollMessages();

    QSplitter *_splitter = nullptr;
    QListWidget *_conversationList = nullptr;
    QTextEdit *_messageHistory = nullptr;
    QTextEdit *_messageInput = nullptr;
    QPushButton *_sendBtn = nullptr;
    QTimer *_pollTimer = nullptr;

    TalkOcsApi *_ocsApi = nullptr;
};

} // namespace OCC

#endif // TALKPANEL_H
