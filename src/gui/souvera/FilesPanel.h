/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FILESPANEL_H
#define FILESPANEL_H

#include <QWidget>
#include <QMap>
#include <QString>

class QLabel;
class QProgressBar;
class QScrollArea;
class QVBoxLayout;
class QPushButton;
class QTreeView;
class QModelIndex;

namespace OCC {

class Folder;
class ProgressInfo;
class AccountState;
class RemoteFilesModel;
struct RemoteFileInfo;

class FilesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FilesPanel(QWidget *parent = nullptr);

    void setAccountState(AccountState *accountState);

private:
    void refreshFolderList();
    void addFolderRow(Folder *folder);
    void updateProgress(const QString &folderAlias, const ProgressInfo &info);
    static QString syncStatusText(Folder *folder);

    void setupRemoteBrowser();
    void navigateUp();
    void onRemoteFileActivated(const QModelIndex &index);
    void onOpenInBrowser();
    void onEditInOffice();
    void onEditLocally();
    void updateRemoteActions();

    [[nodiscard]] RemoteFileInfo selectedFile() const;
    [[nodiscard]] QString officeKind() const;
    [[nodiscard]] QString officeUrl(const QString &fileId, const QString &path) const;
    [[nodiscard]] bool isOfficeMime(const QString &fileName) const;
    [[nodiscard]] QString localPathForRemote(const QString &remotePath) const;

    QScrollArea *_scrollArea = nullptr;
    QWidget *_folderContainer = nullptr;
    QVBoxLayout *_foldersLayout = nullptr;

    struct FolderRowWidgets {
        QLabel *statusLabel = nullptr;
        QProgressBar *progressBar = nullptr;
        QLabel *errorLabel = nullptr;
    };
    QMap<QString, FolderRowWidgets> _folderRows;

    AccountState *_accountState = nullptr;
    RemoteFilesModel *_remoteModel = nullptr;
    QTreeView *_remoteView = nullptr;
    QLabel *_breadcrumbLabel = nullptr;
    QPushButton *_upBtn = nullptr;
    QPushButton *_openBtn = nullptr;
    QPushButton *_officeBtn = nullptr;
    QPushButton *_editLocalBtn = nullptr;
};

} // namespace OCC

#endif // FILESPANEL_H
