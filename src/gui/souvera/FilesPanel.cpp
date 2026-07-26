/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "FilesPanel.h"

#include "folderman.h"
#include "folder.h"
#include "syncresult.h"
#include "progressdispatcher.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QProgressBar>
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

    auto *headerLabel = new QLabel(QStringLiteral("Datei-Synchronisation"), this);
    headerLabel->setObjectName(QStringLiteral("FilesHeaderLabel"));
    layout->addWidget(headerLabel);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setObjectName(QStringLiteral("ContentArea"));

    _folderContainer = new QWidget(_scrollArea);
    _folderContainer->setObjectName(QStringLiteral("ContentArea"));
    _foldersLayout = new QVBoxLayout(_folderContainer);
    _foldersLayout->setContentsMargins(0, 0, 0, 0);
    _foldersLayout->setSpacing(12);

    _scrollArea->setWidget(_folderContainer);
    layout->addWidget(_scrollArea, 1);

    refreshFolderList();

    connect(FolderMan::instance(), &FolderMan::folderSyncStateChange,
            this, [this](Folder *) { refreshFolderList(); });
    connect(FolderMan::instance(), &FolderMan::folderListChanged,
            this, [this](const Folder::Map &) { refreshFolderList(); });
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo,
            this, [this](const QString &folderAlias, const ProgressInfo &info) {
        updateProgress(folderAlias, info);
    });
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
        auto *label = new QLabel(QStringLiteral("Keine Synchronisationsordner konfiguriert"), _folderContainer);
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
    auto *rowLayout = new QVBoxLayout(row);
    rowLayout->setContentsMargins(16, 12, 16, 12);
    rowLayout->setSpacing(6);

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);

    auto *aliasLabel = new QLabel(folder->alias(), row);
    aliasLabel->setObjectName(QStringLiteral("FolderAliasLabel"));
    topRow->addWidget(aliasLabel);

    topRow->addStretch();

    auto *statusLabel = new QLabel(syncStatusText(folder), row);
    statusLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    topRow->addWidget(statusLabel);

    auto *openBtn = new QPushButton(QStringLiteral("Ordner \u00F6ffnen"), row);
    openBtn->setCursor(Qt::PointingHandCursor);
    connect(openBtn, &QPushButton::clicked, this, [folder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder->path()));
    });
    topRow->addWidget(openBtn);

    rowLayout->addLayout(topRow);

    auto *progressBar = new QProgressBar(row);
    progressBar->setObjectName(QStringLiteral("FolderProgressBar"));
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setVisible(false);
    rowLayout->addWidget(progressBar);

    auto *errorLabel = new QLabel(row);
    errorLabel->setObjectName(QStringLiteral("FolderErrorLabel"));
    errorLabel->setVisible(false);
    errorLabel->setWordWrap(true);
    rowLayout->addWidget(errorLabel);

    _folderRows[folder->alias()] = {statusLabel, progressBar, errorLabel};

    _foldersLayout->addWidget(row);
}

void FilesPanel::updateProgress(const QString &folderAlias, const ProgressInfo &info)
{
    auto it = _folderRows.find(folderAlias);
    if (it == _folderRows.end()) return;

    auto &[statusLabel, progressBar, errorLabel] = it.value();

    auto total = info.totalFiles();
    auto completed = info.completedFiles();

    if (info.status() == ProgressInfo::Propagation && total > 0) {
        progressBar->setValue(static_cast<int>(completed * 100 / total));
        progressBar->setVisible(true);

        statusLabel->setText(QStringLiteral("Synchronisiere %1/%2 Dateien")
            .arg(completed).arg(total));
    } else {
        progressBar->setValue(100);
        progressBar->setVisible(false);
    }
}

QString FilesPanel::syncStatusText(Folder *folder)
{
    if (folder->syncPaused()) {
        return QStringLiteral("Pausiert");
    }
    if (folder->isSyncRunning()) {
        return QStringLiteral("Synchronisiere\u2026");
    }

    switch (folder->syncResult().status()) {
    case SyncResult::Success:
        return QStringLiteral("Aktuell");
    case SyncResult::Problem:
        return QStringLiteral("Sync mit Warnungen abgeschlossen");
    case SyncResult::Error:
    case SyncResult::SetupError:
        return QStringLiteral("Sync-Fehler");
    case SyncResult::SyncAbortRequested:
        return QStringLiteral("Sync abgebrochen");
    case SyncResult::NotYetStarted:
    case SyncResult::Undefined:
    default:
        return QStringLiteral("Warte");
    }
}

} // namespace OCC
