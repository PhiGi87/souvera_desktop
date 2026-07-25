/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKOCSAPI_H
#define TALKOCSAPI_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QNetworkAccessManager;

namespace OCC {

class AccountState;

class TalkOcsApi : public QObject
{
    Q_OBJECT
public:
    explicit TalkOcsApi(QObject *parent = nullptr);

    void setAccountState(AccountState *state) { _accountState = state; }

    void fetchConversations();
    void fetchMessages(const QString &token);
    void sendMessage(const QString &token, const QString &text);

signals:
    void conversationsReceived(const QJsonArray &conversations);
    void messagesReceived(const QJsonArray &messages);
    void messageSent();

private:
    [[nodiscard]] QString ocsUrl(const QString &path) const;

    QNetworkAccessManager *_nam = nullptr;
    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // TALKOCSAPI_H
