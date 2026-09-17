/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKSIGNALINGCLIENT_H
#define TALKSIGNALINGCLIENT_H

#include <QJsonObject>
#include <QObject>
#include <QVector>

class QWebSocket;

namespace OCC {

class AccountState;

struct TalkParticipant; // defined in TalkOcsApi.h

/**
 * @brief Native client for the Talk high-performance backend (standalone
 *        signaling) over WebSocket.
 *
 * Flow: fetches the signaling settings from the Nextcloud server (server
 * URL, one-time ticket), connects the WebSocket, performs the ticket-based
 * hello handshake, joins a room and forwards live participant updates —
 * the same protocol the mobile apps use. Keeps the connection alive with
 * periodic pings and reconnects on errors.
 */
class TalkSignalingClient : public QObject
{
    Q_OBJECT
public:
    explicit TalkSignalingClient(QObject *parent = nullptr);
    ~TalkSignalingClient() override;

    void setAccountState(AccountState *state);

    /** Fetches settings, connects and joins the given room. */
    void joinRoom(const QString &roomToken, const QString &roomSessionId);
    void leaveRoom();

    /**
     * Sends a media signaling message (offer, answer, candidate, etc.)
     * broadcast to the current call.
     */
    void sendMediaMessage(const QJsonObject &data);

signals:
    void connected();
    void roomJoined(const QString &roomToken);
    void participantsChanged(const QString &roomToken,
                             const QVector<TalkParticipant> &participants);
    /** A media signaling message (offer/answer/candidate) was received. */
    void mediaMessageReceived(const QJsonObject &data);
    void errorOccurred(const QString &message);

private slots:
    void onTextMessageReceived(const QString &message);
    void onConnectedSocket();
    void onSocketError();

private:
    void fetchSettings();
    void startHello();
    void sendJson(const QJsonObject &message);
    void handleEvent(const QVariantMap &message);
    void scheduleReconnect();

    AccountState *_accountState = nullptr;
    QWebSocket *_socket = nullptr;
    QString _roomToken;
    QString _roomSessionId;
    QString _signalingServerUrl;
    QString _helloVersion;      //!< "1.0" (userid+ticket) or "2.0" (JWT token)
    QJsonObject _helloAuthParams;
    bool _joinedRoom = false;
    int _reconnectDelayMs = 2000;
};

} // namespace OCC

#endif // TALKSIGNALINGCLIENT_H
