/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef MAILTYPES_H
#define MAILTYPES_H

#include <QString>
#include <QDateTime>

namespace OCC {

struct MailFolderData {
    QString path;
    QString name;
    int unreadCount = 0;
    int totalCount = 0;
};

struct MailMessageData {
    QString uid;
    QString subject;
    QString from;
    QString fromAddress;
    QDateTime date;
    bool isRead = false;
    bool hasAttachments = false;
    int sizeBytes = 0;
};

} // namespace OCC

#endif // MAILTYPES_H
