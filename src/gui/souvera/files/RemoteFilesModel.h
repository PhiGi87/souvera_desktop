/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef REMOTEFILESMODEL_H
#define REMOTEFILESMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QUrl>

class QNetworkReply;
#include <QUrl>

namespace OCC {

class AccountState;

struct RemoteFileInfo {
    QString name;
    QString path; //!< Remote path below the user's files root, without leading slash
    QString fileId;
    bool isDir = false;
    qint64 size = 0;
    QDateTime modified;
    bool locked = false;
    QString lockOwner;
    QString lockOwnerType; //!< "office" when the lock is held by an office session
    QString lockOwnerDisplayName;
};

class RemoteFilesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        FileIdRole,
        IsDirRole,
        SizeRole,
        ModifiedRole,
        LockedRole,
        LockOwnerRole,
        LockOwnerTypeRole,
        LockOwnerDisplayNameRole,
    };

    explicit RemoteFilesModel(AccountState *accountState, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void load(const QString &remotePath);
    [[nodiscard]] QString currentPath() const { return _currentPath; }
    [[nodiscard]] QString parentPath() const;

    [[nodiscard]] RemoteFileInfo fileAt(int row) const;
    [[nodiscard]] QString webUrl(const QString &remotePath, bool isDir) const;
    [[nodiscard]] QUrl davUrl(const QString &remotePath) const;

    void setAccountState(AccountState *accountState);

signals:
    void loadingChanged(bool loading);
    void loadFailed(const QString &error);
    void pathChanged(const QString &path);

private:
    void fetchDirectory(const QString &remotePath);

    AccountState *_accountState = nullptr;
    QList<RemoteFileInfo> _entries;
    QString _currentPath;
    QPointer<QNetworkReply> _activeReply;
    int _generation = 0;
};

} // namespace OCC

#endif // REMOTEFILESMODEL_H
