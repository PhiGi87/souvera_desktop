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
#include <functional>

namespace OCC {

class AccountState;

struct JmapMailbox {
    QString id;
    QString name;
    QString role;
    int totalEmails = 0;
    int unreadEmails = 0;
    QString parentId;
};

struct JmapEmail {
    QString id;
    QString subject;
    QString fromAddress;
    QString fromName;
    QString toAddresses;
    QDateTime receivedAt;
    bool isRead = false;
    bool isFlagged = false;
    bool hasAttachments = false;
    QString preview;
    QString threadId;
    qint64 size = 0;
};

struct JmapEmailBody {
    QString htmlBody;
    QString plainBody;
    QList<QPair<QString, QString>> attachments; // blobId, name
};

/**
 * @brief REST client for souvera_mail v2 PHP proxy.
 *
 * Uses HTTP Basic Auth with the Nextcloud app password (from Flow2Auth).
 * All JMAP calls are proxied through /apps/souvera_mail/api/v2/* endpoints.
 */
class JmapClient : public QObject
{
    Q_OBJECT
public:
    explicit JmapClient(AccountState *accountState, QObject *parent = nullptr);

    void setCredentials(const QString &user, const QString &password);
    QString userName() const { return _user; }

    void fetchMailboxes();
    void queryEmails(const QString &mailboxId, int limit = 50, int offset = 0,
                     const QString &searchQuery = QString(), const QString &filterType = QString());
    void fetchEmailBody(const QString &emailId);
    void markRead(const QString &emailId, bool read);
    void moveEmail(const QString &emailId, const QString &targetMailboxId);
    void deleteEmail(const QString &emailId);
    void sendEmail(const QString &to, const QString &cc, const QString &bcc,
                   const QString &subject, const QString &bodyHtml,
                   const QString &inReplyTo);

signals:
    void mailboxesFetched(const QList<JmapMailbox> &mailboxes);
    void emailsFetched(const QList<JmapEmail> &emails, int total);
    void emailBodyFetched(const JmapEmailBody &body);
    void emailSent(bool success, const QString &error);
    void operationCompleted(bool success);
    void networkError(const QString &error);

private:
    QNetworkRequest makeRequest(const QString &path) const;
    void apiGet(const QString &path, std::function<void(const QJsonDocument &)> callback);
    void apiPost(const QString &path, const QJsonObject &body,
                 std::function<void(const QJsonDocument &)> callback);

    AccountState *_accountState;
    QNetworkAccessManager *_nam;
    QString _user;
    QString _password;
};

} // namespace OCC

#endif // JMAPCLIENT_H
