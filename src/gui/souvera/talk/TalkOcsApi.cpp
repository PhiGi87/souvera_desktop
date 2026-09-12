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
    // Try the clean URL first (no /index.php). If it fails with 404 or
    // returns non-JSON (some reverse-proxy setups need /index.php),
    // conversationsRequest falls back internally.
    conversationsRequest(base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), false);
}

void TalkOcsApi::conversationsRequest(const QString &apiBase, bool isV1Retry)
{
    const auto url = apiBase + QStringLiteral("/room");
    qCInfo(lcTalkOcsApi) << "Fetching conversations from:" << url;
    OcsDavClient::ocsRequest(_accountState, "GET", url, {},
        [this](const QJsonValue &payload, int) {
            // OCS data for Talk rooms is an ARRAY of conversation objects
            const auto data = payload.toArray();
            qCInfo(lcTalkOcsApi) << "Conversations received:" << data.size()
                                  << "payload type:" << (payload.isArray() ? "array" : payload.isObject() ? "object" : "other")
                                  << "payload preview:" << QJsonDocument(payload.toObject()).toJson(QJsonDocument::Compact).left(200);
            if (data.isEmpty()) {
                qCWarning(lcTalkOcsApi) << "Talk returned 0 conversations!"
                                         << "Raw payload:" << QJsonDocument(payload.toObject()).toJson(QJsonDocument::Compact);
            }
            emit conversationsReceived(data);
        },
        [this, apiBase, isV1Retry, url](int status, const QString &message) {
            if (status == 404 && !isV1Retry) {
                // v4 not available (older Talk server) — fall back to v1
                qCInfo(lcTalkOcsApi) << "v4 returned 404, retrying with v1";
                conversationsRequest(baseUrlOf(_accountState) + QStringLiteral("/ocs/v2.php/apps/spreed/api/v1"), true);
                return;
            }
            qCWarning(lcTalkOcsApi) << "conversationsRequest FAILED:"
                                     << "status:" << status
                                     << "url_len:" << url.size()
                                     << "msg_len:" << message.size()
                                     << "msg_head:" << message.left(80);
            emit apiError(QStringLiteral("Link: %1").arg(message));
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

void TalkOcsApi::fetchSelfUser()
{
    // The Talk actorId is the plain Nextcloud user id (e.g. "users/jdoe" style
    // prefixes stripped server-side), which can differ from the DAV login
    // name. /cloud/user returns the authoritative id used in chat payloads.
    const auto base = baseUrlOf(_accountState);
    if (base.isEmpty()) return;

    OcsDavClient::ocsRequest(_accountState, "GET",
        base + QStringLiteral("/ocs/v2.php/cloud/user"), {},
        [this](const QJsonValue &payload, int) {
            const auto id = payload.toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty()) {
                emit selfUserReceived(id);
            }
        },
        [this](int status, const QString &message) {
            qCWarning(lcTalkOcsApi) << "fetchSelfUser failed:" << status << message;
        });
}

void TalkOcsApi::messagesRequest(const QString &apiBase, const QString &token, qint64 lastKnownId)
{
    QUrl url(apiBase + QStringLiteral("/chat/%1").arg(token));
    QUrlQuery query;
    // lookIntoFuture is a REQUIRED parameter of the Talk chat endpoint —
    // omitting it makes some Talk server versions answer with HTTP 500.
    if (lastKnownId > 0) {
        query.addQueryItem(QStringLiteral("lookIntoFuture"), QStringLiteral("1"));
        query.addQueryItem(QStringLiteral("lastKnownMessageId"), QString::number(lastKnownId));
        query.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    } else {
        query.addQueryItem(QStringLiteral("lookIntoFuture"), QStringLiteral("0"));
        query.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    }
    url.setQuery(query);

    OcsDavClient::ocsRequest(_accountState, "GET", url.toString(), {},
        [this, token](const QJsonValue &payload, int) {
            const auto data = payload.toArray();
            emit messagesReceived(data, token);
        },
        [this, apiBase, token, lastKnownId](int status, const QString &message) {
            if (status == 404 && apiBase.contains(QStringLiteral("v1"))) {
                messagesRequest(baseUrlOf(_accountState) + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), token, lastKnownId);
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
        [this, token](const QJsonValue &, int) {
            qCInfo(lcTalkOcsApi) << "Message sent to" << token;
            emit messageSent(token);
        },
        [this, apiBase, token, text](int status, const QString &message) {
            if (status == 404 && apiBase.contains(QStringLiteral("v1"))) {
                sendRequest(baseUrlOf(_accountState) + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), token, text);
                return;
            }
            qCWarning(lcTalkOcsApi) << "sendMessage failed:" << status << message;
            emit apiError(message);
        },
        {{QByteArray("Content-Type"), QByteArray("application/x-www-form-urlencoded")}});
}

} // namespace OCC
