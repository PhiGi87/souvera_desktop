/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "JmapMailboxModel.h"

#include <algorithm>

namespace OCC {

JmapMailboxModel::JmapMailboxModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

int JmapMailboxModel::sortWeight(const JmapMailbox &mailbox)
{
    const auto role = mailbox.role.toLower();
    if (role == QLatin1String("inbox")) return 0;
    if (role == QLatin1String("drafts")) return 1;
    if (role == QLatin1String("sent")) return 2;
    if (role == QLatin1String("junk")) return 3;
    if (role == QLatin1String("trash")) return 4;
    if (role == QLatin1String("archive")) return 5;
    return 9;
}

void JmapMailboxModel::rebuild()
{
    beginResetModel();

    qDeleteAll(_roots);
    _roots.clear();
    _byId.clear();

    // Create nodes.
    for (const auto &mailbox : std::as_const(_mailboxes)) {
        auto *node = new Node;
        node->mailbox = mailbox;
        _byId.insert(mailbox.id, node);
    }

    // Wire hierarchy.
    for (auto it = _byId.constBegin(); it != _byId.constEnd(); ++it) {
        auto *node = it.value();
        const auto parentId = node->mailbox.parentId;
        if (!parentId.isEmpty() && _byId.contains(parentId) && _byId.value(parentId) != node) {
            node->parent = _byId.value(parentId);
        }
    }

    // Collect roots and children, sorted.
    auto sorter = [](Node *a, Node *b) {
        const auto wa = sortWeight(a->mailbox);
        const auto wb = sortWeight(b->mailbox);
        if (wa != wb) return wa < wb;
        return a->mailbox.name.compare(b->mailbox.name, Qt::CaseInsensitive) < 0;
    };

    for (auto it = _byId.constBegin(); it != _byId.constEnd(); ++it) {
        auto *node = it.value();
        if (node->parent) {
            node->parent->children.append(node);
        } else {
            _roots.append(node);
        }
    }
    std::sort(_roots.begin(), _roots.end(), sorter);
    for (auto it = _byId.constBegin(); it != _byId.constEnd(); ++it) {
        auto *node = it.value();
        std::sort(node->children.begin(), node->children.end(), sorter);
        for (auto row = 0; row < node->children.size(); ++row) {
            node->children[row]->row = row;
        }
    }
    for (auto row = 0; row < _roots.size(); ++row) {
        _roots[row]->row = row;
    }

    endResetModel();
}

QModelIndex JmapMailboxModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0) return {};
    Node *parentNode = parent.isValid() ? static_cast<Node *>(parent.internalPointer()) : nullptr;
    const auto &siblings = parentNode ? parentNode->children : _roots;
    if (row >= siblings.size()) return {};
    return createIndex(row, column, siblings.at(row));
}

QModelIndex JmapMailboxModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};
    auto *node = static_cast<Node *>(child.internalPointer());
    if (!node || !node->parent) return {};
    return createIndex(node->parent->row, 0, node->parent);
}

int JmapMailboxModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;
    Node *node = parent.isValid() ? static_cast<Node *>(parent.internalPointer()) : nullptr;
    const auto &siblings = node ? node->children : _roots;
    return siblings.size();
}

int JmapMailboxModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant JmapMailboxModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    auto *node = static_cast<Node *>(index.internalPointer());
    if (!node) return {};

    const auto &mailbox = node->mailbox;
    switch (role) {
    case MailboxIdRole: return mailbox.id;
    case NameRole: return mailbox.name;
    case RoleNameRole: return mailbox.role;
    case UnreadCountRole: return mailbox.unreadEmails;
    case TotalCountRole: return mailbox.totalEmails;
    case ParentIdRole: return mailbox.parentId;
    case Qt::DisplayRole: {
        QString name;
        if (mailbox.role.compare(QStringLiteral("inbox"), Qt::CaseInsensitive) == 0) {
            name = QStringLiteral("Posteingang");
        } else if (mailbox.role.compare(QStringLiteral("drafts"), Qt::CaseInsensitive) == 0) {
            name = QStringLiteral("Entw\u00FCrfe");
        } else if (mailbox.role.compare(QStringLiteral("sent"), Qt::CaseInsensitive) == 0) {
            name = QStringLiteral("Gesendet");
        } else if (mailbox.role.compare(QStringLiteral("trash"), Qt::CaseInsensitive) == 0) {
            name = QStringLiteral("Papierkorb");
        } else if (mailbox.role.compare(QStringLiteral("junk"), Qt::CaseInsensitive) == 0) {
            name = QStringLiteral("Spam");
        } else if (mailbox.role.compare(QStringLiteral("archive"), Qt::CaseInsensitive) == 0) {
            name = QStringLiteral("Archiv");
        } else {
            name = mailbox.name;
        }
        // Thunderbird-style: append unread count in parentheses
        if (mailbox.unreadEmails > 0) {
            name += QStringLiteral(" (%1)").arg(mailbox.unreadEmails);
        }
        return name;
    }
    case Qt::DecorationRole: {
        // Folder icons per role
        const auto role = mailbox.role.toLower();
        if (role == QLatin1String("inbox")) return QStringLiteral("\U0001F4E5");
        if (role == QLatin1String("sent")) return QStringLiteral("\U0001F4E4");
        if (role == QLatin1String("drafts")) return QStringLiteral("\U0001F4DD");
        if (role == QLatin1String("trash")) return QStringLiteral("\U0001F5D1");
        if (role == QLatin1String("junk")) return QStringLiteral("\U0001F6A8");
        if (role == QLatin1String("archive")) return QStringLiteral("\U0001F4E6");
        return QStringLiteral("\U0001F4C1"); // generic folder
    }
    case Qt::ToolTipRole: return mailbox.name;
    default: return {};
    }
}

QHash<int, QByteArray> JmapMailboxModel::roleNames() const
{
    return {
        {MailboxIdRole, "mailboxId"},
        {NameRole, "name"},
        {RoleNameRole, "role"},
        {UnreadCountRole, "unreadCount"},
        {TotalCountRole, "totalCount"},
        {ParentIdRole, "parentId"},
    };
}

void JmapMailboxModel::setMailboxes(const QList<JmapMailbox> &mailboxes)
{
    _mailboxes = mailboxes;
    rebuild();
}

QModelIndex JmapMailboxModel::indexForMailboxId(const QString &mailboxId) const
{
    auto *node = _byId.value(mailboxId);
    if (!node) return {};
    return createIndex(node->row, 0, node);
}

QString JmapMailboxModel::mailboxIdForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) return QString();
    auto *node = static_cast<Node *>(index.internalPointer());
    return node ? node->mailbox.id : QString();
}

QModelIndex JmapMailboxModel::inboxIndex() const
{
    for (auto it = _byId.constBegin(); it != _byId.constEnd(); ++it) {
        if (it.value()->mailbox.role.compare(QStringLiteral("inbox"), Qt::CaseInsensitive) == 0) {
            return indexForMailboxId(it.key());
        }
    }
    return _roots.isEmpty() ? QModelIndex() : indexForMailboxId(_roots.first()->mailbox.id);
}

} // namespace OCC
