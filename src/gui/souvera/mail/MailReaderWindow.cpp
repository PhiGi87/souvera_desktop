/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailReaderWindow.h"
#include "MailBodyView.h"
#include "MailComposer.h"
#include "MailLoginFlow.h"

#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"
#include "theme/SouveraTheme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTextBrowser>
#include <QPushButton>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace OCC {

MailReaderWindow::MailReaderWindow(AccountState *accountState, const JmapEmail &email,
                                   QWidget *parent)
    : QWidget(parent, Qt::Window)
    , _accountState(accountState)
    , _email(email)
{
    setWindowTitle(QStringLiteral("%1 \u2014 Souvera Mail").arg(
        email.subject.isEmpty() ? QStringLiteral("(kein Betreff)") : email.subject));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(900, 700);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *headerWidget = new QWidget(this);
    headerWidget->setObjectName(QStringLiteral("MailToolbar"));
    auto *headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(20, 12, 20, 14);
    headerLayout->setSpacing(6);

    _subjectLabel = new QLabel(this);
    _subjectLabel->setObjectName(QStringLiteral("MailReaderSubject"));
    _subjectLabel->setWordWrap(true);
    headerLayout->addWidget(_subjectLabel);

    _metaLabel = new QLabel(this);
    _metaLabel->setObjectName(QStringLiteral("MailReaderMeta"));
    _metaLabel->setWordWrap(true);
    headerLayout->addWidget(_metaLabel);

    auto *btnRow = new QWidget(headerWidget);
    auto *btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(0, 4, 0, 0);
    btnLayout->setSpacing(8);

    _replyBtn = new QPushButton(QStringLiteral("Antworten"), btnRow);
    _replyBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    _forwardBtn = new QPushButton(QStringLiteral("Weiterleiten"), btnRow);
    _forwardBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    _deleteBtn = new QPushButton(QStringLiteral("L\u00F6schen"), btnRow);
    _deleteBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));

    btnLayout->addWidget(_replyBtn);
    btnLayout->addWidget(_forwardBtn);
    btnLayout->addWidget(_deleteBtn);
    btnLayout->addStretch();

    headerLayout->addWidget(btnRow);
    layout->addWidget(headerWidget);

    _bodyView = new MailBodyView(this);
    _bodyView->setObjectName(QStringLiteral("MailPreview"));
    if (_accountState && _accountState->account()) {
        _bodyView->setNetworkAccessManager(_accountState->account()->networkAccessManager());
    }
    _bodyView->setOpenExternalLinks(false);
    _bodyView->setOpenLinks(false);
    layout->addWidget(_bodyView, 1);

    connect(_replyBtn, &QPushButton::clicked, this, &MailReaderWindow::onReply);
    connect(_forwardBtn, &QPushButton::clicked, this, &MailReaderWindow::onForward);
    connect(_deleteBtn, &QPushButton::clicked, this, &MailReaderWindow::onDelete);
    connect(_bodyView, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (url.toString().startsWith(QLatin1String("souvera-attachment:"))) {
            const auto blobId = url.toString()
                .mid(QStringLiteral("souvera-attachment:").size())
                .section(QLatin1Char('?'), 0, 0);
            const auto name = QUrlQuery(url).queryItemValue(QStringLiteral("name"));
            if (_client) {
                _client->downloadAttachment(blobId, name);
            }
            return;
        }
        if (url.scheme().startsWith(QLatin1String("http"))) {
            QDesktopServices::openUrl(url);
        }
    });

    buildHeader();
    loadBody();
}

void MailReaderWindow::buildHeader()
{
    const auto *theme = SouveraTheme::instance();
    Q_UNUSED(theme)

    _subjectLabel->setText(_email.subject.isEmpty()
        ? QStringLiteral("(kein Betreff)") : _email.subject.toHtmlEscaped());

    const auto fromDisplay = _email.fromName.isEmpty()
        ? _email.fromAddress
        : QStringLiteral("%1 <%2>").arg(_email.fromName, _email.fromAddress);
    const auto dateStr = _email.receivedAt.isValid()
        ? _email.receivedAt.toString(QStringLiteral("dddd, dd. MMMM yyyy HH:mm"))
        : QString();

    _metaLabel->setText(QStringLiteral(
        "<b style='color:#cbd5e1;'>Von:</b> %1<br>"
        "<b style='color:#cbd5e1;'>An:</b> %2<br>"
        "<b style='color:#cbd5e1;'>Datum:</b> %3")
        .arg(fromDisplay.toHtmlEscaped(),
             _email.toAddresses.toHtmlEscaped(), dateStr));
}

void MailReaderWindow::loadBody()
{
    if (!_accountState || !_accountState->account()) {
        _bodyView->setMailBody(QStringLiteral("<p style='color:#94a3b8;'>Kein Konto verbunden.</p>"));
        return;
    }

    _bodyView->setMailBody(QStringLiteral("<p style='color:#64748b;'>Nachricht wird geladen\u2026</p>"));

    const auto mailPassword = MailLoginFlow::cachedPassword(_accountState);
    if (mailPassword.isEmpty()) {
        _bodyView->setMailBody(QStringLiteral("<p style='color:#ef4444;'>Mail-Anmeldedaten nicht verf\u00FCgbar. "
                                              "Bitte das Mail-Panel einmal \u00F6ffnen und erneut versuchen.</p>"));
        return;
    }

    const auto acc = _accountState->account();
    const auto user = acc->credentials()->user();

    if (_client) {
        _client->deleteLater();
    }
    _client = new JmapClient(_accountState, this);
    _client->setCredentials(user, mailPassword);

    connect(_client, &JmapClient::sessionResolved, this, [this](const QString &, const QString &) {
        _client->fetchEmailBody(_email.id);
    });

    connect(_client, &JmapClient::emailBodyFetched, this, [this](const JmapEmailBody &body) {
        _body = body;
        auto html = body.htmlBody.isEmpty()
            ? body.plainBody.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))
            : body.htmlBody;

        if (!body.attachments.isEmpty()) {
            const auto *theme = SouveraTheme::instance();
            const auto accent = theme->color(SouveraTheme::Color::Accent).name();
            const auto muted = theme->color(SouveraTheme::Color::TextMuted).name();
            QStringList chips;
            for (const auto &att : body.attachments) {
                chips << QStringLiteral(
                    "<a href='souvera-attachment:%1?name=%2' "
                    "style='color:%4; text-decoration: none;'>\U0001F4CE %3</a>")
                    .arg(att.blobId,
                         QString::fromUtf8(QUrl::toPercentEncoding(att.fileName)),
                         att.fileName.toHtmlEscaped(), accent);
            }
            html += QStringLiteral(
                "<hr style='border:none; border-top:1px solid #22304a;' />"
                "<p style='color:%1; font-size:12px;'><b>Anh\u00E4nge:</b></p>"
                "<p style='line-height:2;'>%2</p>")
                .arg(muted, chips.join(QStringLiteral("&nbsp;&nbsp;")));
        }

        if (!html.isEmpty()) {
            _bodyView->setMailBody(html);
        }
    });

    connect(_client, &JmapClient::sessionError, this, [this](const QString &error) {
        _bodyView->setMailBody(QStringLiteral("<p style='color:#ef4444;'>%1</p>")
            .arg(error.toHtmlEscaped()));
    });

    connect(_client, &JmapClient::attachmentDownloaded, this, [this](const QString &, const QString &localPath) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
    });

    _client->resolveSession();
}

void MailReaderWindow::onReply()
{
    auto *composer = new MailComposer(this);
    composer->setTo(_email.fromAddress);
    composer->setSubject(QStringLiteral("Re: %1").arg(_email.subject));
    connect(composer, &MailComposer::sendRequested, this,
            [this](const QString &to, const QString &cc, const QString &bcc,
                   const QString &subject, const QString &body) {
        const auto mailPassword = MailLoginFlow::cachedPassword(_accountState);
        if (_accountState && !mailPassword.isEmpty()) {
            auto *sender = new JmapClient(_accountState, this);
            sender->setCredentials(_accountState->account()->credentials()->user(), mailPassword);
            // Resolve then send: the client needs a session first.
            connect(sender, &JmapClient::sessionResolved, sender,
                    [sender, to, cc, bcc, subject, body](const QString &, const QString &) {
                sender->sendEmail(to, cc, bcc, subject, body, QString());
            });
            sender->resolveSession();
        }
    });
    composer->setAttribute(Qt::WA_DeleteOnClose);
    composer->show();
}

void MailReaderWindow::onForward()
{
    auto *composer = new MailComposer(this);
    composer->setSubject(QStringLiteral("Fwd: %1").arg(_email.subject));
    composer->setBody(QStringLiteral(
        "\n\n---------- Weitergeleitete Nachricht ----------\n"
        "Von: %1\nDatum: %2\nBetreff: %3\n\n%4")
        .arg(_email.fromAddress,
             _email.receivedAt.toString(QStringLiteral("dd.MM.yyyy HH:mm")),
             _email.subject, _body.plainBody));
    connect(composer, &MailComposer::sendRequested, this,
            [this](const QString &to, const QString &cc, const QString &bcc,
                   const QString &subject, const QString &body) {
        const auto mailPassword = MailLoginFlow::cachedPassword(_accountState);
        if (_accountState && !mailPassword.isEmpty()) {
            auto *sender = new JmapClient(_accountState, this);
            sender->setCredentials(_accountState->account()->credentials()->user(), mailPassword);
            connect(sender, &JmapClient::sessionResolved, sender,
                    [sender, to, cc, bcc, subject, body](const QString &, const QString &) {
                sender->sendEmail(to, cc, bcc, subject, body, QString());
            });
            sender->resolveSession();
        }
    });
    composer->setAttribute(Qt::WA_DeleteOnClose);
    composer->show();
}

void MailReaderWindow::onDelete()
{
    const auto ret = QMessageBox::question(this, QStringLiteral("L\u00F6schen"),
        QStringLiteral("Diese Nachricht wirklich l\u00F6schen?"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    if (_client) {
        _client->deleteEmail(_email.id);
    }
    emit emailDeleted(_email.id);
    close();
}

} // namespace OCC
