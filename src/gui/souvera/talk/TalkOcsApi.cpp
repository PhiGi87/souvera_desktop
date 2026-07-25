/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkOcsApi.h"

#include "accountstate.h"
#include "account.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcTalkOcsApi, "souvera.talk.ocsapi")

namespace OCC {

TalkOcsApi::TalkOcsApi(QObject *parent)
    : QObject(parent)
{
}

void TalkOcsApi::setAccountState(AccountState *state)
{
    _accountState = state;
}

QString TalkOcsApi::ocsUrl(const QString &path) const
{
    if (!_accountState || !_accountState->account()) return {};
    const auto base = _accountState->account()->url().toString();
    const auto baseClean = base.endsWith(QLatin1Char('/')) ? base.chopped(1) : base;
    return QStringLiteral("%1/ocs/v2.php/apps/spreed/api/v1%2").arg(baseClean, path);
}

void TalkOcsApi::fetchConversations()
{
    const auto url = ocsUrl(QStringLiteral("/room"));
    if (url.isEmpty()) {
        qCWarning(lcTalkOcsApi) << "Cannot fetch conversations: no account state";
        return;
    }

    QNetworkRequest req;
    req.setRawHeader("OCS-APIRequest", "true");

    auto *account = _accountState->account().data();
    auto *reply = account->sendRawRequest("GET", QUrl(url), req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcTalkOcsApi) << "fetchConversations failed:" << reply->errorString();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto data = doc.object().value(QStringLiteral("ocs")).toObject()
                              .value(QStringLiteral("data")).toArray();
        qCInfo(lcTalkOcsApi) << "Fetched" << data.size() << "conversations";
        emit conversationsReceived(data);
    });
}

void TalkOcsApi::fetchMessages(const QString &token, qint64 lastKnownId)
{
    auto url = QUrl(ocsUrl(QStringLiteral("/chat/%1").arg(token)));
    if (!url.isValid()) {
        qCWarning(lcTalkOcsApi) << "Cannot fetch messages: no account state";
        return;
    }

    QUrlQuery query;
    if (lastKnownId > 0) {
        query.addQueryItem(QStringLiteral("lookIntoFuture"), QStringLiteral("1"));
        query.addQueryItem(QStringLiteral("lastKnownMessageId"), QString::number(lastKnownId));
        query.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    }
    url.setQuery(query);

    QNetworkRequest req;
    req.setRawHeader("OCS-APIRequest", "true");

    auto *account = _accountState->account().data();
    auto *reply = account->sendRawRequest("GET", url, req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, token]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcTalkOcsApi) << "fetchMessages failed:" << reply->errorString();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto data = doc.object().value(QStringLiteral("ocs")).toObject()
                              .value(QStringLiteral("data")).toArray();
        emit messagesReceived(data, token);
    });
}

void TalkOcsApi::sendMessage(const QString &token, const QString &text)
{
    const auto url = QUrl(ocsUrl(QStringLiteral("/chat/%1").arg(token)));
    if (!url.isValid()) {
        qCWarning(lcTalkOcsApi) << "Cannot send message: no account state";
        return;
    }

    QNetworkRequest req;
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("OCS-APIRequest", "true");

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("message"), text);

    auto *account = _accountState->account().data();
    auto *reply = account->sendRawRequest("POST", url, req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, token]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcTalkOcsApi) << "sendMessage failed:" << reply->errorString();
            return;
        }
        qCInfo(lcTalkOcsApi) << "Message sent to" << token;
        emit messageSent(token);
    });
}

} // namespace OCC
