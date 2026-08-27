/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "RemoteFilesModel.h"

#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

#include <algorithm>

Q_LOGGING_CATEGORY(lcRemoteFilesModel, "souvera.files.remote")

namespace OCC {

namespace {

QString humanSize(qint64 bytes)
{
    constexpr qint64 kb = 1024;
    constexpr qint64 mb = kb * 1024;
    constexpr qint64 gb = mb * 1024;
    if (bytes >= gb) return QStringLiteral("%1 GB").arg(bytes / static_cast<double>(gb), 0, 'f', 1);
    if (bytes >= mb) return QStringLiteral("%1 MB").arg(bytes / static_cast<double>(mb), 0, 'f', 1);
    if (bytes >= kb) return QStringLiteral("%1 KB").arg(bytes / static_cast<double>(kb), 0, 'f', 0);
    return QStringLiteral("%1 B").arg(bytes);
}

} // namespace

RemoteFilesModel::RemoteFilesModel(AccountState *accountState, QObject *parent)
    : QAbstractListModel(parent)
    , _accountState(accountState)
{
}

void RemoteFilesModel::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) return;
    _accountState = accountState;
    if (!_currentPath.isEmpty()) {
        load(_currentPath);
    }
}

int RemoteFilesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return _entries.size();
}

QVariant RemoteFilesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= _entries.size()) return {};
    const auto &f = _entries.at(index.row());
    switch (role) {
    case NameRole: return f.name;
    case PathRole: return f.path;
    case FileIdRole: return f.fileId;
    case IsDirRole: return f.isDir;
    case SizeRole: return f.size;
    case ModifiedRole: return f.modified;
    case LockedRole: return f.locked;
    case LockOwnerRole: return f.lockOwner;
    case LockOwnerTypeRole: return f.lockOwnerType;
    case LockOwnerDisplayNameRole: return f.lockOwnerDisplayName;
    case Qt::DisplayRole: {
        auto text = f.name;
        if (f.isDir) {
            text += QLatin1Char('/');
        } else {
            QStringList details;
            if (f.size > 0) {
                details << humanSize(f.size);
            }
            if (f.modified.isValid()) {
                details << f.modified.toString(QStringLiteral("dd.MM.yyyy"));
            }
            if (!details.isEmpty()) {
                text += QStringLiteral("    (%1)").arg(details.join(QStringLiteral(" \u00B7 ")));
            }
        }
        if (f.locked) {
            const auto owner = f.lockOwnerDisplayName.isEmpty() ? f.lockOwner : f.lockOwnerDisplayName;
            text += QStringLiteral("    \U0001F512 %1").arg(owner.isEmpty() ? QStringLiteral("gesperrt") : owner);
        }
        return text;
    }
    default: return {};
    }
}

QHash<int, QByteArray> RemoteFilesModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {FileIdRole, "fileId"},
        {IsDirRole, "isDir"},
        {SizeRole, "size"},
        {ModifiedRole, "modified"},
        {LockedRole, "locked"},
        {LockOwnerRole, "lockOwner"},
        {LockOwnerTypeRole, "lockOwnerType"},
        {LockOwnerDisplayNameRole, "lockOwnerDisplayName"},
    };
}

QString RemoteFilesModel::parentPath() const
{
    if (_currentPath.isEmpty()) return QString();
    const auto idx = _currentPath.lastIndexOf(QLatin1Char('/'));
    return idx <= 0 ? QString() : _currentPath.left(idx);
}

RemoteFileInfo RemoteFilesModel::fileAt(int row) const
{
    if (row < 0 || row >= _entries.size()) return {};
    return _entries.at(row);
}

QString RemoteFilesModel::webUrl(const QString &remotePath, bool isDir) const
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return {};
    if (isDir) {
        QUrl url = acc->url();
        url.setPath(acc->url().path() + QStringLiteral("/index.php/apps/files/"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("dir"), QLatin1Char('/') + remotePath);
        url.setQuery(q);
        return url.toString();
    }
    return QStringLiteral("%1/index.php/f/%2").arg(acc->url().toString(), remotePath);
}

QUrl RemoteFilesModel::davUrl(const QString &remotePath) const
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return {};
    auto url = acc->url();
    url.setPath(acc->url().path() + QStringLiteral("/remote.php/dav/files/%1/%2").arg(acc->davUser(), remotePath));
    return url;
}

void RemoteFilesModel::load(const QString &remotePath)
{
    if (!_accountState) {
        emit loadFailed(QStringLiteral("Kein Konto verbunden."));
        return;
    }
    _currentPath = remotePath;
    emit pathChanged(remotePath);
    fetchDirectory(remotePath);
}

void RemoteFilesModel::fetchDirectory(const QString &remotePath)
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    const auto creds = acc ? acc->credentials() : nullptr;
    if (!acc || !creds) return;

    emit loadingChanged(true);

    if (_activeReply) {
        _activeReply->abort();
        _activeReply = nullptr;
    }
    const auto generation = ++_generation;

    QNetworkRequest req(davUrl(remotePath));
    const auto cred = QStringLiteral("%1:%2").arg(creds->user(), creds->password()).toUtf8().toBase64();
    req.setRawHeader("Authorization", QByteArray("Basic ") + cred);
    req.setRawHeader("Depth", "1");

    const auto prefix = QStringLiteral("/remote.php/dav/files/") + acc->davUser() + QLatin1Char('/');

    auto *nam = acc->networkAccessManager();
    auto *reply = nam->sendCustomRequest(req, "PROPFIND");
    _activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, remotePath, prefix, generation]() {
        reply->deleteLater();
        if (generation != _generation) {
            return;
        }
        _activeReply = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
            emit loadingChanged(false);
            emit loadFailed(reply->errorString());
            return;
        }

        QList<RemoteFileInfo> entries;
        QXmlStreamReader xml(reply->readAll());
        RemoteFileInfo current;
        QString currentHref;
        QString propName;
        QString propText;
        bool collectingHref = false;
        bool inProp = false;

        while (!xml.atEnd()) {
            const auto tok = xml.readNext();
            if (tok == QXmlStreamReader::StartElement) {
                const auto name = xml.name().toString();
                if (name == QLatin1String("response")) {
                    current = {};
                    currentHref.clear();
                } else if (name == QLatin1String("href") && !inProp) {
                    collectingHref = true;
                    currentHref.clear();
                } else if (name == QLatin1String("collection")) {
                    current.isDir = true;
                } else if (name == QLatin1String("prop") && !inProp) {
                    inProp = true;
                } else if (inProp) {
                    propName = name;
                    propText.clear();
                }
            } else if (tok == QXmlStreamReader::Characters) {
                const auto text = xml.text().toString();
                if (collectingHref) {
                    currentHref += text;
                } else if (!propName.isEmpty()) {
                    propText += text;
                }
            } else if (tok == QXmlStreamReader::EndElement) {
                const auto name = xml.name().toString();
                if (name == QLatin1String("href") && collectingHref) {
                    collectingHref = false;
                } else if (inProp && name == propName) {
                    if (propName == QLatin1String("getcontentlength")) {
                        current.size = propText.toLongLong();
                    } else if (propName == QLatin1String("getlastmodified")) {
                        current.modified = QDateTime::fromString(propText.trimmed(), Qt::RFC2822Date);
                    } else if (propName == QLatin1String("fileid")) {
                        current.fileId = propText.trimmed();
                    } else if (propName == QLatin1String("lock-owner")) {
                        current.lockOwner = propText.trimmed();
                        if (!current.lockOwner.isEmpty()) {
                            current.locked = true;
                        }
                    } else if (propName == QLatin1String("lock-owner-type")) {
                        current.lockOwnerType = propText.trimmed();
                    } else if (propName == QLatin1String("lock-owner-displayname")) {
                        current.lockOwnerDisplayName = propText.trimmed();
                    }
                    propName.clear();
                } else if (name == QLatin1String("prop") && inProp) {
                    inProp = false;
                    propName.clear();
                    propText.clear();
                } else if (name == QLatin1String("response") && !currentHref.isEmpty()) {
                    auto path = QString::fromUtf8(QByteArray::fromPercentEncoding(currentHref.toUtf8()));
                    if (path.startsWith(prefix)) {
                        path.remove(0, prefix.size());
                    } else if (path == prefix.left(prefix.size() - 1)) {
                        path.clear();
                    }
                    if (path.endsWith(QLatin1Char('/'))) {
                        path.chop(1);
                    }
                    if (!path.isEmpty() && path != remotePath) {
                        current.path = path;
                        current.name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
                        if (current.name.isEmpty()) {
                            current.name = path;
                        }
                        entries.append(current);
                    }
                }
            }
        }

        if (xml.hasError()) {
            qCWarning(lcRemoteFilesModel) << "PROPFIND XML error:" << xml.errorString();
            emit loadFailed(xml.errorString());
            emit loadingChanged(false);
            return;
        }

        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            if (a.isDir != b.isDir) return a.isDir;
            return QString::localeAwareCompare(a.name, b.name) < 0;
        });

        beginResetModel();
        _entries = entries;
        endResetModel();
        emit loadingChanged(false);
    });
}

} // namespace OCC
