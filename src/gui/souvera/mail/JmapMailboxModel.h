/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef JMAPMAILBOXMODEL_H
#define JMAPMAILBOXMODEL_H

#include "JmapClient.h"

#include <QAbstractItemModel>
#include <QHash>

namespace OCC {

/**
 * @brief Hierarchical mailbox model sorted Thunderbird-style:
 *        Inbox first, then the special folders, then alphabetically.
 */
class JmapMailboxModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Roles {
        MailboxIdRole = Qt::UserRole + 1,
        NameRole,
        RoleNameRole,
        UnreadCountRole,
        TotalCountRole,
        ParentIdRole,
    };

    explicit JmapMailboxModel(QObject *parent = nullptr);

    [[nodiscard]] QModelIndex index(int row, int column,
                                   const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setMailboxes(const QList<JmapMailbox> &mailboxes);

    [[nodiscard]] QModelIndex indexForMailboxId(const QString &mailboxId) const;
    [[nodiscard]] QString mailboxIdForIndex(const QModelIndex &index) const;
    [[nodiscard]] QModelIndex inboxIndex() const;

private:
    struct Node {
        JmapMailbox mailbox;
        Node *parent = nullptr;
        QList<Node *> children;
        int row = 0;
    };

    void rebuild();
    [[nodiscard]] static int sortWeight(const JmapMailbox &mailbox);

    QList<JmapMailbox> _mailboxes;
    QList<Node *> _roots;
    QHash<QString, Node *> _byId;
};

} // namespace OCC

#endif // JMAPMAILBOXMODEL_H
