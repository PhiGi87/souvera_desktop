/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILFOLDERMODEL_H
#define MAILFOLDERMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QIcon>

namespace OCC {

enum class FolderIcon {
    Inbox,
    Sent,
    Drafts,
    Trash,
    Junk,
    Archive,
    Generic
};

struct FolderInfo {
    QString name;
    FolderIcon icon = FolderIcon::Generic;
    int unreadCount = 0;
};

class MailFolderModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        UnreadCountRole = Qt::UserRole + 1,
        IconTypeRole,
        FolderDisplayRole
    };

    explicit MailFolderModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setFolders(const QList<FolderInfo> &folders);
    void setUnreadCount(const QString &folderName, int count);

private:
    static int sortWeight(const FolderInfo &folder);
    static QIcon iconForType(FolderIcon type);

    QList<FolderInfo> _folders;
};

} // namespace OCC

#endif // MAILFOLDERMODEL_H
