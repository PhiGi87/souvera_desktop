/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "JmapClient.h"
#include "accountstate.h"
#include "account.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace OCC {

JmapClient::JmapClient(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _nam(new QNetworkAccessManager(this))
{
}

void JmapClient::setCredentials(const QString &user, const QString &password)
{
    _user = user;
    _password = password;
}

QNetworkRequest JmapClient::makeRequest(const QString &path) const
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    QUrl url(acc ? acc->url() : QUrl());
    url.setPath(QLatin1String("/index.php/apps/souvera_mail") + path);

    QNetworkRequest req(url);
    const auto cred = QStringLiteral("%1:%2").arg(_user, _password).toUtf8().toBase64();
    req.setRawHeader("Authorization", "Basic " + cred);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("OCS-APIRequest", "true");
    return req;
}

void JmapClient::apiGet(const QString &path, std::function<void(const QJsonDocument &)> callback)
{
    auto *reply = _nam->get(makeRequest(path));
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        callback(QJsonDocument::fromJson(reply->readAll()));
    });
}

void JmapClient::apiPost(const QString &path, const QJsonObject &body,
                          std::function<void(const QJsonDocument &)> callback)
{
    auto req = makeRequest(path);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    auto *reply = _nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        callback(QJsonDocument::fromJson(reply->readAll()));
    });
}

// ——————————————————————————————————————————————————————————————————— Mailboxes

void JmapClient::fetchMailboxes()
{
    apiGet(QLatin1String("/api/v2/mailboxes"), [this](const QJsonDocument &doc) {
        const auto arr = doc.object().value(QLatin1String("mailboxes")).toArray();
        QList<JmapMailbox> mboxes;
        for (const auto &v : arr) {
            const auto mb = v.toObject();
            JmapMailbox m;
            m.id = mb.value(QLatin1String("id")).toString();
            m.name = mb.value(QLatin1String("name")).toString();
            m.role = mb.value(QLatin1String("role")).toString();
            m.totalEmails = mb.value(QLatin1String("total")).toInt();
            m.unreadEmails = mb.value(QLatin1String("unread")).toInt();
            m.parentId = mb.value(QLatin1String("parentId")).toString();
            mboxes.append(m);
        }
        emit mailboxesFetched(mboxes);
    });
}

// ——————————————————————————————————————————————————————————————————— Emails

void JmapClient::queryEmails(const QString &mailboxId, int limit, int offset,
                              const QString &searchQuery, const QString &filterType)
{
    QUrlQuery q;
    if (!mailboxId.isEmpty()) q.addQueryItem(QLatin1String("mailbox"), mailboxId);
    q.addQueryItem(QLatin1String("limit"), QString::number(limit));
    q.addQueryItem(QLatin1String("offset"), QString::number(offset));
    if (!searchQuery.isEmpty()) q.addQueryItem(QLatin1String("q"), searchQuery);
    if (!filterType.isEmpty() && filterType != QLatin1String("all"))
        q.addQueryItem(QLatin1String("filter"), filterType);

    auto path = QLatin1String("/api/v2/emails?") + q.toString(QUrl::FullyEncoded);
    apiGet(path, [this](const QJsonDocument &doc) {
        const auto obj = doc.object();
        const auto arr = obj.value(QLatin1String("emails")).toArray();
        const int total = obj.value(QLatin1String("total")).toInt();
        QList<JmapEmail> emails;
        for (const auto &v : arr) {
            const auto e = v.toObject();
            JmapEmail m;
            m.id = e.value(QLatin1String("id")).toString();
            m.subject = e.value(QLatin1String("subject")).toString();
            m.fromAddress = e.value(QLatin1String("fromAddress")).toString();
            m.fromName = e.value(QLatin1String("fromName")).toString();
            m.toAddresses = e.value(QLatin1String("toAddresses")).toString();
            m.receivedAt = QDateTime::fromString(e.value(QLatin1String("receivedAt")).toString(), Qt::ISODate);
            m.isRead = e.value(QLatin1String("isRead")).toBool();
            m.isFlagged = e.value(QLatin1String("isFlagged")).toBool();
            m.hasAttachments = e.value(QLatin1String("hasAttachment")).toBool();
            m.preview = e.value(QLatin1String("preview")).toString();
            m.threadId = e.value(QLatin1String("threadId")).toString();
            m.size = static_cast<qint64>(e.value(QLatin1String("size")).toDouble());
            emails.append(m);
        }
        emit emailsFetched(emails, total);
    });
}

void JmapClient::fetchEmailBody(const QString &emailId)
{
    apiGet(QLatin1String("/api/v2/emails/") + emailId, [this](const QJsonDocument &doc) {
        const auto e = doc.object().value(QLatin1String("email")).toObject();
        JmapEmailBody body;
        body.htmlBody = e.value(QLatin1String("htmlBody")).toString();
        body.plainBody = e.value(QLatin1String("plainBody")).toString();
        const auto atts = e.value(QLatin1String("attachments")).toArray();
        for (const auto &a : atts) {
            const auto ao = a.toObject();
            body.attachments.append({ao.value(QLatin1String("blobId")).toString(),
                                     ao.value(QLatin1String("name")).toString()});
        }
        emit emailBodyFetched(body);
    });
}

// ——————————————————————————————————————————————————————————————————— Actions

void JmapClient::markRead(const QString &emailId, bool read)
{
    QJsonObject body;
    body[QLatin1String("isRead")] = read ? 1 : 0;
    apiPost(QLatin1String("/api/v2/emails/") + emailId + QLatin1String("/read"), body,
            [this](const QJsonDocument &) { emit operationCompleted(true); });
}

void JmapClient::moveEmail(const QString &emailId, const QString &targetMailboxId)
{
    QJsonObject body;
    body[QLatin1String("mailboxId")] = targetMailboxId;
    apiPost(QLatin1String("/api/v2/emails/") + emailId + QLatin1String("/move"), body,
            [this](const QJsonDocument &) { emit operationCompleted(true); });
}

void JmapClient::deleteEmail(const QString &emailId)
{
    // The PHP proxy handles soft-delete (move to trash)
    auto req = makeRequest(QLatin1String("/api/v2/emails/") + emailId);
    auto *reply = _nam->deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        emit operationCompleted(reply->error() == QNetworkReply::NoError);
    });
}

void JmapClient::sendEmail(const QString &to, const QString &cc, const QString &bcc,
                            const QString &subject, const QString &bodyHtml,
                            const QString &inReplyTo)
{
    QJsonObject body;
    QJsonArray toArr;
    for (const auto &a : to.split(QLatin1Char(','), Qt::SkipEmptyParts))
        toArr.append(a.trimmed());
    body[QLatin1String("to")] = toArr;
    if (!cc.isEmpty()) {
        QJsonArray ccArr;
        for (const auto &a : cc.split(QLatin1Char(','), Qt::SkipEmptyParts))
            ccArr.append(a.trimmed());
        body[QLatin1String("cc")] = ccArr;
    }
    if (!bcc.isEmpty()) {
        QJsonArray bccArr;
        for (const auto &a : bcc.split(QLatin1Char(','), Qt::SkipEmptyParts))
            bccArr.append(a.trimmed());
        body[QLatin1String("bcc")] = bccArr;
    }
    body[QLatin1String("subject")] = subject;
    body[QLatin1String("bodyHtml")] = bodyHtml;
    if (!inReplyTo.isEmpty()) body[QLatin1String("inReplyTo")] = inReplyTo;

    apiPost(QLatin1String("/api/v2/send"), body, [this](const QJsonDocument &doc) {
        const auto success = doc.object().value(QLatin1String("success")).toBool();
        emit emailSent(success, success ? QString() : doc.object().value(QLatin1String("error")).toString());
    });
}

} // namespace OCC
