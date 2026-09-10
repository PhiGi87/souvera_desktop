/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILPANEL_H
#define MAILPANEL_H

#include "JmapClient.h"
#include "MailLoginFlow.h"
#include "JmapMailboxModel.h"
#include "JmapEmailListModel.h"

#include <QWidget>
#include <QSplitter>
#include <QTreeView>
#include <QListView>
#include <QTextBrowser>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QTimer>

namespace OCC {

class AccountState;
class EmailListDelegate;

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
    void wireAccount(AccountState *accountState);
    void startJmap(const QString &user, const QString &mailPassword);
    void remintMailPassword();
    void setStatus(const QString &text, bool isError = false);

    void onFolderSelected(const QModelIndex &index);
    void onMessageDoubleClicked(const QModelIndex &index);
    void applyViewSettings();
    void onMessageSelected(const QModelIndex &index);
    void onNewMessage();
    void onReply();
    void onDelete();
    void onRefresh();
    void runSearch();

    QSplitter *_rootSplitter = nullptr;
    QSplitter *_rightSplitter = nullptr;
    QTreeView *_folderView = nullptr;
    QListView *_messageView = nullptr;
    QTextBrowser *_preview = nullptr;
    QWidget *_toolbar = nullptr;
    QLabel *_statusLabel = nullptr;
    QLineEdit *_searchEdit = nullptr;
    QTimer _searchTimer;
    QPushButton *_newMsgBtn = nullptr;
    QPushButton *_replyBtn = nullptr;
    QPushButton *_deleteBtn = nullptr;
    QPushButton *_refreshBtn = nullptr;
    QComboBox *_sendAsCombo = nullptr;
    EmailListDelegate *_messageDelegate = nullptr;
    QString _mailUser;
    bool _mailRemintTried = false;
    bool _verticalLayout = false;
    bool _showPreviewLines = true;

    JmapMailboxModel *_folderModel = nullptr;
    JmapEmailListModel *_messageModel = nullptr;
    JmapClient *_jmapClient = nullptr;
    AccountState *_accountState = nullptr;

    QString _selectedEmailId;
    QString _currentMailboxId;
};

} // namespace OCC

#endif // MAILPANEL_H
