/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "JmapClient.h"
#include "accountstate.h"
#include "account.h"
#include "networkjobs.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>

namespace OCC {

// ——————————————————————————————————————————————————————————————————— Construction

JmapClient::JmapClient(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _nam(new QNetworkAccessManager(this))
{
    resolveSession();
}

// ——————————————————————————————————————————————————————————————————— Token / URL

void JmapClient::setBearerToken(const QString &token)
{
    _bearerToken = token;
}

QString JmapClient::jmapUrl() const
{
    if (!_apiUrl.isEmpty()) return _apiUrl + QLatin1String("/jmap");
    const auto acc = _accountState ? _accountState->account() : nullptr;
    return acc ? acc->url().toString() + QLatin1String("/jmap") : QString();
}

// ——————————————————————————————————————————————————————————————————— Session

void JmapClient::resolveSession()
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc || _bearerToken.isEmpty()) {
        emit sessionError(QLatin1String("No account or token"));
        return;
    }

    QUrl url(acc->url().toString().replace(QLatin1String("/index.php"), QLatin1String("")));
    url.setPath(url.path() + QLatin1String("/jmap/session"));

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + _bearerToken).toUtf8());
    req.setRawHeader("Accept", "application/json");

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit sessionError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto obj = doc.object();
        _apiUrl = obj.value(QLatin1String("apiUrl")).toString();
        const auto primary = obj.value(QLatin1String("primaryAccounts")).toObject();
        _accountId = primary.value(QLatin1String("urn:ietf:params:jmap:mail")).toString();
        if (_accountId.isEmpty()) {
            emit sessionError(QLatin1String("No JMAP mail accountId in session"));
            return;
        }
        emit sessionResolved(_accountId, _apiUrl);
    });
}

// ——————————————————————————————————————————————————————————————————— Helpers

void JmapClient::jmapCall(const QString &method, const QJsonObject &args,
                           std::function<void(const QJsonObject &)> callback)
{
    const QList<QPair<QString, QJsonObject>> calls = {{method, args}};
    jmapBatch(calls, [callback](const QJsonArray &responses) {
        if (!responses.isEmpty()) callback(responses.first().toObject());
    });
}

void JmapClient::jmapBatch(const QList<QPair<QString, QJsonObject>> &calls,
                            std::function<void(const QJsonArray &)> callback)
{
    const QUrl url = jmapUrl();
    QJsonArray methodCalls;
    int id = 0;
    for (const auto &pair : calls) {
        QJsonArray triple;
        triple.append(pair.first);
        triple.append(pair.second);
        triple.append(QStringLiteral("c%1").arg(id++));
        methodCalls.append(triple);
    }

    QJsonObject body;
    body[QLatin1String("using")] = QJsonArray{
        QLatin1String("urn:ietf:params:jmap:mail"),
        QLatin1String("urn:ietf:params:jmap:submission")
    };
    body[QLatin1String("methodCalls")] = methodCalls;

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + _bearerToken).toUtf8());
    req.setRawHeader("Content-Type", "application/json");
    req.setRawHeader("Accept", "application/json");

    auto *reply = _nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            callback({});
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto arr = doc.object().value(QLatin1String("methodResponses")).toArray();
        QJsonArray results;
        for (const auto &val : arr) {
            const auto triple = val.toArray();
            if (triple.size() >= 2) results.append(triple.at(1).toArray().isEmpty()
                ? triple.at(1).toObject() : QJsonObject{{QLatin1String("data"), triple.at(1).toArray()}});
        }
        callback(results);
    });
}

// ——————————————————————————————————————————————————————————————————— Mailboxes

void JmapClient::fetchMailboxes()
{
    QJsonObject args;
    args[QLatin1String("accountId")] = _accountId;
    jmapCall(QLatin1String("Mailbox/get"), args, [this](const QJsonObject &resp) {
        const auto list = resp.value(QLatin1String("list")).toArray();
        QList<JmapMailbox> mboxes;
        for (const auto &v : list) {
            const auto mb = v.toObject();
            JmapMailbox m;
            m.id = mb.value(QLatin1String("id")).toString();
            m.name = mb.value(QLatin1String("name")).toString();
            m.role = mb.value(QLatin1String("role")).toString();
            m.totalEmails = mb.value(QLatin1String("totalEmails")).toInt();
            m.unreadEmails = mb.value(QLatin1String("unreadEmails")).toInt();
            m.parentId = mb.value(QLatin1String("parentId")).toString();
            mboxes.append(m);
        }
        emit mailboxesFetched(mboxes);
    });
}

// ——————————————————————————————————————————————————————————————————— Emails

void JmapClient::queryEmails(const QString &mailboxId, int limit, int offset)
{
    QJsonObject filter;
    if (!mailboxId.isEmpty()) filter[QLatin1String("inMailbox")] = mailboxId;

    QJsonObject args;
    args[QLatin1String("accountId")] = _accountId;
    args[QLatin1String("filter")] = filter;
    QJsonArray sort;
    QJsonObject sortItem;
    sortItem[QLatin1String("property")] = QLatin1String("receivedAt");
    sortItem[QLatin1String("isAscending")] = false;
    sort.append(sortItem);
    args[QLatin1String("sort")] = sort;
    args[QLatin1String("position")] = offset;
    args[QLatin1String("limit")] = limit;

    jmapCall(QLatin1String("Email/query"), args, [this, limit](const QJsonObject &resp) {
        const auto ids = resp.value(QLatin1String("ids")).toArray();
        const int total = resp.value(QLatin1String("total")).toInt(ids.size());

        if (ids.isEmpty()) {
            emit emailsFetched({}, 0);
            return;
        }

        QJsonObject getArgs;
        getArgs[QLatin1String("accountId")] = _accountId;
        getArgs[QLatin1String("ids")] = ids;
        QJsonArray props;
        props.append(QLatin1String("id")); props.append(QLatin1String("subject"));
        props.append(QLatin1String("from")); props.append(QLatin1String("to"));
        props.append(QLatin1String("receivedAt")); props.append(QLatin1String("size"));
        props.append(QLatin1String("hasAttachment")); props.append(QLatin1String("keywords"));
        props.append(QLatin1String("threadId")); props.append(QLatin1String("preview"));
        getArgs[QLatin1String("properties")] = props;

        jmapCall(QLatin1String("Email/get"), getArgs, [this, total](const QJsonObject &resp2) {
            const auto list = resp2.value(QLatin1String("list")).toArray();
            QList<JmapEmail> emails;
            for (const auto &v : list) {
                const auto e = v.toObject();
                const auto from = e.value(QLatin1String("from")).toArray();
                const auto kw = e.value(QLatin1String("keywords")).toObject();
                JmapEmail m;
                m.id = e.value(QLatin1String("id")).toString();
                m.subject = e.value(QLatin1String("subject")).toString();
                m.fromAddress = from.isEmpty() ? QString() : from.first().toObject().value(QLatin1String("email")).toString();
                m.fromName = from.isEmpty() ? QString() : from.first().toObject().value(QLatin1String("name")).toString();
                m.toAddresses = e.value(QLatin1String("to")).toArray().isEmpty() ? QString() : e.value(QLatin1String("to")).toArray().first().toObject().value(QLatin1String("email")).toString();
                m.receivedAt = QDateTime::fromString(e.value(QLatin1String("receivedAt")).toString(), Qt::ISODate);
                m.isRead = kw.contains(QLatin1String("$seen"));
                m.isFlagged = kw.contains(QLatin1String("$flagged"));
                m.hasAttachments = e.value(QLatin1String("hasAttachment")).toBool();
                m.preview = e.value(QLatin1String("preview")).toString();
                m.threadId = e.value(QLatin1String("threadId")).toString();
                m.size = static_cast<qint64>(e.value(QLatin1String("size")).toDouble());
                emails.append(m);
            }
            emit emailsFetched(emails, total);
        });
    });
}

void JmapClient::fetchEmailBody(const QString &emailId)
{
    QJsonObject args;
    args[QLatin1String("accountId")] = _accountId;
    QJsonArray ids; ids.append(emailId);
    args[QLatin1String("ids")] = ids;
    QJsonArray props;
    props.append(QLatin1String("textBody")); props.append(QLatin1String("htmlBody"));
    props.append(QLatin1String("attachments")); props.append(QLatin1String("bodyValues"));
    args[QLatin1String("properties")] = props;
    args[QLatin1String("fetchTextBodyValues")] = true;
    args[QLatin1String("fetchHTMLBodyValues")] = true;

    jmapCall(QLatin1String("Email/get"), args, [this](const QJsonObject &resp) {
        const auto list = resp.value(QLatin1String("list")).toArray();
        if (list.isEmpty()) { emit emailBodyFetched({}); return; }
        const auto e = list.first().toObject();
        const auto bodyVals = e.value(QLatin1String("bodyValues")).toObject();

        JmapEmailBody body;
        const auto htmlArr = e.value(QLatin1String("htmlBody")).toArray();
        if (!htmlArr.isEmpty()) {
            const auto partId = htmlArr.first().toObject().value(QLatin1String("partId")).toString();
            body.htmlBody = bodyVals.value(partId).toObject().value(QLatin1String("value")).toString();
        }
        const auto textArr = e.value(QLatin1String("textBody")).toArray();
        if (!textArr.isEmpty()) {
            const auto partId = textArr.first().toObject().value(QLatin1String("partId")).toString();
            body.plainBody = bodyVals.value(partId).toObject().value(QLatin1String("value")).toString();
        }
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
    QJsonObject update;
    update[QLatin1String("keywords/$seen")] = read ? QJsonValue(true) : QJsonValue(QJsonValue::Null);

    QJsonObject emailUpdate; emailUpdate[emailId] = update;

    QJsonObject args;
    args[QLatin1String("accountId")] = _accountId;
    args[QLatin1String("update")] = emailUpdate;

    jmapCall(QLatin1String("Email/set"), args, [this](const QJsonObject &) {
        emit operationCompleted(true);
    });
}

void JmapClient::moveEmail(const QString &emailId, const QString &targetMailboxId)
{
    QJsonObject update;
    update[QLatin1String("mailboxIds")] = QJsonObject{{targetMailboxId, true}};
    QJsonObject emailUpdate; emailUpdate[emailId] = update;

    QJsonObject args;
    args[QLatin1String("accountId")] = _accountId;
    args[QLatin1String("update")] = emailUpdate;

    jmapCall(QLatin1String("Email/set"), args, [this](const QJsonObject &) {
        emit operationCompleted(true);
    });
}

void JmapClient::deleteEmail(const QString &emailId)
{
    markRead(emailId, true);
    // Move to trash: find trash mailbox first, then move
    fetchMailboxes(); // triggers mailboxesFetched — caller should connect a single-shot handler
}

void JmapClient::sendEmail(const QString &to, const QString &cc, const QString &bcc,
                            const QString &subject, const QString &bodyHtml,
                            const QString &inReplyTo)
{
    QJsonObject email;
    email[QLatin1String("subject")] = subject;
    QJsonArray toArr;
    for (const auto &a : to.split(QLatin1Char(','), Qt::SkipEmptyParts))
        toArr.append(QJsonObject{{QLatin1String("email"), a.trimmed()}});
    email[QLatin1String("to")] = toArr;
    if (!cc.isEmpty()) {
        QJsonArray ccArr;
        for (const auto &a : cc.split(QLatin1Char(','), Qt::SkipEmptyParts))
            ccArr.append(QJsonObject{{QLatin1String("email"), a.trimmed()}});
        email[QLatin1String("cc")] = ccArr;
    }
    if (!bcc.isEmpty()) {
        QJsonArray bccArr;
        for (const auto &a : bcc.split(QLatin1Char(','), Qt::SkipEmptyParts))
            bccArr.append(QJsonObject{{QLatin1String("email"), a.trimmed()}});
        email[QLatin1String("bcc")] = bccArr;
    }
    if (!inReplyTo.isEmpty()) email[QLatin1String("inReplyTo")] = QJsonArray{inReplyTo};
    email[QLatin1String("keywords")] = QJsonObject{{QLatin1String("$draft"), true}, {QLatin1String("$seen"), true}};
    email[QLatin1String("htmlBody")] = QJsonArray{QJsonObject{
        {QLatin1String("partId"), QLatin1String("1")}, {QLatin1String("type"), QLatin1String("text/html")}}};
    email[QLatin1String("bodyValues")] = QJsonObject{
        {QLatin1String("1"), QJsonObject{{QLatin1String("value"), bodyHtml}}}};

    // Step 1: Create draft email
    QJsonObject createArgs;
    createArgs[QLatin1String("accountId")] = _accountId;
    createArgs[QLatin1String("create")] = QJsonObject{{QLatin1String("draft1"), email}};

    // Step 2: Submit
    QJsonObject submitArgs;
    submitArgs[QLatin1String("accountId")] = _accountId;
    submitArgs[QLatin1String("create")] = QJsonObject{{QLatin1String("send1"), QJsonObject{
        {QLatin1String("emailId"), QLatin1String("#draft1")},
        {QLatin1String("identityId"), _accountId}}}};

    jmapBatch({{QLatin1String("Email/set"), createArgs}, {QLatin1String("EmailSubmission/set"), submitArgs}},
              [this](const QJsonArray &responses) {
        if (responses.isEmpty()) { emit emailSent(false, QLatin1String("Empty response")); return; }
        emit emailSent(true, QString());
    });
}

} // namespace OCC
