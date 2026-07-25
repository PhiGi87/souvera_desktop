/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILPANEL_H
#define MAILPANEL_H

#include <QWidget>
#include <QSplitter>
#include <QTreeView>
#include <QListView>
#include <QTextBrowser>
#include <QPushButton>
#include <QComboBox>

namespace OCC {

class MailFolderModel;
class MailMessageModel;
class MailAccount;
class AccountState;

class MailPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MailPanel(QWidget *parent = nullptr);
    ~MailPanel() override;

    void setAccountState(AccountState *accountState);

private:
    void setupUi();
    void setupToolbar();
    void setupConnections();

    void onFolderSelected(const QModelIndex &index);
    void onMessageSelected(const QModelIndex &index);
    void onNewMessage();
    void onReply();
    void onDelete();
    void onRefresh();
    void onSendAsChanged(int index);

    void connectMailAccount();
    void onFoldersFetched(const QList<ImapFolderData> &folders);
    void onMessagesFetched(const QList<ImapMessageData> &messages);
    void onBodyFetched(int seq, const QString &htmlBody, const QString &plainBody);
    void onImapConnected();

    QSplitter *_splitter = nullptr;
    QTreeView *_folderView = nullptr;
    QListView *_messageView = nullptr;
    QTextBrowser *_preview = nullptr;
    QWidget *_toolbar = nullptr;
    QPushButton *_newMsgBtn = nullptr;
    QPushButton *_replyBtn = nullptr;
    QPushButton *_deleteBtn = nullptr;
    QPushButton *_refreshBtn = nullptr;
    QComboBox *_sendAsCombo = nullptr;

    MailFolderModel *_folderModel = nullptr;
    MailMessageModel *_messageModel = nullptr;
    MailAccount *_mailAccount = nullptr;
    AccountState *_accountState = nullptr;

    int _selectedMessageSeq = -1;
    QString _currentFolder;
};

} // namespace OCC

#endif // MAILPANEL_H
