/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef JMAPEMAILLISTMODEL_H
#define JMAPEMAILLISTMODEL_H

#include "JmapClient.h"
#include <QAbstractListModel>

namespace OCC {

class JmapEmailListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        SubjectRole,
        FromAddressRole,
        FromNameRole,
        ReceivedAtRole,
        IsReadRole,
        IsFlaggedRole,
        HasAttachmentRole,
        PreviewRole,
    };

    explicit JmapEmailListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEmails(const QList<JmapEmail> &emails);
    void setTotal(int total) { _total = total; }
    int total() const { return _total; }

    QString emailIdForRow(int row) const;
    void markRead(int row);

private:
    QList<JmapEmail> _emails;
    int _total = 0;
};

} // namespace OCC

#endif // JMAPEMAILLISTMODEL_H
