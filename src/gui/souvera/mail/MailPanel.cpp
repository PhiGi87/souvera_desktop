/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailPanel.h"
#include "MailFolderModel.h"
#include "MailMessageModel.h"
#include "MailAccount.h"
#include "MailComposer.h"
#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcMailPanel, "souvera.mail.panel")

namespace OCC {

MailPanel::MailPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

MailPanel::~MailPanel() = default;

void MailPanel::setAccountState(AccountState *accountState)
{
    _accountState = accountState;
    if (!accountState) return;

    auto *acc = accountState->account();
    if (!acc) return;

    auto *creds = acc->credentials();

    _sendAsCombo->clear();
    auto email = QStringLiteral("%1@%2").arg(creds->user(), acc->url().host());
    _sendAsCombo->addItem(email);

    auto displayName = acc->displayName();
    if (!displayName.isEmpty() && displayName != creds->user()) {
        _sendAsCombo->setItemText(0, QStringLiteral("%1 <%2>").arg(displayName, email));
    }

    connectMailAccount();
    onRefresh();
}

void MailPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolbar();
    layout->addWidget(_toolbar);

    _splitter = new QSplitter(Qt::Horizontal, this);

    auto *folderPanel = new QWidget(_splitter);
    auto *folderLayout = new QVBoxLayout(folderPanel);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(0);

    auto *folderHeader = new QLabel(QStringLiteral("Ordner"), folderPanel);
    folderHeader->setStyleSheet(QStringLiteral(
        "QLabel { font-weight: bold; padding: 8px 12px; background: #f0f0f0; border-bottom: 1px solid #ddd; }"));
    folderLayout->addWidget(folderHeader);

    _folderView = new QTreeView(folderPanel);
    _folderView->setHeaderHidden(true);
    _folderView->setFixedWidth(220);
    _folderView->setIndentation(12);
    _folderView->setAnimated(true);
    _folderView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    folderLayout->addWidget(_folderView);

    _splitter->addWidget(folderPanel);

    auto *messagePanel = new QWidget(_splitter);
    auto *messageLayout = new QVBoxLayout(messagePanel);
    messageLayout->setContentsMargins(0, 0, 0, 0);
    messageLayout->setSpacing(0);

    auto *messageHeader = new QLabel(QStringLiteral("Nachrichten"), messagePanel);
    messageHeader->setStyleSheet(QStringLiteral(
        "QLabel { font-weight: bold; padding: 8px 12px; background: #f0f0f0; border-bottom: 1px solid #ddd; }"));
    messageLayout->addWidget(messageHeader);

    _messageView = new QListView(messagePanel);
    _messageView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _messageView->setSelectionMode(QAbstractItemView::SingleSelection);
    _messageView->setAlternatingRowColors(true);
    messageLayout->addWidget(_messageView);

    _splitter->addWidget(messagePanel);

    auto *previewPanel = new QWidget(_splitter);
    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    auto *previewHeader = new QLabel(QStringLiteral("Vorschau"), previewPanel);
    previewHeader->setStyleSheet(QStringLiteral(
        "QLabel { font-weight: bold; padding: 8px 12px; background: #f0f0f0; border-bottom: 1px solid #ddd; }"));
    previewLayout->addWidget(previewHeader);

    _preview = new QTextBrowser(previewPanel);
    _preview->setOpenExternalLinks(true);
    _preview->setPlaceholderText(QStringLiteral("Wählen Sie eine Nachricht aus…"));
    previewLayout->addWidget(_preview);

    _splitter->addWidget(previewPanel);

    _splitter->setSizes({220, 300, 500});
    _splitter->setStretchFactor(0, 0);
    _splitter->setStretchFactor(1, 1);
    _splitter->setStretchFactor(2, 2);

    layout->addWidget(_splitter, 1);

    _folderModel = new MailFolderModel(this);
    _folderView->setModel(_folderModel);

    _messageModel = new MailMessageModel(this);
    _messageView->setModel(_messageModel);

    setupConnections();
}

void MailPanel::setupToolbar()
{
    _toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(_toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);
    toolbarLayout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("E-Mail"), _toolbar);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; margin-right: 16px;"));
    toolbarLayout->addWidget(title);

    _newMsgBtn = new QPushButton(QStringLiteral("Neue Mail"), _toolbar);
    _newMsgBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4a90d9; color: white; border: none;"
        "  border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #357abd; }"
        "QPushButton:disabled { background-color: #ccc; }"));
    toolbarLayout->addWidget(_newMsgBtn);

    _replyBtn = new QPushButton(QStringLiteral("Antworten"), _toolbar);
    _replyBtn->setEnabled(false);
    toolbarLayout->addWidget(_replyBtn);

    _deleteBtn = new QPushButton(QStringLiteral("Löschen"), _toolbar);
    _deleteBtn->setEnabled(false);
    toolbarLayout->addWidget(_deleteBtn);

    _refreshBtn = new QPushButton(QStringLiteral("Aktualisieren"), _toolbar);
    toolbarLayout->addWidget(_refreshBtn);

    toolbarLayout->addStretch();

    auto *sendAsLabel = new QLabel(QStringLiteral("Senden als:"), _toolbar);
    toolbarLayout->addWidget(sendAsLabel);

    _sendAsCombo = new QComboBox(_toolbar);
    _sendAsCombo->setMinimumWidth(200);
    toolbarLayout->addWidget(_sendAsCombo);
}

void MailPanel::setupConnections()
{
    connect(_newMsgBtn, &QPushButton::clicked, this, &MailPanel::onNewMessage);
    connect(_replyBtn, &QPushButton::clicked, this, &MailPanel::onReply);
    connect(_deleteBtn, &QPushButton::clicked, this, &MailPanel::onDelete);
    connect(_refreshBtn, &QPushButton::clicked, this, &MailPanel::onRefresh);
    connect(_sendAsCombo, &QComboBox::currentIndexChanged, this, &MailPanel::onSendAsChanged);

    connect(_folderView, &QTreeView::clicked, this, &MailPanel::onFolderSelected);
    connect(_messageView, &QListView::clicked, this, &MailPanel::onMessageSelected);
}

void MailPanel::connectMailAccount()
{
    if (!_accountState) return;

    delete _mailAccount;
    _mailAccount = new MailAccount(_accountState, this);

    connect(_mailAccount, &MailAccount::imapConnected, this, &MailPanel::onImapConnected);
    connect(_mailAccount, &MailAccount::foldersFetched, this, &MailPanel::onFoldersFetched);
    connect(_mailAccount, &MailAccount::messagesFetched, this, &MailPanel::onMessagesFetched);
    connect(_mailAccount, &MailAccount::bodyFetched, this, &MailPanel::onBodyFetched);
    connect(_mailAccount, &MailAccount::imapConnectionError, this, [this](const QString &error) {
        qCWarning(lcMailPanel) << "IMAP error:" << error;
    });
}

void MailPanel::onFolderSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;
    auto folderName = index.data(MailFolderModel::NameRole).toString();
    _currentFolder = folderName;

    qCInfo(lcMailPanel) << "Folder selected:" << folderName;

    _messageModel->clear();
    _preview->clear();
    _replyBtn->setEnabled(false);
    _deleteBtn->setEnabled(false);
    _selectedMessageSeq = -1;

    if (_mailAccount && _mailAccount->isImapConnected()) {
        _mailAccount->fetchMessages(folderName);
    }
}

void MailPanel::onMessageSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;
    auto uid = index.data(MailMessageModel::UidRole).toInt();
    _selectedMessageSeq = uid;

    auto from = index.data(MailMessageModel::FromRole).toString();
    auto subject = index.data(MailMessageModel::SubjectRole).toString();
    auto date = index.data(MailMessageModel::DateDisplayRole).toString();

    qCInfo(lcMailPanel) << "Message selected - from:" << from << "subject:" << subject;

    _preview->setHtml(QStringLiteral(
        "<div style='border-bottom: 1px solid #ddd; padding-bottom: 12px; margin-bottom: 12px;'>"
        "<h2 style='margin: 0 0 8px 0;'>%1</h2>"
        "<p style='color: #666; margin: 2px 0;'><b>Von:</b> %2</p>"
        "<p style='color: #666; margin: 2px 0;'><b>Datum:</b> %3</p>"
        "</div>"
        "<p style='color: #999;'>Lade Nachricht…</p>")
        .arg(subject.toHtmlEscaped(), from.toHtmlEscaped(), date));

    _replyBtn->setEnabled(true);
    _deleteBtn->setEnabled(true);

    if (_mailAccount) {
        _mailAccount->fetchBody(uid);
    }
}

void MailPanel::onNewMessage()
{
    if (!_accountState) {
        QMessageBox::information(this, QStringLiteral("Hinweis"),
                                  QStringLiteral("Bitte zuerst ein Konto einrichten."));
        return;
    }

    auto *composer = new MailComposer(this);
    connect(composer, &MailComposer::sendRequested, this, [this](const QString &to, const QString &cc,
                                                                  const QString &bcc, const QString &subject,
                                                                  const QString &body) {
        if (_mailAccount) {
            _mailAccount->sendMail(to, cc, bcc, subject, body);
        }
    });
    composer->setAttribute(Qt::WA_DeleteOnClose);
    composer->show();
}

void MailPanel::onReply()
{
    if (_selectedMessageSeq < 0 || !_messageModel) return;

    auto idx = _messageView->currentIndex();
    if (!idx.isValid()) return;

    auto from = idx.data(MailMessageModel::FromRole).toString();
    auto subject = idx.data(MailMessageModel::SubjectRole).toString();

    auto *composer = new MailComposer(this);
    composer->setTo(from);
    composer->setSubject(QStringLiteral("Re: %1").arg(subject));

    connect(composer, &MailComposer::sendRequested, this, [this](const QString &to, const QString &cc,
                                                                  const QString &bcc, const QString &subject,
                                                                  const QString &body) {
        if (_mailAccount) {
            _mailAccount->sendMail(to, cc, bcc, subject, body);
        }
    });
    composer->setAttribute(Qt::WA_DeleteOnClose);
    composer->show();
}

void MailPanel::onDelete()
{
    if (_selectedMessageSeq < 0) return;

    auto ret = QMessageBox::question(this, QStringLiteral("Löschen"),
                                      QStringLiteral("Möchten Sie diese Nachricht wirklich löschen?"),
                                      QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    _messageModel->clear();
    _preview->clear();
    _replyBtn->setEnabled(false);
    _deleteBtn->setEnabled(false);
    _selectedMessageSeq = -1;

    qCInfo(lcMailPanel) << "Message deleted (seq:" << _selectedMessageSeq << ")";
}

void MailPanel::onRefresh()
{
    qCInfo(lcMailPanel) << "Refreshing mail...";

    if (!_mailAccount) {
        if (_accountState) {
            connectMailAccount();
        }
    }

    if (_mailAccount) {
        if (!_mailAccount->isImapConnected()) {
            _mailAccount->connectImap();
        } else {
            _mailAccount->fetchFolders();
        }
    }
}

void MailPanel::onSendAsChanged(int index)
{
    auto text = _sendAsCombo->currentText();
    qCInfo(lcMailPanel) << "Send-as changed to:" << text;
    Q_UNUSED(index)
}

void MailPanel::onImapConnected()
{
    qCInfo(lcMailPanel) << "IMAP connected, fetching folders";
    _mailAccount->fetchFolders();
}

void MailPanel::onFoldersFetched(const QList<MailFolderData> &folders)
{
    qCInfo(lcMailPanel) << "Folders received, count:" << folders.size();

    QList<FolderInfo> folderInfos;
    for (const auto &imapFolder : folders) {
        FolderInfo info;
        info.name = imapFolder.name;
        info.unreadCount = imapFolder.unseen;

        auto lower = imapFolder.name.toLower();
        if (lower == QStringLiteral("inbox")) info.icon = FolderIcon::Inbox;
        else if (lower == QStringLiteral("gesendet") || lower == QStringLiteral("sent")) info.icon = FolderIcon::Sent;
        else if (lower == QStringLiteral("entwürfe") || lower == QStringLiteral("drafts")) info.icon = FolderIcon::Drafts;
        else if (lower == QStringLiteral("gelöscht") || lower == QStringLiteral("papierkorb") || lower == QStringLiteral("trash")) info.icon = FolderIcon::Trash;
        else if (lower == QStringLiteral("spam") || lower == QStringLiteral("junk")) info.icon = FolderIcon::Junk;
        else if (lower == QStringLiteral("archiv") || lower == QStringLiteral("archive")) info.icon = FolderIcon::Archive;
        else info.icon = FolderIcon::Generic;

        folderInfos.append(info);
    }

    _folderModel->setFolders(folderInfos);
}

void MailPanel::onMessagesFetched(const QList<MailMessageData> &messages)
{
    qCInfo(lcMailPanel) << "Messages received, count:" << messages.size();

    QList<MailMessage> mailMessages;
    for (const auto &imapMsg : messages) {
        MailMessage msg;
        msg.uid = imapMsg.seq;
        msg.from = imapMsg.from;
        msg.subject = imapMsg.subject;
        msg.dateTime = imapMsg.dateTime;
        msg.unread = !imapMsg.seen;
        mailMessages.append(msg);
    }

    _messageModel->loadMessages(mailMessages);
}

void MailPanel::onBodyFetched(int seq, const QString &htmlBody, const QString &plainBody)
{
    Q_UNUSED(plainBody)
    if (seq != _selectedMessageSeq) return;

    if (!htmlBody.isEmpty()) {
        _preview->setHtml(htmlBody);
    } else if (!plainBody.isEmpty()) {
        _preview->setPlainText(plainBody);
    }
}

} // namespace OCC
