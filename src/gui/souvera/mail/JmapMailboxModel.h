/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef JMAPMAILBOXMODEL_H
#define JMAPMAILBOXMODEL_H

#include "JmapClient.h"
#include <QAbstractListModel>

namespace OCC {

class JmapMailboxModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        RoleRole,
        UnreadRole,
        TotalRole,
        ParentIdRole,
    };

    explicit JmapMailboxModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMailboxes(const QList<JmapMailbox> &mailboxes);

    QString mailboxIdForRow(int row) const;
    QList<QString> roleIds() const;

private:
    QList<JmapMailbox> _mailboxes;
};

} // namespace OCC

#endif // JMAPMAILBOXMODEL_H
