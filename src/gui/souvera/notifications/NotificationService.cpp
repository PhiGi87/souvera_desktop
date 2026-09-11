/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "NotificationService.h"
#include "mail/JmapClient.h"
#include "mail/MailLoginFlow.h"
#include "net/OcsDavClient.h"
#include "systray.h"
#include "account.h"
#include "accountstate.h"

#include <QLoggingCategory>
#include <QSettings>
#include <QSystemTrayIcon>

namespace OCC {

Q_LOGGING_CATEGORY(lcNotificationService, "souvera.notifications", QtInfoMsg)

namespace {
constexpr int NcPollIntervalMs = 45 * 1000;
constexpr int MailPollIntervalMs = 60 * 1000;
constexpr int MaxSeenIds = 200;

QString settingsKey(const char *name)
{
    return QStringLiteral("souvera/notifications/%1").arg(QString::fromLatin1(name));
}
}

NotificationService::NotificationService(QObject *parent)
    : QObject(parent)
{
    _ncTimer.setInterval(NcPollIntervalMs);
    connect(&_ncTimer, &QTimer::timeout, this, &NotificationService::pollNextcloud);

    _mailTimer.setInterval(MailPollIntervalMs);
    connect(&_mailTimer, &QTimer::timeout, this, &NotificationService::pollMail);
}

void NotificationService::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) return;
    _accountState = accountState;

    _ncUnavailable = false;
    _mailBaselineDone = false;
    _inboxId.clear();
    _seenMailIds.clear();
    stopMailPolling();

    if (!accountState) {
        _ncTimer.stop();
        return;
    }

    if (notificationsEnabled()) {
        pollNextcloud();
        _ncTimer.start();
    }
    if (mailNotificationsEnabled()) {
        startMailPolling();
    }
}

bool NotificationService::notificationsEnabled()
{
    QSettings settings;
    return settings.value(settingsKey("enabled"), true).toBool();
}

void NotificationService::setNotificationsEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(settingsKey("enabled"), enabled);
}

bool NotificationService::mailNotificationsEnabled()
{
    QSettings settings;
    return settings.value(settingsKey("mail"), true).toBool();
}

void NotificationService::setMailNotificationsEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(settingsKey("mail"), enabled);
}

void NotificationService::pollNextcloud()
{
    if (!_accountState) return;

    OcsDavClient::ocsRequest(_accountState, "GET",
        QStringLiteral("/ocs/v2.php/apps/notifications/api/v2/notifications"), {},
        [this](const QJsonValue &payload, int) {
            _ncUnavailable = false;
            const auto data = payload.toArray();
            QStringList freshIds;
            for (const auto &item : data) {
                const auto obj = item.toObject();
                const auto id = QString::number(obj[QStringLiteral("notification_id")].toVariant().toLongLong());
                freshIds.append(id);
                if (_seenNcIds.contains(id)) continue;
                _seenNcIds.append(id);
                const auto subject = obj[QStringLiteral("subject")].toString();
                const auto app = obj[QStringLiteral("app")].toString();
                if (subject.isEmpty()) continue;
                showNotification(app.isEmpty() ? QStringLiteral("Souvera") : app, subject,
                                 QSystemTrayIcon::Information);
            }
            _seenNcIds = freshIds + _seenNcIds;
            trimSeenIds();
        },
        [this](int status, const QString &message) {
            // 404 = notifications app not installed: stop polling silently.
            if (status == 404 && !_ncUnavailable) {
                _ncUnavailable = true;
                _ncTimer.stop();
                qCInfo(lcNotificationService) << "Notifications app not available, polling disabled.";
                return;
            }
            if (status != 404) {
                qCWarning(lcNotificationService) << "pollNextcloud failed:" << status << message;
            }
        });
}

void NotificationService::startMailPolling()
{
    if (!_accountState || !_accountState->account()) return;

    const auto user = _accountState->account()->davUser();
    const auto password = MailLoginFlow::cachedPassword(_accountState);
    if (user.isEmpty() || password.isEmpty()) {
        // No cached mail credential yet: the mail panel mints one when opened.
        qCInfo(lcNotificationService) << "No cached mail password, mail polling deferred.";
        return;
    }

    if (_jmapClient) {
        _jmapClient->deleteLater();
        _jmapClient = nullptr;
    }
    _jmapClient = new JmapClient(_accountState, this);
    _jmapClient->setCredentials(user, password);

    connect(_jmapClient, &JmapClient::sessionResolved, this, [this](const QString &, const QString &) {
        _jmapClient->fetchMailboxes();
    });
    connect(_jmapClient, &JmapClient::mailboxesFetched, this, [this](const QList<JmapMailbox> &mailboxes) {
        for (const auto &mailbox : mailboxes) {
            if (mailbox.role.compare(QStringLiteral("inbox"), Qt::CaseInsensitive) == 0) {
                _inboxId = mailbox.id;
                break;
            }
        }
        if (_inboxId.isEmpty()) {
            qCWarning(lcNotificationService) << "No inbox mailbox found, mail polling disabled.";
            return;
        }
        _mailBaselineDone = false;
        pollMail();
        _mailTimer.start();
    });
    connect(_jmapClient, &JmapClient::sessionError, this, [this](const QString &error) {
        qCWarning(lcNotificationService) << "JMAP session error, mail polling stopped:" << error;
        stopMailPolling();
    });
    connect(_jmapClient, &JmapClient::emailsFetched, this,
            [this](const QList<JmapEmail> &emails, int) {
        for (const auto &email : emails) {
            if (_seenMailIds.contains(email.id)) continue;
            _seenMailIds.insert(email.id);
            // First poll after startup is a baseline only, no notification spam.
            if (_mailBaselineDone && !email.isRead) {
                const auto from = email.fromName.isEmpty() ? email.fromAddress : email.fromName;
                showNotification(QStringLiteral("Neue E-Mail"),
                                 QStringLiteral("%1: %2").arg(from, email.subject),
                                 QSystemTrayIcon::Information);
            }
        }
        _mailBaselineDone = true;
    });

    _jmapClient->resolveSession();
}

void NotificationService::stopMailPolling()
{
    _mailTimer.stop();
    if (_jmapClient) {
        _jmapClient->deleteLater();
        _jmapClient = nullptr;
    }
}

void NotificationService::pollMail()
{
    if (!_jmapClient || _inboxId.isEmpty()) return;
    _jmapClient->queryEmails(_inboxId, 5, 0);
}

void NotificationService::showNotification(const QString &title, const QString &body,
                                           QSystemTrayIcon::MessageIcon icon)
{
    if (!notificationsEnabled()) return;
    if (auto *tray = Systray::instance()) {
        tray->showMessage(title, body, icon);
    }
}

void NotificationService::trimSeenIds()
{
    while (_seenNcIds.size() > MaxSeenIds) {
        _seenNcIds.removeLast();
    }
}

} // namespace OCC
