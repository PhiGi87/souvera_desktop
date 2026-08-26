/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "StatusHeader.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "folderman.h"
#include "folder.h"
#include "syncresult.h"
#include "theme/SouveraTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>

Q_LOGGING_CATEGORY(lcStatusHeader, "souvera.statusheader")

namespace OCC {

StatusHeader::StatusHeader(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("StatusHeader"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);

    auto accounts = AccountManager::instance()->accounts();
    auto email = accounts.isEmpty() ? QStringLiteral("Nicht verbunden")
                                    : accounts.first()->account()->credentials()->user();
    auto host = accounts.isEmpty() ? QString()
                                   : accounts.first()->account()->url().host();

    auto displayText = accounts.isEmpty() ? QStringLiteral("Nicht verbunden")
                                          : QStringLiteral("%1@%2").arg(email, host);

    _userLabel = new QLabel(displayText, this);
    _userLabel->setObjectName(QStringLiteral("UserEmailLabel"));
    layout->addWidget(_userLabel);

    layout->addStretch();

    _syncIconLabel = new QLabel(this);
    _syncIconLabel->setObjectName(QStringLiteral("SyncIconLabel"));
    _syncIconLabel->setFixedWidth(20);
    layout->addWidget(_syncIconLabel);

    _syncTextLabel = new QLabel(QStringLiteral("Synchronisiert"), this);
    _syncTextLabel->setObjectName(QStringLiteral("SyncTextLabel"));
    layout->addWidget(_syncTextLabel);

    layout->addSpacing(24);

    _settingsButton = new QPushButton(this);
    _settingsButton->setObjectName(QStringLiteral("SettingsButton"));
    _settingsButton->setCursor(Qt::PointingHandCursor);
    _settingsButton->setToolTip(QStringLiteral("Einstellungen"));
    _settingsButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("settings"), SouveraTheme::Color::TextMuted));
    _settingsButton->setIconSize(QSize(18, 18));
    connect(_settingsButton, &QPushButton::clicked, this, &StatusHeader::settingsClicked);
    layout->addWidget(_settingsButton);

    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        _settingsButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("settings"), SouveraTheme::Color::TextMuted));
    });

    updateSyncStatus();

    connect(FolderMan::instance(), &FolderMan::folderSyncStateChange, this, [this](Folder *) {
        updateSyncStatus();
    });
    connect(FolderMan::instance(), &FolderMan::folderListChanged, this, [this](const Folder::Map &) {
        updateSyncStatus();
    });
}

void StatusHeader::updateSyncStatus()
{
    auto *fm = FolderMan::instance();
    auto folders = fm->map();

    if (folders.isEmpty()) {
        _syncIconLabel->setText(QStringLiteral("\u25CB"));
        _syncTextLabel->setText(QStringLiteral("Keine Ordner"));
        return;
    }

    if (fm->isAnySyncRunning()) {
        _syncIconLabel->setText(QStringLiteral("\u25B6"));
        _syncTextLabel->setText(QStringLiteral("Synchronisiere\u2026"));
        return;
    }

    bool hasError = false;
    bool hasWarning = false;
    bool allGood = true;

    for (auto *folder : folders) {
        if (folder->syncPaused()) {
            allGood = false;
            continue;
        }
        switch (folder->syncResult().status()) {
        case SyncResult::Error:
        case SyncResult::SetupError:
            hasError = true;
            allGood = false;
            break;
        case SyncResult::Problem:
            hasWarning = true;
            allGood = false;
            break;
        case SyncResult::NotYetStarted:
        case SyncResult::Undefined:
            allGood = false;
            break;
        default:
            break;
        }
    }

    if (hasError) {
        _syncIconLabel->setText(QStringLiteral("\u2716"));
        _syncTextLabel->setText(QStringLiteral("Sync-Fehler"));
    } else if (hasWarning) {
        _syncIconLabel->setText(QStringLiteral("\u26A0"));
        _syncTextLabel->setText(QStringLiteral("Warnung"));
    } else if (allGood) {
        _syncIconLabel->setText(QStringLiteral("\u2714"));
        _syncTextLabel->setText(QStringLiteral("Synchronisiert"));
    } else {
        _syncIconLabel->setText(QStringLiteral("\u25CB"));
        _syncTextLabel->setText(QStringLiteral("Warte\u2026"));
    }
}

} // namespace OCC
