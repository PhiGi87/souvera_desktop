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
#include <QTextEdit>
#include <QPushButton>

namespace OCC {

class MailFolderModel;
class MailMessageModel;

class MailPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MailPanel(QWidget *parent = nullptr);
    ~MailPanel() override = default;

private:
    void setupUi();
    void onFolderSelected(const QModelIndex &index);
    void onMessageSelected(const QModelIndex &index);
    void onNewMessage();

    QSplitter *_splitter = nullptr;
    QTreeView *_folderView = nullptr;
    QListView *_messageView = nullptr;
    QTextEdit *_preview = nullptr;
    QPushButton *_newMsgBtn = nullptr;

    MailFolderModel *_folderModel = nullptr;
    MailMessageModel *_messageModel = nullptr;
};

} // namespace OCC

#endif // MAILPANEL_H
