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

    auto refreshUser = [this]() {
        const auto accounts = AccountManager::instance()->accounts();
        if (accounts.isEmpty()) {
            _userLabel->setText(QStringLiteral("Nicht verbunden"));
            return;
        }
        const auto acc = accounts.first()->account();
        const auto creds = acc ? acc->credentials() : nullptr;
        const auto user = creds ? creds->user() : QString();
        const auto host = acc ? acc->url().host() : QString();
        // The login id may already BE the mail address (email login) —
        // appending the workspace host would duplicate the domain.
        if (user.isEmpty()) {
            _userLabel->setText(QStringLiteral("Nicht verbunden"));
        } else {
            _userLabel->setText(user.contains(QLatin1Char('@'))
                ? user
                : QStringLiteral("%1@%2").arg(user, host));
        }
    };

    _userLabel = new QLabel(this);
    _userLabel->setObjectName(QStringLiteral("UserEmailLabel"));
    refreshUser();
    layout->addWidget(_userLabel);

    connect(AccountManager::instance(), &AccountManager::accountAdded,
            this, refreshUser);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
            this, refreshUser);

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
    if (_settingsButton->icon().isNull()) {
        _settingsButton->setText(QStringLiteral("\u2699"));
    }
    connect(_settingsButton, &QPushButton::clicked, this, &StatusHeader::settingsClicked);
    layout->addWidget(_settingsButton);

    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        _settingsButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("settings"), SouveraTheme::Color::TextMuted));
        if (_settingsButton->icon().isNull()) {
            _settingsButton->setText(QStringLiteral("\u2699"));
        }
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
        _syncTextLabel->setText(QStringLiteral("Keine Sync-Ordner"));
        return;
    }

    if (fm->isAnySyncRunning()) {
        _syncIconLabel->setText(QStringLiteral("\u25B6"));
        _syncTextLabel->setText(QStringLiteral("Dateien werden synchronisiert\u2026"));
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
        _syncTextLabel->setText(QStringLiteral("Datei-Sync-Fehler"));
    } else if (hasWarning) {
        _syncIconLabel->setText(QStringLiteral("\u26A0"));
        _syncTextLabel->setText(QStringLiteral("Datei-Sync-Warnung"));
    } else if (allGood) {
        _syncIconLabel->setText(QStringLiteral("\u2714"));
        _syncTextLabel->setText(QStringLiteral("Dateien synchron"));
    } else {
        _syncIconLabel->setText(QStringLiteral("\u25CB"));
        _syncTextLabel->setText(QStringLiteral("Warte\u2026"));
    }
}

} // namespace OCC
