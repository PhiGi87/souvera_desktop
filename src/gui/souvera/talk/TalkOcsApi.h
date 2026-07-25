/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKOCSAPI_H
#define TALKOCSAPI_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>

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

private:
    [[nodiscard]] QString ocsUrl(const QString &path) const;

    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // TALKOCSAPI_H
