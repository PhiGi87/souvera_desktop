/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
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

namespace OCC {

class Folder;
class ProgressInfo;

class FilesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FilesPanel(QWidget *parent = nullptr);

private:
    void refreshFolderList();
    void addFolderRow(Folder *folder);
    void updateProgress(const QString &folderAlias, const ProgressInfo &info);
    static QString syncStatusText(Folder *folder);

    QScrollArea *_scrollArea = nullptr;
    QWidget *_folderContainer = nullptr;
    QVBoxLayout *_foldersLayout = nullptr;

    struct FolderRowWidgets {
        QLabel *statusLabel = nullptr;
        QProgressBar *progressBar = nullptr;
        QLabel *errorLabel = nullptr;
    };
    QMap<QString, FolderRowWidgets> _folderRows;
};

} // namespace OCC

#endif // FILESPANEL_H
