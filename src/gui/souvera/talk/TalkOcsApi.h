/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKOCSAPI_H
#define TALKOCSAPI_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

namespace OCC {

class AccountState;

/** One member of a Talk room as returned by the participants endpoint. */
struct TalkParticipant
{
    QString actorId;
    QString displayName;
    int inCall = 0; //!< non-zero while the participant is inside a call
};

class TalkOcsApi : public QObject
{
    Q_OBJECT
public:
    explicit TalkOcsApi(QObject *parent = nullptr);

    void setAccountState(AccountState *state);

    void fetchConversations();
    void fetchSelfUser();
    void fetchMessages(const QString &token, qint64 lastKnownId = 0);
    void sendMessage(const QString &token, const QString &text);

    // Native call layer (the mobile-app flow: join/leave over the REST API).
    void joinCall(const QString &token, int flags = 0);
    void leaveCall(const QString &token);
    void fetchParticipants(const QString &token);

    /**
     * @brief Starts (or joins) the running call — the counterpart of the
     *        web app's POST call/{token}. Must be called after the
     *        signaling room join; this is what sets the server-side
     *        in-call state.
     */
    void startCall(const QString &token, int flags = 1);

signals:
    void conversationsReceived(const QJsonArray &conversations);
    void messagesReceived(const QJsonArray &messages, const QString &token);
    void messageSent(const QString &token);
    void selfUserReceived(const QString &userId);
    void apiError(const QString &message);

    void callJoined(const QString &token, const QString &sessionId);
    void callStarted(const QString &token);
    void callLeft(const QString &token);
    void participantsReceived(const QString &token, const QVector<TalkParticipant> &participants);

private:
    void conversationsRequest(const QString &apiBase, bool isV1Retry);
    void messagesRequest(const QString &apiBase, const QString &token, qint64 lastKnownId);
    void sendRequest(const QString &apiBase, const QString &token, const QString &text);
    void participantsRequest(const QString &apiBase, const QString &token);

    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // TALKOCSAPI_H
