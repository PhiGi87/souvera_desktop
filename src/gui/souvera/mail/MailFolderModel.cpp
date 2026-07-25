/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailFolderModel.h"

namespace OCC {

MailFolderModel::MailFolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
    _folders = {
        QStringLiteral("INBOX"),
        QStringLiteral("Gesendet"),
        QStringLiteral("Entwürfe"),
        QStringLiteral("Papierkorb"),
        QStringLiteral("Spam"),
        QStringLiteral("Archiv")
    };
}

int MailFolderModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return _folders.size();
}

QVariant MailFolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _folders.size()) return {};
    if (role == Qt::DisplayRole) {
        return _folders.at(index.row());
    }
    return {};
}

void MailFolderModel::setFolders(const QStringList &folders)
{
    beginResetModel();
    _folders = folders;
    endResetModel();
}

} // namespace OCC
