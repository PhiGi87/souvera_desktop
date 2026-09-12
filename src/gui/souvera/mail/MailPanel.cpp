/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailPanel.h"
#include "MailBodyView.h"
#include "MailComposer.h"
#include "EmailListDelegate.h"
#include "MailReaderWindow.h"
#include "net/SouveraAccountGate.h"
#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"
#include "theme/SouveraTheme.h"

#include <QDesktopServices>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QUrlQuery>

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
    const auto host = acc->url().host();

    // The login id is usually already the mail address — only append the
    // host for non-mail usernames.
    auto email = user.contains(QLatin1Char('@'))
        ? user
        : QStringLiteral("%1@%2").arg(user, host);
    _sendAsCombo->clear();
    _sendAsCombo->addItem(email);

    const auto displayName = acc->displayName();
    if (!displayName.isEmpty() && displayName != user) {
        _sendAsCombo->setItemText(0, QStringLiteral("%1 <%2>").arg(displayName, email));
    }

    // The Nextcloud app password X is NOT accepted by Stalwart/JMAP.
    // Like the Android/iOS clients, mint the combined mail password Y via
    // souvera_mail's login-flow endpoint and use THAT for JMAP.
    _mailUser = user;
    _mailRemintTried = false;
    setStatus(QStringLiteral("Mail-Anmeldung wird eingerichtet\u2026"));
    const auto accountGuard = QPointer<AccountState>(accountState);
    // Wait for the keychain fetch: requests fired with empty credentials
    // never complete.
    Sou::whenCredentialsReady(accountState, this, [this, accountGuard, accountState]() {
        if (!accountGuard || accountState != _accountState) return;
        MailLoginFlow::ensureCombinedPassword(accountState,
            [this, accountGuard, accountState](const CombinedAppPassword &result) {
                if (!accountGuard || accountState != _accountState) return;
                startJmap(_mailUser, result.appPassword);
            },
            [this, accountGuard](const QString &error) {
                if (!accountGuard) return;
                setStatus(error, true);
            });
    });
}

void MailPanel::startJmap(const QString &user, const QString &mailPassword)
{
    auto *accountState = _accountState;
    if (!accountState) return;

    setStatus(QStringLiteral("Verbinde mit Mail-Server\u2026"));

    if (_jmapClient) {
        _jmapClient->deleteLater();
        _jmapClient = nullptr;
    }
    if (_preview && accountState && accountState->account()) {
        _preview->setNetworkAccessManager(accountState->account()->networkAccessManager());
    }
    _jmapClient = new JmapClient(accountState, this);
    _jmapClient->setCredentials(user, mailPassword);

    connect(_jmapClient, &JmapClient::sessionResolved, this, [this](const QString &, const QString &) {
        qCInfo(lcMailPanel) << "JMAP session resolved, fetching mailboxes";
        setStatus(QStringLiteral("Angemeldet \u2014 Postf\u00E4cher werden geladen\u2026"));
        _jmapClient->fetchMailboxes();
    });

    connect(_jmapClient, &JmapClient::sessionError, this, [this](const QString &err) {
        qCWarning(lcMailPanel) << "JMAP session error:" << err;
        // A rejected combined password means the server re-provisioned:
        // re-mint once, then retry the session.
        if (!_mailRemintTried && err.contains(QStringLiteral("401"))) {
            _mailRemintTried = true;
            remintMailPassword();
            return;
        }
        setStatus(err, true);
    });

    connect(_jmapClient, &JmapClient::mailboxesFetched, this, [this](const QList<JmapMailbox> &boxes) {
        qCInfo(lcMailPanel) << "Mailboxes fetched:" << boxes.size();
        _folderModel->setMailboxes(boxes);
        _folderView->expandAll();
        // Always start in the inbox (Thunderbird-style), never in a random folder.
        const auto inbox = _folderModel->inboxIndex();
        if (inbox.isValid()) {
            _folderView->setCurrentIndex(inbox);
        }
        setStatus(QStringLiteral("Bereit \u2014 %1 Ordner").arg(boxes.size()));
    });

    connect(_jmapClient, &JmapClient::emailsFetched, this, [this](const QList<JmapEmail> &emails, int total) {
        qCInfo(lcMailPanel) << "Emails fetched:" << emails.size() << "total:" << total;
        _messageModel->setEmails(emails);
        _messageModel->setTotal(total);
        setStatus(total > static_cast<int>(emails.size())
                      ? QStringLiteral("%1 von %2 Nachrichten").arg(emails.size()).arg(total)
                      : QStringLiteral("%1 Nachrichten").arg(emails.size()));
    });

    connect(_jmapClient, &JmapClient::emailBodyFetched, this, [this](const JmapEmailBody &body) {
        const auto *theme = SouveraTheme::instance();
        const auto textPrimary = theme->color(SouveraTheme::Color::TextPrimary).name();
        const auto textMuted = theme->color(SouveraTheme::Color::TextMuted).name();
        const auto border = theme->color(SouveraTheme::Color::Border).name();
        const auto accent = theme->color(SouveraTheme::Color::Accent).name();

        auto html = body.htmlBody.isEmpty()
            ? body.plainBody.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))
            : body.htmlBody;

        if (!body.attachments.isEmpty()) {
            QStringList chips;
            for (const auto &att : body.attachments) {
                chips << QStringLiteral(
                    "<a href='souvera-attachment:%1?name=%2' "
                    "style='color:%4; text-decoration: none;'>\U0001F4CE %3</a>")
                    .arg(att.blobId,
                         QString::fromUtf8(QUrl::toPercentEncoding(att.fileName)),
                         att.fileName.toHtmlEscaped(),
                         accent);
            }
            html += QStringLiteral(
                "<hr style='border:none; border-top:1px solid %1;' />"
                "<p style='color:%2; font-size:12px;'><b>Anh\u00E4nge:</b></p>"
                "<p style='color:%3; line-height:2;'>%4</p>")
                .arg(border, textMuted, textPrimary, chips.join(QStringLiteral("&nbsp;&nbsp;")));
        }

        if (!html.isEmpty()) {
            _preview->setMailBody(html);
        }
    });

    connect(_jmapClient, &JmapClient::attachmentDownloaded, this, [this](const QString &fileName, const QString &localPath) {
        Q_UNUSED(fileName)
        setStatus(QStringLiteral("Anhang gespeichert: %1").arg(localPath));
        QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
    });

    connect(_jmapClient, &JmapClient::attachmentDownloadFailed, this, [this](const QString &fileName, const QString &error) {
        QMessageBox::warning(this, QStringLiteral("Anhang"),
            QStringLiteral("Anhang \u201E%1\u201C konnte nicht geladen werden:\n%2").arg(fileName, error));
    });

    connect(_jmapClient, &JmapClient::operationCompleted, this, [this](bool) {
        if (!_currentMailboxId.isEmpty()) {
            _jmapClient->queryEmails(_currentMailboxId, 50, 0, _searchEdit ? _searchEdit->text() : QString());
        }
    });

    connect(_jmapClient, &JmapClient::networkError, this, [this](const QString &err) {
        qCWarning(lcMailPanel) << "JMAP network error:" << err;
        setStatus(err, true);
    });

    _jmapClient->resolveSession();
}

void MailPanel::remintMailPassword()
{
    auto *accountState = _accountState;
    if (!accountState) return;

    setStatus(QStringLiteral("Mail-Passwort abgelehnt \u2014 neues wird angefordert\u2026"));

    const auto staleId = MailLoginFlow::cachedStalwartId(accountState);
    if (!staleId.isEmpty()) {
        MailLoginFlow::deleteCombinedPassword(accountState, staleId);
    }
    MailLoginFlow::clearCachedPassword(accountState);

    const auto accountGuard = QPointer<AccountState>(accountState);
    MailLoginFlow::ensureCombinedPassword(accountState,
        [this, accountGuard, accountState](const CombinedAppPassword &result) {
            if (!accountGuard || accountState != _accountState) return;
            startJmap(_mailUser, result.appPassword);
        },
        [this, accountGuard](const QString &error) {
            if (!accountGuard) return;
            setStatus(error, true);
        });
}

void MailPanel::setStatus(const QString &text, bool isError)
{
    if (!_statusLabel) return;
    _statusLabel->setText(text);
    const auto *theme = SouveraTheme::instance();
    _statusLabel->setProperty("error", isError);
    _statusLabel->setStyleSheet(QStringLiteral("color: %1; padding: 2px 14px; font-size: 11px; background: transparent;")
        .arg(isError ? theme->color(SouveraTheme::Color::Danger).name()
                     : theme->color(SouveraTheme::Color::TextMuted).name()));
}

void MailPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolbar();
    layout->addWidget(_toolbar);

    _rootSplitter = new QSplitter(Qt::Horizontal, this);
    _rootSplitter->setObjectName(QStringLiteral("MailSplitter"));
    _rightSplitter = new QSplitter(Qt::Horizontal, _rootSplitter);

    auto *folderPanel = new QWidget(_rootSplitter);
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
    _rootSplitter->addWidget(folderPanel);

    auto *messagePanel = new QWidget(_rootSplitter);
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
    _rightSplitter->addWidget(messagePanel);

    auto *previewPanel = new QWidget(_rootSplitter);
    previewPanel->setObjectName(QStringLiteral("MailPreviewPanel"));
    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    _preview = new MailBodyView(previewPanel);
    _preview->setObjectName(QStringLiteral("MailPreview"));
    _preview->setOpenExternalLinks(false);
    _preview->setOpenLinks(false);
    _preview->setPlaceholderText(QStringLiteral("Nachricht ausw\u00E4hlen\u2026"));
    previewLayout->addWidget(_preview);
    _rightSplitter->addWidget(previewPanel);

    _rootSplitter->addWidget(_rightSplitter);
    _rootSplitter->setStretchFactor(0, 0);
    _rootSplitter->setStretchFactor(1, 1);
    _rootSplitter->setSizes({220, 940});

    layout->addWidget(_rootSplitter, 1);

    _statusLabel = new QLabel(QStringLiteral("Kein Konto verbunden."), this);
    _statusLabel->setObjectName(QStringLiteral("MailStatusLabel"));
    setStatus(QStringLiteral("Kein Konto verbunden."));
    layout->addWidget(_statusLabel);

    _folderModel = new JmapMailboxModel(this);
    _folderView->setModel(_folderModel);

    _messageModel = new JmapEmailListModel(this);
    _messageView->setModel(_messageModel);

    _messageDelegate = new EmailListDelegate(_messageView);
    _messageView->setItemDelegate(_messageDelegate);
    _messageView->setAlternatingRowColors(false);
    _messageView->setUniformItemSizes(false);
    _messageView->setMouseTracking(true);
    connect(_messageView, &QListView::doubleClicked, this, &MailPanel::onMessageDoubleClicked);
    applyViewSettings();

    setupConnections();
}

void MailPanel::applyViewSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("souvera/mailview"));
    _verticalLayout = settings.value(QStringLiteral("verticalLayout"), false).toBool();
    _showPreviewLines = settings.value(QStringLiteral("previewLines"), true).toBool();
    settings.endGroup();

    _messageDelegate->setShowPreview(_showPreviewLines);
    _messageView->doItemsLayout();

    if (!_rightSplitter) return;
    _rightSplitter->setOrientation(_verticalLayout ? Qt::Vertical : Qt::Horizontal);
    if (_verticalLayout) {
        _rightSplitter->setSizes({400, 400});
    } else {
        _rightSplitter->setSizes({320, 620});
    }
    // NOTE: _rootSplitter sizes are set in setupUi() with explicit pixel
    // values and must not be touched here — calling setSizes with the
    // current (pre-show) widget width of 0 collapses both panes.
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

    _searchEdit = new QLineEdit(_toolbar);
    _searchEdit->setObjectName(QStringLiteral("MailSearchEdit"));
    _searchEdit->setPlaceholderText(QStringLiteral("Nachrichten durchsuchen\u2026"));
    _searchEdit->setMinimumWidth(200);
    _searchEdit->setClearButtonEnabled(true);
    toolbarLayout->addWidget(_searchEdit);

    toolbarLayout->addSpacing(8);

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
            setStatus(QStringLiteral("Aktualisiere\u2026"));
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

    // Attachment links from the preview; external links are handled by MailBodyView
    connect(_preview, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (url.toString().startsWith(QLatin1String("souvera-attachment:"))) {
            const auto blobId = url.toString().mid(QStringLiteral("souvera-attachment:").size()).section(QLatin1Char('?'), 0, 0);
            const auto name = QUrlQuery(url).queryItemValue(QStringLiteral("name"));
            if (_jmapClient && !blobId.isEmpty()) {
                setStatus(QStringLiteral("Lade Anhang \u201E%1\u201C\u2026").arg(name));
                _jmapClient->downloadAttachment(blobId, name);
            }
        }
    });

    // Debounced search
    _searchTimer.setSingleShot(true);
    _searchTimer.setInterval(400);
    connect(&_searchTimer, &QTimer::timeout, this, &MailPanel::runSearch);
    connect(_searchEdit, &QLineEdit::textChanged, this, [this]() {
        _searchTimer.start();
    });
}

void MailPanel::runSearch()
{
    if (!_jmapClient || _currentMailboxId.isEmpty()) return;
    setStatus(QStringLiteral("Suche\u2026"));
    _jmapClient->queryEmails(_currentMailboxId, 50, 0, _searchEdit->text().trimmed());
}

void MailPanel::onFolderSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;

    const auto mailboxId = _folderModel->mailboxIdForIndex(index);
    if (mailboxId.isEmpty()) return;

    _currentMailboxId = mailboxId;
    qCInfo(lcMailPanel) << "Folder selected:" << index.data().toString();

    _messageModel->setEmails({});
    _preview->clear();
    _replyBtn->setEnabled(false);
    _deleteBtn->setEnabled(false);
    _selectedEmailId.clear();

    if (_jmapClient) {
        _jmapClient->queryEmails(mailboxId, 50, 0,
                                  _searchEdit ? _searchEdit->text().trimmed() : QString());
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

    const auto *theme = SouveraTheme::instance();
    const auto border = theme->color(SouveraTheme::Color::Border).name();
    const auto textPrimary = theme->color(SouveraTheme::Color::TextPrimary).name();
    const auto textSecondary = theme->color(SouveraTheme::Color::TextSecondary).name();
    const auto textMuted = theme->color(SouveraTheme::Color::TextMuted).name();

    _preview->setMailBody(QStringLiteral(
        "<div style='border-bottom: 1px solid %4; padding-bottom: 14px; margin-bottom: 14px;'>"
        "<h2 style='margin: 0 0 10px 0; color: %5; font-size: 18px; font-weight: 700;'>%1</h2>"
        "<p style='color: %6; margin: 4px 0;'><b style='color: %5;'>Von:</b> %2</p>"
        "<p style='color: %6; margin: 4px 0;'><b style='color: %5;'>Datum:</b> %3</p>"
        "</div>"
        "<p style='color: %7;'>Nachricht wird geladen\u2026</p>")
        .arg(subject.toHtmlEscaped(), fromDisplay.toHtmlEscaped(), dateStr,
             border, textPrimary, textSecondary, textMuted));

    _replyBtn->setEnabled(true);
    _deleteBtn->setEnabled(true);

    if (_jmapClient) {
        _jmapClient->fetchEmailBody(_selectedEmailId);
        _jmapClient->markRead(_selectedEmailId, true);
    }
}

void MailPanel::onMessageDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || !_accountState) return;
    const auto emailId = _messageModel->emailIdForRow(index.row());
    if (emailId.isEmpty()) return;

    JmapEmail email;
    email.id = emailId;
    email.subject = index.data(JmapEmailListModel::SubjectRole).toString();
    email.fromName = index.data(JmapEmailListModel::FromNameRole).toString();
    email.fromAddress = index.data(JmapEmailListModel::FromAddressRole).toString();
    email.receivedAt = index.data(JmapEmailListModel::ReceivedAtRole).toDateTime();

    auto *reader = new MailReaderWindow(_accountState, email, this);
    reader->show();
    reader->raise();
    reader->activateWindow();

    if (_jmapClient) {
        _jmapClient->markRead(emailId, true);
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
