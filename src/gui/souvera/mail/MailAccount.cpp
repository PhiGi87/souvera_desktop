/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailAccount.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMailAccount, "souvera.mail.account")

namespace OCC {

MailAccount::MailAccount(QObject *parent)
    : QObject(parent)
{
}

QString MailAccount::deriveImapHost(const QString &souveraDomain)
{
    return QStringLiteral("imap.%1").arg(souveraDomain);
}

QString MailAccount::deriveSmtpHost(const QString &souveraDomain)
{
    return QStringLiteral("smtp.%1").arg(souveraDomain);
}

} // namespace OCC
