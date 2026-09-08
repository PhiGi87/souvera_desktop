/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OFFICEMANAGER_H
#define OFFICEMANAGER_H

#include <QHash>
#include <QObject>
#include <QUrl>

namespace OCC {

class AccountState;
class OfficeWindow;

/**
 * @brief Opens office documents for editing ("Souvera Office").
 *
 * While a document is open it is locked on the server (WebDAV LOCK) so
 * other users and office sessions cannot modify it concurrently. Remote
 * files are downloaded to a local cache first and uploaded back on save;
 * files inside a sync folder are edited in place so the sync engine
 * propagates the changes.
 */
class OfficeManager : public QObject
{
    Q_OBJECT
public:
    static OfficeManager *instance();

    void openDocument(AccountState *accountState, const QString &remotePath,
                      const QString &fileName, const QString &fileId,
                      const QString &localPathIfSynced);

private:
    explicit OfficeManager(QObject *parent = nullptr);

    void lockRemote(AccountState *accountState, const QUrl &davUrl, OfficeWindow *window);
    void unlockRemote(AccountState *accountState, const QUrl &davUrl, const QString &token);
    void uploadFile(AccountState *accountState, const QUrl &davUrl, const QString &localPath);
    void downloadThenOpen(AccountState *accountState, const QUrl &davUrl,
                          const QString &remotePath, const QString &fileName,
                          const QString &fileId);
    void openWindow(const QString &localPath, const QString &fileName,
                    AccountState *accountState, const QString &remotePath,
                    const QUrl &davUrl, bool uploadOnSave);
    [[nodiscard]] QUrl davUrlFor(AccountState *accountState, const QString &remotePath) const;
    [[nodiscard]] QUrl collaboraUrlFor(AccountState *accountState, const QString &fileId,
                                       const QString &remotePath) const;

    struct OpenDocument {
        OfficeWindow *window = nullptr;
        AccountState *accountState = nullptr;
        QString remotePath;
        QUrl davUrl;
        QString lockToken;
        QString localPath;
        bool uploadOnSave = false;
    };

    QHash<OfficeWindow *, OpenDocument> _open;

private slots:
    void onWindowClosed(OfficeWindow *window);
    void onDocumentSaved(const QString &localPath);
};

} // namespace OCC

#endif // OFFICEMANAGER_H
