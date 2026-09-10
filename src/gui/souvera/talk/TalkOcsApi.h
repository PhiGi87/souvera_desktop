/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKOCSAPI_H
#define TALKOCSAPI_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

namespace OCC {

class AccountState;

class TalkOcsApi : public QObject
{
    Q_OBJECT
public:
    explicit TalkOcsApi(QObject *parent = nullptr);

    void setAccountState(AccountState *state);

    void fetchConversations();
    void fetchMessages(const QString &token, qint64 lastKnownId = 0);
    void sendMessage(const QString &token, const QString &text);

signals:
    void conversationsReceived(const QJsonArray &conversations);
    void messagesReceived(const QJsonArray &messages, const QString &token);
    void messageSent(const QString &token);
    void apiError(const QString &message);

private:
    void conversationsRequest(const QString &apiBase, bool isV1Retry);
    void messagesRequest(const QString &apiBase, const QString &token, qint64 lastKnownId);
    void sendRequest(const QString &apiBase, const QString &token, const QString &text);

    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // TALKOCSAPI_H
