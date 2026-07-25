/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailMessageModel.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMailMessageModel, "souvera.mail.messagemodel")

namespace OCC {

MailMessageModel::MailMessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MailMessageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return _messages.size();
}

QVariant MailMessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _messages.size()) return {};
    const auto &msg = _messages.at(index.row());
    if (role == Qt::DisplayRole) {
        return QStringLiteral("%1 — %2").arg(msg.sender, msg.subject);
    }
    if (role == Qt::UserRole) {
        return msg.bodyHtml;
    }
    return {};
}

void MailMessageModel::loadFolder(const QString &folderName)
{
    Q_UNUSED(folderName)
    beginResetModel();
    _messages.clear();
    MailMessage placeholder;
    placeholder.subject = QStringLiteral("Willkommen bei Souvera Mail");
    placeholder.sender = QStringLiteral("support@souvera.work");
    placeholder.date = QStringLiteral("2025-07-25");
    placeholder.bodyHtml = QStringLiteral("<h2>Willkommen</h2><p>Ihr E-Mail-Postfach ist bereit.</p>");
    _messages.append(placeholder);
    endResetModel();
    qCInfo(lcMailMessageModel) << "Loaded folder:" << folderName;
}

} // namespace OCC
