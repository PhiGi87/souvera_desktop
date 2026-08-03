/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "JmapEmailListModel.h"

namespace OCC {

JmapEmailListModel::JmapEmailListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int JmapEmailListModel::rowCount(const QModelIndex &) const
{
    return _emails.size();
}

QVariant JmapEmailListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _emails.size()) return {};
    const auto &e = _emails.at(index.row());
    switch (role) {
    case IdRole: return e.id;
    case SubjectRole: return e.subject;
    case FromAddressRole: return e.fromAddress;
    case FromNameRole: return e.fromName.isEmpty() ? e.fromAddress : e.fromName;
    case ReceivedAtRole: return e.receivedAt;
    case IsReadRole: return e.isRead;
    case IsFlaggedRole: return e.isFlagged;
    case HasAttachmentRole: return e.hasAttachments;
    case PreviewRole: return e.preview;
    case Qt::DisplayRole: return e.subject.isEmpty() ? QLatin1String("(no subject)") : e.subject;
    default: return {};
    }
}

QHash<int, QByteArray> JmapEmailListModel::roleNames() const
{
    return {
        {IdRole, "emailId"},
        {SubjectRole, "subject"},
        {FromAddressRole, "fromAddress"},
        {FromNameRole, "fromName"},
        {ReceivedAtRole, "receivedAt"},
        {IsReadRole, "isRead"},
        {IsFlaggedRole, "isFlagged"},
        {HasAttachmentRole, "hasAttachment"},
        {PreviewRole, "preview"},
    };
}

void JmapEmailListModel::setEmails(const QList<JmapEmail> &emails)
{
    beginResetModel();
    _emails = emails;
    endResetModel();
}

QString JmapEmailListModel::emailIdForRow(int row) const
{
    if (row < 0 || row >= _emails.size()) return {};
    return _emails.at(row).id;
}

void JmapEmailListModel::markRead(int row)
{
    if (row < 0 || row >= _emails.size()) return;
    _emails[row].isRead = true;
    emit dataChanged(index(row), index(row), {IsReadRole});
}

} // namespace OCC
