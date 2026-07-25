/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailFolderModel.h"

#include <QLoggingCategory>
#include <QStyle>
#include <QApplication>

Q_LOGGING_CATEGORY(lcMailFolderModel, "souvera.mail.foldermodel")

namespace OCC {

MailFolderModel::MailFolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MailFolderModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return _folders.size();
}

QVariant MailFolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _folders.size()) return {};

    const auto &folder = _folders.at(index.row());

    switch (role) {
    case NameRole:
        return folder.name;
    case Qt::DecorationRole:
        return iconForType(folder.icon);
    case UnreadCountRole:
        return folder.unreadCount;
    case IconTypeRole:
        return static_cast<int>(folder.icon);
    case FolderDisplayRole:
        return folder.unreadCount > 0
            ? QStringLiteral("%1 (%2)").arg(folder.name).arg(folder.unreadCount)
            : folder.name;
    case Qt::ToolTipRole:
        return folder.unreadCount > 0
            ? QStringLiteral("%1 - %2 ungelesen").arg(folder.name).arg(folder.unreadCount)
            : folder.name;
    default:
        return {};
    }
}

void MailFolderModel::setFolders(const QList<FolderInfo> &folders)
{
    beginResetModel();

    auto sorted = folders;
    std::sort(sorted.begin(), sorted.end(), [](const FolderInfo &a, const FolderInfo &b) {
        return sortWeight(a) < sortWeight(b);
    });

    _folders = sorted;
    endResetModel();

    qCInfo(lcMailFolderModel) << "Folders updated, count:" << _folders.size();
}

void MailFolderModel::setUnreadCount(const QString &folderName, int count)
{
    for (auto i = 0; i < _folders.size(); ++i) {
        if (_folders[i].name == folderName) {
            _folders[i].unreadCount = count;
            auto idx = index(i);
            emit dataChanged(idx, idx, {UnreadCountRole, FolderDisplayRole});
            return;
        }
    }
}

int MailFolderModel::sortWeight(const FolderInfo &folder)
{
    auto lower = folder.name.toLower();
    if (lower == QStringLiteral("inbox")) return 0;
    if (lower == QStringLiteral("gesendet") || lower == QStringLiteral("sent")) return 1;
    if (lower == QStringLiteral("entwürfe") || lower == QStringLiteral("drafts")) return 2;
    if (lower == QStringLiteral("gelöscht") || lower == QStringLiteral("papierkorb") || lower == QStringLiteral("trash")) return 3;
    if (lower == QStringLiteral("spam") || lower == QStringLiteral("junk")) return 4;
    return 10;
}

QIcon MailFolderModel::iconForType(FolderIcon type)
{
    auto *style = QApplication::style();
    switch (type) {
    case FolderIcon::Inbox:
        return style->standardIcon(QStyle::SP_DirIcon);
    case FolderIcon::Sent:
        return style->standardIcon(QStyle::SP_ArrowUp);
    case FolderIcon::Drafts:
        return style->standardIcon(QStyle::SP_FileIcon);
    case FolderIcon::Trash:
        return style->standardIcon(QStyle::SP_TrashIcon);
    case FolderIcon::Junk:
        return style->standardIcon(QStyle::SP_MessageBoxWarning);
    case FolderIcon::Archive:
        return style->standardIcon(QStyle::SP_DirHomeIcon);
    default:
        return style->standardIcon(QStyle::SP_DirIcon);
    }
}

} // namespace OCC
