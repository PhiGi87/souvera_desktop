/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkOcsApi.h"
#include "net/OcsDavClient.h"

#include "accountstate.h"
#include "account.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcTalkOcsApi, "souvera.talk.ocsapi")

namespace OCC {

namespace {

QString baseUrlOf(AccountState *state)
{
    if (!state || !state->account()) return QString();
    auto base = state->account()->url().toString();
    if (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base;
}

QByteArray chatBody(const QString &text)
{
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("message"), text);
    return body.toString(QUrl::FullyEncoded).toUtf8();
}

} // namespace

TalkOcsApi::TalkOcsApi(QObject *parent)
    : QObject(parent)
{
}

void TalkOcsApi::setAccountState(AccountState *state)
{
    _accountState = state;
}

void TalkOcsApi::fetchConversations()
{
    const auto base = baseUrlOf(_accountState);
    if (base.isEmpty()) {
        emit apiError(QStringLiteral("Kein Konto verbunden."));
        return;
    }
    // Talk 4.x endpoint; falls back to v1 when the server is older.
    conversationsRequest(base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v1"), false);
}

void TalkOcsApi::conversationsRequest(const QString &apiBase, bool isV1Retry)
{
    const auto url = apiBase + QStringLiteral("/room");
    OcsDavClient::ocsRequest(_accountState, "GET", url, {},
        [this](const QJsonObject &payload, int) {
            const auto data = payload.value(QStringLiteral("data")).toArray();
            qCInfo(lcTalkOcsApi) << "Fetched" << data.size() << "conversations";
            emit conversationsReceived(data);
        },
        [this, apiBase, isV1Retry](int status, const QString &message) {
            if (status == 404 && !isV1Retry) {
                conversationsRequest(QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), true);
                return;
            }
            qCWarning(lcTalkOcsApi) << "fetchConversations failed:" << status << message;
            emit apiError(message);
        });
}

void TalkOcsApi::fetchMessages(const QString &token, qint64 lastKnownId)
{
    const auto base = baseUrlOf(_accountState);
    if (base.isEmpty()) {
        emit apiError(QStringLiteral("Kein Konto verbunden."));
        return;
    }
    messagesRequest(base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v1"), token, lastKnownId);
}

void TalkOcsApi::messagesRequest(const QString &apiBase, const QString &token, qint64 lastKnownId)
{
    QUrl url(apiBase + QStringLiteral("/chat/%1").arg(token));
    QUrlQuery query;
    if (lastKnownId > 0) {
        query.addQueryItem(QStringLiteral("lookIntoFuture"), QStringLiteral("1"));
        query.addQueryItem(QStringLiteral("lastKnownMessageId"), QString::number(lastKnownId));
        query.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    }
    url.setQuery(query);

    OcsDavClient::ocsRequest(_accountState, "GET", url.toString(), {},
        [this, token](const QJsonObject &payload, int) {
            const auto data = payload.value(QStringLiteral("data")).toArray();
            emit messagesReceived(data, token);
        },
        [this, apiBase, token, lastKnownId](int status, const QString &message) {
            if (status == 404 && apiBase.contains(QStringLiteral("v1"))) {
                messagesRequest(QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), token, lastKnownId);
                return;
            }
            qCWarning(lcTalkOcsApi) << "fetchMessages failed:" << status << message;
            emit apiError(message);
        });
}

void TalkOcsApi::sendMessage(const QString &token, const QString &text)
{
    const auto base = baseUrlOf(_accountState);
    if (base.isEmpty()) {
        emit apiError(QStringLiteral("Kein Konto verbunden."));
        return;
    }
    sendRequest(base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v1"), token, text);
}

void TalkOcsApi::sendRequest(const QString &apiBase, const QString &token, const QString &text)
{
    const auto url = apiBase + QStringLiteral("/chat/%1").arg(token);
    // Talk API contract: form-urlencoded body with the "message" field.
    OcsDavClient::ocsRequest(_accountState, "POST", url, chatBody(text),
        [this, token](const QJsonObject &, int) {
            qCInfo(lcTalkOcsApi) << "Message sent to" << token;
            emit messageSent(token);
        },
        [this, apiBase, token, text](int status, const QString &message) {
            if (status == 404 && apiBase.contains(QStringLiteral("v1"))) {
                sendRequest(QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), token, text);
                return;
            }
            qCWarning(lcTalkOcsApi) << "sendMessage failed:" << status << message;
            emit apiError(message);
        },
        {{QByteArray("Content-Type"), QByteArray("application/x-www-form-urlencoded")}});
}

} // namespace OCC
