/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef JMAPCLIENT_H
#define JMAPCLIENT_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QList>
#include <QPair>
#include <QDateTime>
#include <functional>

namespace OCC {

class AccountState;

struct JmapMailbox { QString id; QString name; QString role; int totalEmails = 0; int unreadEmails = 0; QString parentId; };
struct JmapEmail { QString id; QString subject; QString fromAddress; QString fromName; QString toAddresses; QDateTime receivedAt; bool isRead = false; bool isFlagged = false; bool hasAttachments = false; QString preview; QString threadId; qint64 size = 0; };
struct JmapEmailBody { QString htmlBody; QString plainBody; QList<QPair<QString, QString>> attachments; };

/**
 * @brief Direct Stalwart JMAP client (like Android JmapClient.kt).
 *
 * Session discovery: GET /jmap/session
 * Auth: Basic (app password) with Bearer fallback.
 */
class JmapClient : public QObject
{
    Q_OBJECT
public:
    explicit JmapClient(AccountState *accountState, QObject *parent = nullptr);

    void setCredentials(const QString &user, const QString &password);
    void setBearerToken(const QString &token);

    void resolveSession();

    void fetchMailboxes();
    void queryEmails(const QString &mailboxId, int limit = 50, int offset = 0,
                     const QString &searchQuery = QString(), const QString &filterType = QString());
    void fetchEmailBody(const QString &emailId);
    void markRead(const QString &emailId, bool read);
    void moveEmail(const QString &emailId, const QString &targetMailboxId);
    void deleteEmail(const QString &emailId);
    void sendEmail(const QString &to, const QString &cc, const QString &bcc,
                   const QString &subject, const QString &bodyHtml, const QString &inReplyTo);

signals:
    void sessionResolved(const QString &accountId, const QString &apiUrl);
    void sessionError(const QString &error);
    void needsBearerToken();

    void mailboxesFetched(const QList<JmapMailbox> &mailboxes);
    void emailsFetched(const QList<JmapEmail> &emails, int total);
    void emailBodyFetched(const JmapEmailBody &body);
    void emailSent(bool success, const QString &error);
    void operationCompleted(bool success);
    void networkError(const QString &error);

private:
    void jmapCall(const QString &method, const QJsonObject &args,
                  std::function<void(const QJsonObject &)> callback);
    void jmapBatch(const QList<QPair<QString, QJsonObject>> &calls,
                   std::function<void(const QJsonArray &)> callback);

    QString baseUrl() const;
    QString authHeader() const;

    QString _apiUrl;
    QString _accountId;
    QString _user;
    QString _password;
    QString _bearerToken;
    QNetworkAccessManager *_nam;
    AccountState *_accountState;
};

} // namespace OCC
#endif // JMAPCLIENT_H
