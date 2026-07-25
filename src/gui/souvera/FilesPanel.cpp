/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "FilesPanel.h"

#include "folderman.h"
#include "folder.h"
#include "syncresult.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcFilesPanel, "souvera.filespanel")

namespace OCC {

FilesPanel::FilesPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("FilesPanel"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setObjectName(QStringLiteral("ContentArea"));

    _folderContainer = new QWidget(_scrollArea);
    _folderContainer->setObjectName(QStringLiteral("ContentArea"));
    _foldersLayout = new QVBoxLayout(_folderContainer);
    _foldersLayout->setContentsMargins(0, 0, 0, 0);
    _foldersLayout->setSpacing(8);

    _scrollArea->setWidget(_folderContainer);
    layout->addWidget(_scrollArea);

    refreshFolderList();

    connect(FolderMan::instance(), &FolderMan::folderSyncStateChange,
            this, [this](Folder *) { refreshFolderList(); });
    connect(FolderMan::instance(), &FolderMan::folderListChanged,
            this, [this](const Folder::Map &) { refreshFolderList(); });
}

void FilesPanel::refreshFolderList()
{
    while (auto *item = _foldersLayout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    auto folders = FolderMan::instance()->map();
    if (folders.isEmpty()) {
        auto *label = new QLabel(QStringLiteral("No sync folders configured"), _folderContainer);
        label->setObjectName(QStringLiteral("EmptyLabel"));
        label->setAlignment(Qt::AlignCenter);
        _foldersLayout->addWidget(label);
    } else {
        for (auto *folder : folders) {
            addFolderRow(folder);
        }
    }

    _foldersLayout->addStretch();
}

void FilesPanel::addFolderRow(Folder *folder)
{
    auto *row = new QFrame(_folderContainer);
    row->setObjectName(QStringLiteral("FolderRow"));
    row->setFrameShape(QFrame::NoFrame);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 0, 12, 0);

    auto *aliasLabel = new QLabel(folder->alias(), row);
    aliasLabel->setObjectName(QStringLiteral("FolderAliasLabel"));
    rowLayout->addWidget(aliasLabel);

    rowLayout->addStretch();

    auto *statusLabel = new QLabel(syncStatusText(folder), row);
    statusLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    rowLayout->addWidget(statusLabel);

    auto *openBtn = new QPushButton(QStringLiteral("Open Folder"), row);
    openBtn->setCursor(Qt::PointingHandCursor);
    connect(openBtn, &QPushButton::clicked, this, [folder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder->path()));
    });
    rowLayout->addWidget(openBtn);

    _foldersLayout->addWidget(row);
}

QString FilesPanel::syncStatusText(Folder *folder)
{
    if (folder->syncPaused()) {
        return QStringLiteral("Paused");
    }
    if (folder->isSyncRunning()) {
        return QStringLiteral("Syncing\u2026");
    }

    switch (folder->syncResult().status()) {
    case SyncResult::Success:
        return QStringLiteral("Up to date");
    case SyncResult::Problem:
        return QStringLiteral("Sync completed with warnings");
    case SyncResult::Error:
    case SyncResult::SetupError:
        return QStringLiteral("Sync error");
    case SyncResult::SyncAbortRequested:
        return QStringLiteral("Sync cancelled");
    case SyncResult::NotYetStarted:
    case SyncResult::Undefined:
    default:
        return QStringLiteral("Waiting");
    }
}

} // namespace OCC
