/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkSignalingClient.h"
#include "TalkOcsApi.h"

#include "account.h"
#include "accountstate.h"
#include "net/OcsDavClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

Q_LOGGING_CATEGORY(lcTalkSignaling, "souvera.talk.signaling")

namespace OCC {

TalkSignalingClient::TalkSignalingClient(QObject *parent)
    : QObject(parent)
{
}

TalkSignalingClient::~TalkSignalingClient() = default;

void TalkSignalingClient::setAccountState(AccountState *state)
{
    _accountState = state;
}

void TalkSignalingClient::joinRoom(const QString &roomToken, const QString &roomSessionId)
{
    _roomToken = roomToken;
    _roomSessionId = roomSessionId;
    _joinedRoom = false;
    fetchSettings();
}

void TalkSignalingClient::leaveRoom()
{
    _roomToken.clear();
    _joinedRoom = false;
    if (_socket && _socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject bye;
        bye.insert(QStringLiteral("type"), QStringLiteral("bye"));
        sendJson(bye);
        _socket->close();
    }
}

void TalkSignalingClient::fetchSettings()
{
    if (!_accountState || !_accountState->account()) {
        emit errorOccurred(QStringLiteral("Kein Konto verbunden."));
        return;
    }
    const auto base = _accountState->account()->url().toString();
    OcsDavClient::ocsRequest(_accountState, "GET",
        base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v3/signaling/settings"), {},
        [this](const QJsonValue &payload, int) {
            const auto settings = payload.toObject();
            _signalingServerUrl = settings.value(QStringLiteral("server")).toString();
            _ticket = settings.value(QStringLiteral("ticket")).toString();
            _userId = settings.value(QStringLiteral("userId")).toString();
            if (_signalingServerUrl.isEmpty() || _ticket.isEmpty()) {
                emit errorOccurred(QStringLiteral("Signaling-Server nicht konfiguriert."));
                return;
            }
            if (!_socket) {
                _socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
                connect(_socket, &QWebSocket::connected, this, &TalkSignalingClient::onConnectedSocket);
                connect(_socket, &QWebSocket::textMessageReceived, this, &TalkSignalingClient::onTextMessageReceived);
                connect(_socket, &QWebSocket::errorOccurred, this, &TalkSignalingClient::onSocketError);
                connect(_socket, &QWebSocket::disconnected, this, &TalkSignalingClient::scheduleReconnect);
            }
            QUrl url(_signalingServerUrl);
            if (url.scheme() == QStringLiteral("https")) {
                url.setScheme(QStringLiteral("wss"));
            } else if (url.scheme() == QStringLiteral("http")) {
                url.setScheme(QStringLiteral("ws"));
            }
            const auto path = url.path();
            if (!path.endsWith(QStringLiteral("/spreed"))) {
                url.setPath(path + QStringLiteral("/spreed"));
            }
            qCInfo(lcTalkSignaling) << "Connecting to signaling server:" << url.toString();
            _socket->open(url);
        },
        [this](int status, const QString &message) {
            qCWarning(lcTalkSignaling) << "Signaling settings failed:" << status << message;
            emit errorOccurred(message);
            scheduleReconnect();
        });
}

void TalkSignalingClient::startHello()
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return;
    QString base = acc->url().toString();
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);

    // Hello V1: the server validates the one-time ticket against the
    // Nextcloud backend. The auth url points at the Talk OCS base.
    QJsonObject params;
    params.insert(QStringLiteral("userid"), _userId);
    params.insert(QStringLiteral("ticket"), _ticket);

    QJsonObject auth;
    auth.insert(QStringLiteral("type"), QStringLiteral("client"));
    auth.insert(QStringLiteral("url"),
                QString(base + QStringLiteral("/ocs/v2.php/apps/spreed/api/v3")));
    auth.insert(QStringLiteral("params"), params);

    QJsonObject helloInner;
    helloInner.insert(QStringLiteral("version"), QStringLiteral("1.0"));
    helloInner.insert(QStringLiteral("auth"), auth);

    QJsonObject hello;
    hello.insert(QStringLiteral("type"), QStringLiteral("hello"));
    hello.insert(QStringLiteral("hello"), helloInner);
    sendJson(hello);
}

void TalkSignalingClient::sendJson(const QJsonObject &message)
{
    if (!_socket || _socket->state() != QAbstractSocket::ConnectedState) return;
    _socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void TalkSignalingClient::onConnectedSocket()
{
    qCInfo(lcTalkSignaling) << "Signaling socket connected";
    _reconnectDelayMs = 2000;
    // The server greets with a "welcome" message; the hello is sent from
    // onTextMessageReceived once it arrives.
}

void TalkSignalingClient::onSocketError()
{
    if (_socket) {
        qCWarning(lcTalkSignaling) << "Signaling socket error:" << _socket->errorString();
    }
}

void TalkSignalingClient::scheduleReconnect()
{
    if (_roomToken.isEmpty()) return;
    QTimer::singleShot(_reconnectDelayMs, this, [this]() {
        if (_roomToken.isEmpty()) return;
        _reconnectDelayMs = qMin(_reconnectDelayMs * 2, 30000);
        fetchSettings();
    });
}

void TalkSignalingClient::handleEvent(const QVariantMap &message)
{
    const auto target = message.value(QStringLiteral("target")).toString();
    const auto type = message.value(QStringLiteral("type")).toString();

    if (target == QStringLiteral("participants") && type == QStringLiteral("update")) {
        QVector<TalkParticipant> participants;
        const auto users = message.value(QStringLiteral("users")).toList();
        for (const auto &u : users) {
            const auto obj = u.toMap();
            TalkParticipant p;
            p.actorId = obj.value(QStringLiteral("userid")).toString();
            p.inCall = obj.value(QStringLiteral("incall")).toInt();
            const auto display = obj.value(QStringLiteral("displayName"));
            p.displayName = display.isValid() ? display.toString() : p.actorId;
            participants.append(p);
        }
        if (!_roomToken.isEmpty()) {
            emit participantsChanged(_roomToken, participants);
        }
    }
}

void TalkSignalingClient::onTextMessageReceived(const QString &message)
{
    const auto doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    const auto root = doc.object().toVariantMap();
    const auto type = root.value(QStringLiteral("type")).toString();
    qCInfo(lcTalkSignaling) << "Signaling message:" << type
                            << (root.contains(QStringLiteral("error"))
                                    ? root.value(QStringLiteral("error")).toString() : QString());

    if (type == QStringLiteral("welcome")) {
        // Server announced its protocol version; authenticate now.
        startHello();
        return;
    }
    if (type == QStringLiteral("hello")) {
        const auto hello = root.value(QStringLiteral("hello")).toMap();
        qCInfo(lcTalkSignaling) << "Signaling hello ok, session:" << hello.value(QStringLiteral("sessionid")).toString().left(8);
        _reconnectDelayMs = 2000;
        emit connected();
        if (!_roomToken.isEmpty()) {
            QJsonObject room;
            room.insert(QStringLiteral("roomid"), _roomToken);
            if (!_roomSessionId.isEmpty()) {
                // The Talk participant session links the signaling room to
                // the REST call join — required by the backend room check.
                room.insert(QStringLiteral("sessionid"), _roomSessionId);
            }
            QJsonObject join;
            join.insert(QStringLiteral("type"), QStringLiteral("room"));
            join.insert(QStringLiteral("room"), room);
            sendJson(join);
        }
        return;
    }
    if (type == QStringLiteral("room")) {
        const auto room = root.value(QStringLiteral("room")).toMap();
        if (room.contains(QStringLiteral("roomid"))) {
            qCInfo(lcTalkSignaling) << "Signaling room joined:" << room.value(QStringLiteral("roomid")).toString();
            _joinedRoom = true;
            emit roomJoined(_roomToken);
        }
        return;
    }
    if (type == QStringLiteral("event")) {
        handleEvent(root.value(QStringLiteral("event")).toMap());
        return;
    }
    if (type == QStringLiteral("ping")) {
        // HPB requires the client to answer server pings.
        QJsonObject pong;
        pong.insert(QStringLiteral("type"), QStringLiteral("ping"));
        sendJson(pong);
        return;
    }
}

} // namespace OCC
