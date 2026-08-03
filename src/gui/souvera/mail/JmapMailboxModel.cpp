/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "JmapMailboxModel.h"

namespace OCC {

JmapMailboxModel::JmapMailboxModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int JmapMailboxModel::rowCount(const QModelIndex &) const
{
    return _mailboxes.size();
}

QVariant JmapMailboxModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _mailboxes.size()) return {};
    const auto &m = _mailboxes.at(index.row());
    switch (role) {
    case IdRole: return m.id;
    case NameRole: return m.name;
    case RoleRole: return m.role;
    case UnreadRole: return m.unreadEmails;
    case TotalRole: return m.totalEmails;
    case ParentIdRole: return m.parentId;
    case Qt::DisplayRole: return m.name;
    default: return {};
    }
}

QHash<int, QByteArray> JmapMailboxModel::roleNames() const
{
    return {
        {IdRole, "mailboxId"},
        {NameRole, "name"},
        {RoleRole, "role"},
        {UnreadRole, "unread"},
        {TotalRole, "total"},
        {ParentIdRole, "parentId"},
    };
}

void JmapMailboxModel::setMailboxes(const QList<JmapMailbox> &mailboxes)
{
    beginResetModel();
    _mailboxes = mailboxes;
    endResetModel();
}

QString JmapMailboxModel::mailboxIdForRow(int row) const
{
    if (row < 0 || row >= _mailboxes.size()) return {};
    return _mailboxes.at(row).id;
}

QList<QString> JmapMailboxModel::roleIds() const
{
    QList<QString> ids;
    for (const auto &m : _mailboxes) {
        if (m.role == QLatin1String("inbox") || m.role == QLatin1String("sent")
            || m.role == QLatin1String("drafts") || m.role == QLatin1String("archive")
            || m.role == QLatin1String("junk") || m.role == QLatin1String("trash"))
            ids.append(m.id);
    }
    return ids;
}

} // namespace OCC
