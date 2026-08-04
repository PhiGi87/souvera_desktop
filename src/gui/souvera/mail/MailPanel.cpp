/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailPanel.h"
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
    if (_accountState == accountState) return;
    _accountState = accountState;
    if (accountState) {
        wireAccount(accountState);
    }
}

void MailPanel::wireAccount(AccountState *accountState)
{
    const auto acc = accountState->account();
    if (!acc) return;

    const auto creds = acc->credentials();
    if (!creds) return;
    const auto user = creds->user();
    const auto password = creds->password();
    const auto host = acc->url().host();

    auto email = QStringLiteral("%1@%2").arg(user, host);
    _sendAsCombo->clear();
    _sendAsCombo->addItem(email);

    const auto displayName = acc->displayName();
    if (!displayName.isEmpty() && displayName != user) {
        _sendAsCombo->setItemText(0, QStringLiteral("%1 <%2>").arg(displayName, email));
    }

    delete _jmapClient;
    _jmapClient = new JmapClient(accountState, this);
    _jmapClient->setCredentials(user, password);

    connect(_jmapClient, &JmapClient::sessionResolved, this, [this](const QString &, const QString &) {
        qCInfo(lcMailPanel) << "JMAP session resolved, fetching mailboxes";
        _jmapClient->fetchMailboxes();
    });

    connect(_jmapClient, &JmapClient::sessionError, this, [this](const QString &err) {
        qCWarning(lcMailPanel) << "JMAP session error:" << err;
    });

    connect(_jmapClient, &JmapClient::mailboxesFetched, this, [this](const QList<JmapMailbox> &boxes) {
        qCInfo(lcMailPanel) << "Mailboxes fetched:" << boxes.size();
        _folderModel->setMailboxes(boxes);
        if (_folderModel->rowCount() > 0) {
            _folderView->setCurrentIndex(_folderModel->index(0, 0));
        }
    });

    connect(_jmapClient, &JmapClient::emailsFetched, this, [this](const QList<JmapEmail> &emails, int total) {
        qCInfo(lcMailPanel) << "Emails fetched:" << emails.size() << "total:" << total;
        _messageModel->setEmails(emails);
        _messageModel->setTotal(total);
    });

    connect(_jmapClient, &JmapClient::emailBodyFetched, this, [this](const JmapEmailBody &body) {
        if (!body.htmlBody.isEmpty()) {
            _preview->setHtml(body.htmlBody);
        } else if (!body.plainBody.isEmpty()) {
            _preview->setPlainText(body.plainBody);
        }
    });

    connect(_jmapClient, &JmapClient::operationCompleted, this, [this](bool) {
        if (!_currentMailboxId.isEmpty()) {
            _jmapClient->queryEmails(_currentMailboxId);
        }
    });

    connect(_jmapClient, &JmapClient::networkError, this, [this](const QString &err) {
        qCWarning(lcMailPanel) << "JMAP network error:" << err;
    });

    _jmapClient->resolveSession();
}

void MailPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolbar();
    layout->addWidget(_toolbar);

    _splitter = new QSplitter(Qt::Horizontal, this);
    _splitter->setObjectName(QStringLiteral("MailSplitter"));

    auto *folderPanel = new QWidget(_splitter);
    folderPanel->setObjectName(QStringLiteral("MailFolderPanel"));
    auto *folderLayout = new QVBoxLayout(folderPanel);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(0);

    _folderView = new QTreeView(folderPanel);
    _folderView->setObjectName(QStringLiteral("MailFolderView"));
    _folderView->setHeaderHidden(true);
    _folderView->setFixedWidth(220);
    _folderView->setIndentation(16);
    _folderView->setAnimated(true);
    _folderView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    folderLayout->addWidget(_folderView);
    _splitter->addWidget(folderPanel);

    auto *messagePanel = new QWidget(_splitter);
    messagePanel->setObjectName(QStringLiteral("MailMessagePanel"));
    auto *messageLayout = new QVBoxLayout(messagePanel);
    messageLayout->setContentsMargins(0, 0, 0, 0);
    messageLayout->setSpacing(0);

    _messageView = new QListView(messagePanel);
    _messageView->setObjectName(QStringLiteral("MailMessageView"));
    _messageView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _messageView->setSelectionMode(QAbstractItemView::SingleSelection);
    _messageView->setAlternatingRowColors(true);
    messageLayout->addWidget(_messageView);
    _splitter->addWidget(messagePanel);

    auto *previewPanel = new QWidget(_splitter);
    previewPanel->setObjectName(QStringLiteral("MailPreviewPanel"));
    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    _preview = new QTextBrowser(previewPanel);
    _preview->setObjectName(QStringLiteral("MailPreview"));
    _preview->setOpenExternalLinks(true);
    _preview->setPlaceholderText(QStringLiteral("Nachricht ausw\u00E4hlen\u2026"));
    previewLayout->addWidget(_preview);
    _splitter->addWidget(previewPanel);

    _splitter->setSizes({220, 320, 500});
    _splitter->setStretchFactor(0, 0);
    _splitter->setStretchFactor(1, 1);
    _splitter->setStretchFactor(2, 2);

    layout->addWidget(_splitter, 1);

    _folderModel = new JmapMailboxModel(this);
    _folderView->setModel(_folderModel);

    _messageModel = new JmapEmailListModel(this);
    _messageView->setModel(_messageModel);

    setupConnections();
}

void MailPanel::setupToolbar()
{
    _toolbar = new QWidget(this);
    _toolbar->setObjectName(QStringLiteral("MailToolbar"));
    auto *toolbarLayout = new QHBoxLayout(_toolbar);
    toolbarLayout->setContentsMargins(16, 8, 16, 8);
    toolbarLayout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Mail"), _toolbar);
    title->setObjectName(QStringLiteral("MailToolbarTitle"));
    toolbarLayout->addWidget(title);

    toolbarLayout->addSpacing(16);

    _newMsgBtn = new QPushButton(QStringLiteral("Verfassen"), _toolbar);
    _newMsgBtn->setObjectName(QStringLiteral("MailComposeBtn"));
    toolbarLayout->addWidget(_newMsgBtn);

    _replyBtn = new QPushButton(QStringLiteral("Antworten"), _toolbar);
    _replyBtn->setObjectName(QStringLiteral("MailReplyBtn"));
    _replyBtn->setEnabled(false);
    toolbarLayout->addWidget(_replyBtn);

    _deleteBtn = new QPushButton(QStringLiteral("L\u00F6schen"), _toolbar);
    _deleteBtn->setObjectName(QStringLiteral("MailDeleteBtn"));
    _deleteBtn->setEnabled(false);
    toolbarLayout->addWidget(_deleteBtn);

    _refreshBtn = new QPushButton(QStringLiteral("Aktualisieren"), _toolbar);
    _refreshBtn->setObjectName(QStringLiteral("MailRefreshBtn"));
    toolbarLayout->addWidget(_refreshBtn);

    toolbarLayout->addStretch();

    auto *sendAsLabel = new QLabel(QStringLiteral("Senden als:"), _toolbar);
    sendAsLabel->setObjectName(QStringLiteral("MailSendAsLabel"));
    toolbarLayout->addWidget(sendAsLabel);

    _sendAsCombo = new QComboBox(_toolbar);
    _sendAsCombo->setObjectName(QStringLiteral("MailSendAsCombo"));
    _sendAsCombo->setMinimumWidth(200);
    toolbarLayout->addWidget(_sendAsCombo);
}

void MailPanel::setupConnections()
{
    connect(_newMsgBtn, &QPushButton::clicked, this, &MailPanel::onNewMessage);
    connect(_replyBtn, &QPushButton::clicked, this, &MailPanel::onReply);
    connect(_deleteBtn, &QPushButton::clicked, this, &MailPanel::onDelete);
    connect(_refreshBtn, &QPushButton::clicked, this, [this]() {
        if (_jmapClient) {
            _jmapClient->fetchMailboxes();
        }
    });

    connect(_folderView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &cur, const QModelIndex &) {
        onFolderSelected(cur);
    });
    connect(_messageView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &cur, const QModelIndex &) {
        onMessageSelected(cur);
    });
}

void MailPanel::onFolderSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;

    const auto mailboxId = _folderModel->mailboxIdForRow(index.row());
    if (mailboxId.isEmpty()) return;

    _currentMailboxId = mailboxId;
    qCInfo(lcMailPanel) << "Folder selected:" << index.data().toString();

    _messageModel->setEmails({});
    _preview->clear();
    _replyBtn->setEnabled(false);
    _deleteBtn->setEnabled(false);
    _selectedEmailId.clear();

    if (_jmapClient) {
        _jmapClient->queryEmails(mailboxId);
    }
}

void MailPanel::onMessageSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;

    _selectedEmailId = _messageModel->emailIdForRow(index.row());
    if (_selectedEmailId.isEmpty()) return;

    const auto subject = index.data(JmapEmailListModel::SubjectRole).toString();
    const auto from = index.data(JmapEmailListModel::FromNameRole).toString();
    const auto email = index.data(JmapEmailListModel::FromAddressRole).toString();
    const auto date = index.data(JmapEmailListModel::ReceivedAtRole).toDateTime();

    qCInfo(lcMailPanel) << "Message selected:" << subject;

    const auto fromDisplay = from.isEmpty() ? email : QStringLiteral("%1 <%2>").arg(from, email);
    const auto dateStr = date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy HH:mm")) : QString();

    _preview->setHtml(QStringLiteral(
        "<div style='border-bottom: 1px solid #1e293b; padding-bottom: 14px; margin-bottom: 14px;'>"
        "<h2 style='margin: 0 0 10px 0; color: #e2e8f0; font-size: 18px; font-weight: 700;'>%1</h2>"
        "<p style='color: #94a3b8; margin: 4px 0;'><b style='color: #cbd5e1;'>Von:</b> %2</p>"
        "<p style='color: #94a3b8; margin: 4px 0;'><b style='color: #cbd5e1;'>Datum:</b> %3</p>"
        "</div>"
        "<p style='color: #64748b;'>Nachricht wird geladen\u2026</p>")
        .arg(subject.toHtmlEscaped(), fromDisplay.toHtmlEscaped(), dateStr));

    _replyBtn->setEnabled(true);
    _deleteBtn->setEnabled(true);

    if (_jmapClient) {
        _jmapClient->fetchEmailBody(_selectedEmailId);
        _jmapClient->markRead(_selectedEmailId, true);
    }
}

void MailPanel::onNewMessage()
{
    if (!_jmapClient) {
        QMessageBox::information(this, QStringLiteral("Hinweis"),
                                  QStringLiteral("Kein Konto verbunden."));
        return;
    }

    auto *composer = new MailComposer(this);
    connect(composer, &MailComposer::sendRequested, this,
            [this](const QString &to, const QString &cc, const QString &bcc,
                   const QString &subject, const QString &body) {
        if (_jmapClient) {
            _jmapClient->sendEmail(to, cc, bcc, subject, body, QString());
        }
    });
    composer->setAttribute(Qt::WA_DeleteOnClose);
    composer->show();
}

void MailPanel::onReply()
{
    if (!_jmapClient || _selectedEmailId.isEmpty()) return;

    const auto idx = _messageView->currentIndex();
    if (!idx.isValid()) return;

    const auto from = idx.data(JmapEmailListModel::FromAddressRole).toString();
    const auto subject = idx.data(JmapEmailListModel::SubjectRole).toString();

    auto *composer = new MailComposer(this);
    composer->setTo(from);
    composer->setSubject(QStringLiteral("Re: %1").arg(subject));

    connect(composer, &MailComposer::sendRequested, this,
            [this](const QString &to, const QString &cc, const QString &bcc,
                   const QString &subject, const QString &body) {
        if (_jmapClient) {
            _jmapClient->sendEmail(to, cc, bcc, subject, body, _selectedEmailId);
        }
    });
    composer->setAttribute(Qt::WA_DeleteOnClose);
    composer->show();
}

void MailPanel::onDelete()
{
    if (_selectedEmailId.isEmpty() || !_jmapClient) return;

    const auto ret = QMessageBox::question(this, QStringLiteral("L\u00F6schen"),
                                            QStringLiteral("Nachricht wirklich l\u00F6schen?"),
                                            QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    _jmapClient->deleteEmail(_selectedEmailId);

    _preview->clear();
    _replyBtn->setEnabled(false);
    _deleteBtn->setEnabled(false);
    _selectedEmailId.clear();
}

} // namespace OCC
