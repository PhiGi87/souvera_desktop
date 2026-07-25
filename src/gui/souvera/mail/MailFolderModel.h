/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILFOLDERMODEL_H
#define MAILFOLDERMODEL_H

#include <QAbstractListModel>
#include <QStringList>

namespace OCC {

class MailFolderModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit MailFolderModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setFolders(const QStringList &folders);

private:
    QStringList _folders;
};

} // namespace OCC

#endif // MAILFOLDERMODEL_H
