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

// ---------------------------------------------------------------------------
// Native call layer — the mobile-app flow: register / deregister as a call
// participant over the REST API. Media transport (WebRTC) is a separate
// engine on top of this layer.
// ---------------------------------------------------------------------------

void TalkOcsApi::joinCall(const QString &token, int flags)
{
    const auto base = baseUrlOf(_accountState);
    if (base.isEmpty()) {
        emit apiError(QStringLiteral("Kein Konto verbunden."));
        return;
    }
    // v4 join; older servers keep the same endpoint since Talk 4.
    const auto url = base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4/room/%1/participants/active?flags=%2")
        .arg(token).arg(flags);
    OcsDavClient::ocsRequest(_accountState, "POST", url, {},
        [this, token](const QJsonValue &payload, int) {
            // The response carries the room session id that identifies this
            // participant's call session at the signaling backend.
            const auto sessionId = payload.toObject().value(QStringLiteral("sessionId")).toString();
            qCInfo(lcTalkOcsApi) << "Call joined:" << token << "session:" << sessionId.left(8);
            emit callJoined(token, sessionId);
        },
        [this](int status, const QString &message) {
            qCWarning(lcTalkOcsApi) << "joinCall failed:" << status << message;
            emit apiError(message);
        });
}

void TalkOcsApi::startCall(const QString &token, int flags)
{
    const auto base = baseUrlOf(_accountState);
    if (base.isEmpty()) {
        emit apiError(QStringLiteral("Kein Konto verbunden."));
        return;
    }
    // The web app (signaling.js joinCall) posts JSON to call/{token} after
    // the signaling room join — this is what sets the in-call state.
    const auto url = base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4/call/%1").arg(token);
    QJsonObject body;
    body.insert(QStringLiteral("flags"), flags);
    body.insert(QStringLiteral("silent"), false);
    body.insert(QStringLiteral("recordingConsent"), 0);
    body.insert(QStringLiteral("silentFor"), 0);
    OcsDavClient::ocsRequest(_accountState, "POST", url,
        QJsonDocument(body).toJson(QJsonDocument::Compact),
        [this, token](const QJsonValue &, int) {
            qCInfo(lcTalkOcsApi) << "Call started:" << token;
            emit callStarted(token);
        },
        [this](int status, const QString &message) {
            qCWarning(lcTalkOcsApi) << "startCall failed:" << status << message;
            emit apiError(message);
        },
        {{QByteArray("Content-Type"), QByteArray("application/json")}});
}

void TalkOcsApi::leaveCall(const QString &token)
{
    const auto base = baseUrlOf(_accountState);
    if (base.isEmpty()) return;

    // Web-app leave order (signaling.js leaveCall): first leave the call
    // (DELETE call/{token}, {all}), then the room session
    // (DELETE room/{token}/participants/active).
    const auto callUrl = base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4/call/%1").arg(token);
    QJsonObject body;
    body.insert(QStringLiteral("all"), false);
    OcsDavClient::ocsRequest(_accountState, "DELETE", callUrl,
        QJsonDocument(body).toJson(QJsonDocument::Compact),
        [this, token, base](const QJsonValue &, int) {
            qCInfo(lcTalkOcsApi) << "Call call-session left:" << token;
            const auto url = base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4/room/%1/participants/active").arg(token);
            OcsDavClient::ocsRequest(_accountState, "DELETE", url, {},
                [this, token](const QJsonValue &, int) {
                    qCInfo(lcTalkOcsApi) << "Call left:" << token;
                    emit callLeft(token);
                },
                [this, token](int status, const QString &message) {
                    qCWarning(lcTalkOcsApi) << "leaveCall (room) failed:" << status << message;
                    emit callLeft(token);
                });
        },
        [this, token](int status, const QString &message) {
            // A 404 here means no call was active for this room — the room
            // session still needs to be released.
            qCWarning(lcTalkOcsApi) << "leaveCall (call) failed:" << status << message;
            emit callLeft(token);
        },
        {{QByteArray("Content-Type"), QByteArray("application/json")}});
}

void TalkOcsApi::fetchParticipants(const QString &token)
{
    participantsRequest(baseUrlOf(_accountState) + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), token);
}

void TalkOcsApi::participantsRequest(const QString &apiBase, const QString &token)
{
    const auto url = apiBase + QStringLiteral("/room/%1/participants").arg(token);
    OcsDavClient::ocsRequest(_accountState, "GET", url, {},
        [this, token](const QJsonValue &payload, int) {
            QVector<TalkParticipant> participants;
            const auto data = payload.toArray();
            for (const auto &v : data) {
                const auto obj = v.toObject();
                TalkParticipant p;
                p.actorId = obj.value(QStringLiteral("actorId")).toString();
                p.displayName = obj.value(QStringLiteral("displayName")).toString();
                p.inCall = obj.value(QStringLiteral("inCall")).toInt();
                participants.append(p);
            }
            emit participantsReceived(token, participants);
        },
        [this, apiBase, token](int status, const QString &message) {
            if (status == 404 && apiBase.contains(QStringLiteral("v1"))) {
                participantsRequest(baseUrlOf(_accountState) + QStringLiteral("/ocs/v2.php/apps/spreed/api/v4"), token);
                return;
            }
            qCWarning(lcTalkOcsApi) << "fetchParticipants failed:" << status << message;
        });
}

} // namespace OCC
