/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailMessageModel.h"

#include <QLoggingCategory>
#include <QLocale>

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

    switch (role) {
    case FromRole:
        return msg.from;
    case SubjectRole:
        return msg.subject;
    case DateTimeRole:
        return msg.dateTime;
    case BodyHtmlRole:
        return msg.bodyHtml;
    case BodyPlainRole:
        return msg.bodyPlain;
    case UnreadRole:
        return msg.unread;
    case HasAttachmentsRole:
        return msg.hasAttachments;
    case UidRole:
        return msg.uid;
    case DateDisplayRole: {
        auto now = QDateTime::currentDateTime();
        if (msg.dateTime.date() == now.date()) {
            return msg.dateTime.toString(QStringLiteral("HH:mm"));
        }
        if (msg.dateTime.date().year() == now.date().year()) {
            return msg.dateTime.toString(QStringLiteral("dd.MM"));
        }
        return msg.dateTime.toString(QStringLiteral("dd.MM.yy"));
    }
    case SummaryRole: {
        auto preview = msg.bodyPlain.left(100).simplified();
        if (preview.isEmpty()) {
            preview = QStringLiteral("...");
        }
        if (msg.unread) {
            return QStringLiteral("◆ %1\n%2\n%3").arg(msg.from, msg.subject, preview);
        }
        return QStringLiteral("%1\n%2\n%3").arg(msg.from, msg.subject, preview);
    }
    case Qt::ToolTipRole:
        return QStringLiteral("Von: %1\nBetreff: %2\nDatum: %3")
            .arg(msg.from, msg.subject, QLocale().toString(msg.dateTime, QLocale::LongFormat));
    default:
        return {};
    }
}

void MailMessageModel::loadMessages(const QList<MailMessage> &messages)
{
    beginResetModel();

    auto sorted = messages;
    std::sort(sorted.begin(), sorted.end(), [](const MailMessage &a, const MailMessage &b) {
        return a.dateTime > b.dateTime;
    });

    _messages = sorted;
    endResetModel();

    qCInfo(lcMailMessageModel) << "Messages loaded, count:" << _messages.size();
}

void MailMessageModel::clear()
{
    beginResetModel();
    _messages.clear();
    endResetModel();
}

const MailMessage *MailMessageModel::messageAt(int row) const
{
    if (row < 0 || row >= _messages.size()) return nullptr;
    return &_messages.at(row);
}

} // namespace OCC
