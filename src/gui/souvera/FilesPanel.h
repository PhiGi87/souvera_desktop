/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FILESPANEL_H
#define FILESPANEL_H

#include <QWidget>

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace OCC {

class Folder;

class FilesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FilesPanel(QWidget *parent = nullptr);

private:
    void refreshFolderList();
    void addFolderRow(Folder *folder);
    static QString syncStatusText(Folder *folder);

    QScrollArea *_scrollArea = nullptr;
    QWidget *_folderContainer = nullptr;
    QVBoxLayout *_foldersLayout = nullptr;
};

} // namespace OCC

#endif // FILESPANEL_H
