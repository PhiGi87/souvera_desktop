/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "FilesPanel.h"

#include "RemoteFilesModel.h"
#include "account.h"
#include "accountstate.h"
#include "capabilities.h"
#include "folderman.h"
#include "folder.h"
#include "syncresult.h"
#include "progressdispatcher.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTreeView>

Q_LOGGING_CATEGORY(lcFilesPanel, "souvera.filespanel")

namespace OCC {

namespace {
const QStringList officeMimeExtensions{
    QStringLiteral("odt"), QStringLiteral("ods"), QStringLiteral("odp"), QStringLiteral("odg"),
    QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("xls"), QStringLiteral("xlsx"),
    QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("csv"), QStringLiteral("rtf"),
    QStringLiteral("txt"), QStringLiteral("md"),
};
}

FilesPanel::FilesPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("FilesPanel"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *headerLabel = new QLabel(QStringLiteral("Datei-Synchronisation"), this);
    headerLabel->setObjectName(QStringLiteral("FilesHeaderLabel"));
    layout->addWidget(headerLabel);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setObjectName(QStringLiteral("ContentArea"));
    _scrollArea->setMaximumHeight(240);

    _folderContainer = new QWidget(_scrollArea);
    _folderContainer->setObjectName(QStringLiteral("ContentArea"));
    _foldersLayout = new QVBoxLayout(_folderContainer);
    _foldersLayout->setContentsMargins(0, 0, 0, 0);
    _foldersLayout->setSpacing(12);

    _scrollArea->setWidget(_folderContainer);
    layout->addWidget(_scrollArea);

    setupRemoteBrowser();
    layout->addWidget(_remoteView, 1);

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

void FilesPanel::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) return;
    _accountState = accountState;
    if (_remoteModel) {
        _remoteModel->setAccountState(accountState);
    }
    if (accountState) {
        refreshRemoteFiles();
    }
}

void FilesPanel::setupRemoteBrowser()
{
    auto *remoteHeader = new QLabel(QStringLiteral("Dateien auf dem Server"), this);
    remoteHeader->setObjectName(QStringLiteral("FilesHeaderLabel"));

    _upBtn = new QPushButton(QStringLiteral("\u2191 Nach oben"), this);
    _upBtn->setEnabled(false);

    _breadcrumbLabel = new QLabel(QStringLiteral("/"), this);
    _breadcrumbLabel->setObjectName(QStringLiteral("FolderStatusLabel"));

    auto *remoteToolbar = new QHBoxLayout;
    remoteToolbar->setContentsMargins(0, 0, 0, 0);
    remoteToolbar->addWidget(remoteHeader);
    remoteToolbar->addSpacing(12);
    remoteToolbar->addWidget(_upBtn);
    remoteToolbar->addWidget(_breadcrumbLabel, 1);

    auto *actionsLayout = new QHBoxLayout;
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    _openBtn = new QPushButton(QStringLiteral("\u00D6ffnen im Browser"), this);
    _officeBtn = new QPushButton(QStringLiteral("In Souvera Office bearbeiten"), this);
    _editLocalBtn = new QPushButton(QStringLiteral("Lokal bearbeiten"), this);
    _openBtn->setEnabled(false);
    _officeBtn->setEnabled(false);
    _editLocalBtn->setEnabled(false);
    actionsLayout->addWidget(_openBtn);
    actionsLayout->addWidget(_officeBtn);
    actionsLayout->addWidget(_editLocalBtn);
    actionsLayout->addStretch();

    _remoteView = new QTreeView(this);
    _remoteView->setObjectName(QStringLiteral("MailFolderView"));
    _remoteView->setRootIsDecorated(false);
    _remoteView->setAlternatingRowColors(true);
    _remoteView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _remoteView->setSelectionMode(QAbstractItemView::SingleSelection);
    _remoteView->setUniformRowHeights(true);

    _remoteModel = new RemoteFilesModel(_accountState, this);
    _remoteView->setModel(_remoteModel);
    _remoteView->setColumnWidth(0, 340);

    auto *browserLayout = qobject_cast<QVBoxLayout *>(layout());
    if (browserLayout) {
        browserLayout->addLayout(remoteToolbar);
        browserLayout->addLayout(actionsLayout);
    }

    connect(_upBtn, &QPushButton::clicked, this, &FilesPanel::navigateUp);
    connect(_openBtn, &QPushButton::clicked, this, &FilesPanel::onOpenInBrowser);
    connect(_officeBtn, &QPushButton::clicked, this, &FilesPanel::onEditInOffice);
    connect(_editLocalBtn, &QPushButton::clicked, this, &FilesPanel::onEditLocally);
    connect(_remoteView, &QTreeView::doubleClicked, this, &FilesPanel::onRemoteFileActivated);
    connect(_remoteView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &cur, const QModelIndex &) { Q_UNUSED(cur) updateRemoteActions(); });
    connect(_remoteModel, &RemoteFilesModel::pathChanged, this, [this](const QString &path) {
        _breadcrumbLabel->setText(QLatin1Char('/') + path);
        _upBtn->setEnabled(!path.isEmpty());
        updateRemoteActions();
    });
}

void FilesPanel::refreshRemoteFiles()
{
    if (_remoteModel && _accountState) {
        _remoteModel->load(_remoteModel->currentPath());
    }
}

void FilesPanel::navigateUp()
{
    if (!_remoteModel) return;
    const auto parent = _remoteModel->parentPath();
    _remoteModel->load(parent);
}

RemoteFileInfo FilesPanel::selectedFile() const
{
    const auto idx = _remoteView->currentIndex();
    if (!idx.isValid() || !_remoteModel) return {};
    return _remoteModel->fileAt(idx.row());
}

QString FilesPanel::officeKind() const
{
    if (!_accountState) return {};
    const auto acc = _accountState->account();
    if (!acc) return {};
    const auto &caps = acc->capabilities().raw();
    if (caps.contains(QStringLiteral("richdocuments"))) return QStringLiteral("richdocuments");
    if (caps.contains(QStringLiteral("onlyoffice"))) return QStringLiteral("onlyoffice");
    return {};
}

bool FilesPanel::isOfficeMime(const QString &fileName) const
{
    const auto suffix = QFileInfo(fileName).suffix().toLower();
    return officeMimeExtensions.contains(suffix);
}

QString FilesPanel::officeUrl(const QString &fileId, const QString &path) const
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return {};
    const auto base = acc->url().toString();
    const auto kind = officeKind();
    if (kind == QLatin1String("onlyoffice")) {
        return QStringLiteral("%1/index.php/apps/onlyoffice/%2").arg(base, fileId);
    }
    // richdocuments (Collabora) - prefer the fileId based route
    if (!fileId.isEmpty()) {
        return QStringLiteral("%1/index.php/apps/richdocuments/index?fileId=%2").arg(base, fileId);
    }
    return QStringLiteral("%1/index.php/apps/richdocuments/index?path=/%2").arg(base, path);
}

QString FilesPanel::localPathForRemote(const QString &remotePath) const
{
    const auto folders = FolderMan::instance()->map();
    for (const auto *folder : folders) {
        auto remote = folder->remotePathTrailingSlash();
        if (remotePath.startsWith(remote)) {
            const auto relative = remotePath.mid(remote.size());
            const auto local = QDir::cleanPath(folder->path() + QLatin1Char('/') + relative);
            if (QFileInfo::exists(local)) {
                return local;
            }
        }
    }
    return {};
}

void FilesPanel::onRemoteFileActivated(const QModelIndex &index)
{
    if (!index.isValid() || !_remoteModel) return;
    const auto file = _remoteModel->fileAt(index.row());
    if (file.path.isEmpty()) return;
    if (file.isDir) {
        _remoteModel->load(file.path);
        return;
    }
    onEditLocally();
}

void FilesPanel::onOpenInBrowser()
{
    const auto file = selectedFile();
    if (file.path.isEmpty()) return;
    const auto url = _remoteModel->webUrl(file.path, file.isDir);
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void FilesPanel::onEditInOffice()
{
    const auto file = selectedFile();
    if (file.path.isEmpty()) return;

    if (file.locked && file.lockOwnerType != QLatin1String("office")) {
        const auto owner = file.lockOwnerDisplayName.isEmpty() ? file.lockOwner : file.lockOwnerDisplayName;
        QMessageBox::warning(this, QStringLiteral("Datei gesperrt"),
            QStringLiteral("Diese Datei wird gerade von %1 bearbeitet.\nBitte versuche es sp\u00E4ter erneut.").arg(owner));
        return;
    }

    const auto url = officeUrl(file.fileId, file.path);
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void FilesPanel::onEditLocally()
{
    const auto file = selectedFile();
    if (file.path.isEmpty() || file.isDir) return;

    if (file.locked) {
        const auto owner = file.lockOwnerDisplayName.isEmpty() ? file.lockOwner : file.lockOwnerDisplayName;
        QMessageBox::warning(this, QStringLiteral("Datei gesperrt"),
            QStringLiteral("Diese Datei wird gerade von %1 bearbeitet.\nBitte versuche es sp\u00E4ter erneut.").arg(owner));
        return;
    }

    const auto local = localPathForRemote(file.path);
    if (!local.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(local));
        return;
    }

    // Fallback: download the file and open it with the local default application.
    if (!_accountState) return;
    const auto acc = _accountState->account();
    if (!acc) return;

    const auto downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const auto target = QDir::cleanPath(downloadDir + QLatin1Char('/') + file.name);

    QNetworkRequest req(_remoteModel->davUrl(file.path));
    const auto creds = acc->credentials();
    if (creds) {
        const auto cred = QStringLiteral("%1:%2").arg(creds->user(), creds->password()).toUtf8().toBase64();
        req.setRawHeader("Authorization", QByteArray("Basic ") + cred);
    }
    auto *reply = acc->networkAccessManager()->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, target]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, QStringLiteral("Download fehlgeschlagen"), reply->errorString());
            return;
        }
        QFile out(target);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(reply->readAll());
            out.close();
            QDesktopServices::openUrl(QUrl::fromLocalFile(target));
        }
    });
}

void FilesPanel::updateRemoteActions()
{
    const auto file = selectedFile();
    const auto hasFile = !file.path.isEmpty();
    const auto officeAvailable = !officeKind().isEmpty() && isOfficeMime(file.name);
    _openBtn->setEnabled(hasFile);
    _officeBtn->setEnabled(hasFile && !file.isDir && officeAvailable);
    _editLocalBtn->setEnabled(hasFile && !file.isDir);
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
