/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILREADERWINDOW_H
#define MAILREADERWINDOW_H

#include "JmapClient.h"

#include <QWidget>

class QLabel;
class QTextBrowser;
class MailBodyView;
class QPushButton;

namespace OCC {

class MailBodyView;

class AccountState;
class MailComposer;

/**
 * @brief Full-mail reader window opened on double-click (Thunderbird-style).
 */
class MailReaderWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MailReaderWindow(AccountState *accountState, const JmapEmail &email,
                              QWidget *parent = nullptr);

signals:
    void replyRequested(const JmapEmail &email, bool replyAll);
    void forwardRequested(const JmapEmail &email);
    void emailDeleted(const QString &emailId);

private:
    void loadBody();
    void buildHeader();
    void onReply();
    void onForward();
    void onDelete();

    AccountState *_accountState = nullptr;
    JmapEmail _email;
    JmapEmailBody _body;
    JmapClient *_client = nullptr;
    QLabel *_subjectLabel = nullptr;
    QLabel *_metaLabel = nullptr;
    MailBodyView *_bodyView = nullptr;
    QPushButton *_replyBtn = nullptr;
    QPushButton *_forwardBtn = nullptr;
    QPushButton *_deleteBtn = nullptr;
};

} // namespace OCC

#endif // MAILREADERWINDOW_H
