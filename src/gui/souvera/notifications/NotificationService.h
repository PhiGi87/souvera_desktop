/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NOTIFICATIONSERVICE_H
#define NOTIFICATIONSERVICE_H

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTimer>

namespace OCC {

class AccountState;
class JmapClient;

/**
 * @brief Desktop notification service.
 *
 * Polls the Nextcloud notifications app endpoint and the JMAP inbox and
 * raises system tray notifications for anything new. This mirrors the
 * official desktop client's fallback behaviour (server push registration
 * via a push proxy is not implemented).
 */
class NotificationService : public QObject
{
    Q_OBJECT
public:
    explicit NotificationService(QObject *parent = nullptr);

    void setAccountState(AccountState *accountState);

    static bool notificationsEnabled();
    static void setNotificationsEnabled(bool enabled);
    static bool mailNotificationsEnabled();
    static void setMailNotificationsEnabled(bool enabled);

private slots:
    void pollNextcloud();
    void pollMail();

private:
    void startMailPolling();
    void stopMailPolling();
    void showNotification(const QString &title, const QString &body,
                          QSystemTrayIcon::MessageIcon icon);
    void trimSeenIds();

    AccountState *_accountState = nullptr;
    QTimer _ncTimer;
    QTimer _mailTimer;
    bool _ncUnavailable = false;

    JmapClient *_jmapClient = nullptr;
    QString _inboxId;
    QSet<QString> _seenMailIds;
    bool _mailBaselineDone = false;

    QStringList _seenNcIds;
};

} // namespace OCC

#endif // NOTIFICATIONSERVICE_H
