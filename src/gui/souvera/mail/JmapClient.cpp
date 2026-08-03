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

QString JmapClient::baseUrl() const
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    return acc ? acc->url().toString().replace(QLatin1String("/index.php"), QLatin1String("")) : QString();
}

QString JmapClient::authHeader() const
{
    if (!_bearerToken.isEmpty())
        return QLatin1String("Bearer ") + _bearerToken;
    const auto cred = QStringLiteral("%1:%2").arg(_user, _password).toUtf8().toBase64();
    return QLatin1String("Basic ") + cred;
}

void JmapClient::setBearerToken(const QString &token)
{
    _bearerToken = token;
}

// ————————————————————————————————————————————————— Session Discovery (like Android)

void JmapClient::resolveSession()
{
    QUrl url(baseUrl() + QLatin1String("/jmap/session"));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", authHeader().toUtf8());
    req.setRawHeader("Accept", "application/json");

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == 401 && _bearerToken.isEmpty()) {
            emit needsBearerToken();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit sessionError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto obj = doc.object();
        _apiUrl = obj.value(QLatin1String("apiUrl")).toString().takeIf([](auto s){ return !s.isEmpty(); })
            .value_or(baseUrl() + QLatin1String("/jmap"));
        const auto primary = obj.value(QLatin1String("primaryAccounts")).toObject();
        _accountId = primary.value(QLatin1String("urn:ietf:params:jmap:mail")).toString();
        if (_accountId.isEmpty()) {
            emit sessionError(QLatin1String("No JMAP mail accountId in session"));
            return;
        }
        emit sessionResolved(_accountId, _apiUrl);
    });
}

// ————————————————————————————————————————————————— JMAP Batch Calls

void JmapClient::jmapCall(const QString &method, const QJsonObject &args,
                           std::function<void(const QJsonObject &)> callback)
{
    jmapBatch({{method, args}}, [callback](const QJsonArray &responses) {
        callback(responses.isEmpty() ? QJsonObject() : responses.first().toObject());
    });
}

void JmapClient::jmapBatch(const QList<QPair<QString, QJsonObject>> &calls,
                            std::function<void(const QJsonArray &)> callback)
{
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
        QLatin1String("urn:ietf:params:jmap:core"),
        QLatin1String("urn:ietf:params:jmap:mail"),
        QLatin1String("urn:ietf:params:jmap:submission")
    };
    body[QLatin1String("methodCalls")] = methodCalls;

    QUrl url(_apiUrl);
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", authHeader().toUtf8());
    req.setRawHeader("Content-Type", "application/json");
    req.setRawHeader("Accept", "application/json");

    auto *reply = _nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() == 401 && _bearerToken.isEmpty()) {
            emit needsBearerToken();
            callback({});
            return;
        }
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
            if (triple.size() >= 2) {
                const auto arg = triple.at(1).toObject();
                const auto name = triple.at(0).toString();
                if (name == QLatin1String("error") || arg.contains(QLatin1String("type")))
                    results.append(QJsonObject{{QLatin1String("error"), true}, {QLatin1String("detail"), arg}});
                else
                    results.append(QJsonObject{{QLatin1String("data"), arg}});
            }
        }
        callback(results);
    });
}

// ————————————————————————————————————————————————— Mailboxes

void JmapClient::fetchMailboxes()
{
    QJsonObject args;
    args[QLatin1String("accountId")] = _accountId;
    jmapCall(QLatin1String("Mailbox/get"), args, [this](const QJsonObject &resp) {
        const auto data = resp.value(QLatin1String("data")).toObject();
        const auto list = data.value(QLatin1String("list")).toArray();
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

// ————————————————————————————————————————————————— Emails

void JmapClient::queryEmails(const QString &mailboxId, int limit, int offset,
                              const QString &searchQuery, const QString &filterType)
{
    QJsonObject filter;
    if (!mailboxId.isEmpty()) filter[QLatin1String("inMailbox")] = mailboxId;
    if (!searchQuery.isEmpty()) filter[QLatin1String("text")] = searchQuery;
    if (filterType == QLatin1String("unread")) filter[QLatin1String("isUnread")] = true;
    if (filterType == QLatin1String("flagged")) filter[QLatin1String("isFlagged")] = true;
    if (filterType == QLatin1String("attachments")) filter[QLatin1String("hasAttachment")] = true;

    QJsonObject qArgs;
    qArgs[QLatin1String("accountId")] = _accountId;
    qArgs[QLatin1String("filter")] = filter.isEmpty() ? QJsonObject() : filter;
    qArgs[QLatin1String("sort")] = QJsonArray{QJsonObject{
        {QLatin1String("property"), QLatin1String("receivedAt")},
        {QLatin1String("isAscending"), false}}};
    qArgs[QLatin1String("position")] = offset;
    qArgs[QLatin1String("limit")] = limit;

    jmapCall(QLatin1String("Email/query"), qArgs, [this](const QJsonObject &resp) {
        const auto data = resp.value(QLatin1String("data")).toObject();
        const auto ids = data.value(QLatin1String("ids")).toArray();
        const int total = data.value(QLatin1String("total")).toInt(ids.size());

        if (ids.isEmpty()) { emit emailsFetched({}, 0); return; }

        QJsonObject gArgs;
        gArgs[QLatin1String("accountId")] = _accountId;
        gArgs[QLatin1String("ids")] = ids;
        gArgs[QLatin1String("properties")] = QJsonArray{
            QLatin1String("id"), QLatin1String("subject"), QLatin1String("from"),
            QLatin1String("receivedAt"), QLatin1String("size"), QLatin1String("hasAttachment"),
            QLatin1String("keywords"), QLatin1String("threadId"), QLatin1String("preview")};

        jmapCall(QLatin1String("Email/get"), gArgs, [this, total](const QJsonObject &r2) {
            const auto list = r2.value(QLatin1String("data")).toObject().value(QLatin1String("list")).toArray();
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
    args[QLatin1String("ids")] = QJsonArray{emailId};
    args[QLatin1String("properties")] = QJsonArray{
        QLatin1String("textBody"), QLatin1String("htmlBody"), QLatin1String("attachments"),
        QLatin1String("bodyValues")};
    args[QLatin1String("fetchTextBodyValues")] = true;
    args[QLatin1String("fetchHTMLBodyValues")] = true;

    jmapCall(QLatin1String("Email/get"), args, [this](const QJsonObject &resp) {
        const auto list = resp.value(QLatin1String("data")).toObject().value(QLatin1String("list")).toArray();
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
        emit emailBodyFetched(body);
    });
}

// ————————————————————————————————————————————————— Actions

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
    // Move to trash: find trash mailbox, then move
    QJsonObject args;
    args[QLatin1String("accountId")] = _accountId;
    jmapCall(QLatin1String("Mailbox/get"), args, [this, emailId](const QJsonObject &resp) {
        const auto list = resp.value(QLatin1String("data")).toObject().value(QLatin1String("list")).toArray();
        QString trashId;
        for (const auto &v : list) {
            if (v.toObject().value(QLatin1String("role")).toString() == QLatin1String("trash")) {
                trashId = v.toObject().value(QLatin1String("id")).toString();
                break;
            }
        }
        if (trashId.isEmpty()) {
            // No trash => hard-delete by marking deleted
            QJsonObject upd;
            upd[QLatin1String("keywords/$deleted")] = true;
            QJsonObject eu; eu[emailId] = upd;
            QJsonObject a;
            a[QLatin1String("accountId")] = _accountId;
            a[QLatin1String("update")] = eu;
            jmapCall(QLatin1String("Email/set"), a, [this](const QJsonObject &) { emit operationCompleted(true); });
        } else {
            moveEmail(emailId, trashId);
        }
    });
}

void JmapClient::sendEmail(const QString &to, const QString &cc, const QString &bcc,
                            const QString &subject, const QString &bodyHtml,
                            const QString &inReplyTo)
{
    QJsonObject email;
    email[QLatin1String("subject")] = subject;
    email[QLatin1String("keywords")] = QJsonObject{{QLatin1String("$draft"), true}, {QLatin1String("$seen"), true}};
    email[QLatin1String("htmlBody")] = QJsonArray{QJsonObject{
        {QLatin1String("partId"), QLatin1String("1")}, {QLatin1String("type"), QLatin1String("text/html")}}};
    email[QLatin1String("bodyValues")] = QJsonObject{
        {QLatin1String("1"), QJsonObject{{QLatin1String("value"), bodyHtml}}}};

    auto addRecipients = [](const QString &list) {
        QJsonArray arr;
        for (const auto &a : list.split(QLatin1Char(','), Qt::SkipEmptyParts))
            arr.append(QJsonObject{{QLatin1String("email"), a.trimmed()}});
        return arr;
    };
    email[QLatin1String("to")] = addRecipients(to);
    if (!cc.isEmpty()) email[QLatin1String("cc")] = addRecipients(cc);
    if (!bcc.isEmpty()) email[QLatin1String("bcc")] = addRecipients(bcc);
    if (!inReplyTo.isEmpty()) email[QLatin1String("inReplyTo")] = QJsonArray{inReplyTo};

    QJsonObject ca;
    ca[QLatin1String("accountId")] = _accountId;
    ca[QLatin1String("create")] = QJsonObject{{QLatin1String("draft1"), email}};

    QJsonObject sa;
    sa[QLatin1String("accountId")] = _accountId;
    sa[QLatin1String("create")] = QJsonObject{{QLatin1String("send1"), QJsonObject{
        {QLatin1String("emailId"), QLatin1String("#draft1")},
        {QLatin1String("identityId"), _accountId}}}};

    jmapBatch({{QLatin1String("Email/set"), ca}, {QLatin1String("EmailSubmission/set"), sa}},
              [this](const QJsonArray &responses) {
        for (const auto &r : responses) {
            if (r.toObject().contains(QLatin1String("error"))) {
                emit emailSent(false, r.toObject().value(QLatin1String("detail")).toObject()
                    .value(QLatin1String("description")).toString(QLatin1String("Send failed")));
                return;
            }
        }
        emit emailSent(true, QString());
    });
}

} // namespace OCC
