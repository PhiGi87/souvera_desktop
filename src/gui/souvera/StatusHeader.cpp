/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "StatusHeader.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "folderman.h"

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
    layout->setContentsMargins(12, 0, 12, 0);

    auto accounts = AccountManager::instance()->accounts();
    auto displayName = accounts.isEmpty() ? QStringLiteral("Not connected")
                                          : accounts.first()->account()->displayName();

    _userLabel = new QLabel(displayName, this);
    layout->addWidget(_userLabel);

    layout->addStretch();

    _syncStatusLabel = new QLabel(QStringLiteral("\u25CF"), this);
    layout->addWidget(_syncStatusLabel);

    _settingsButton = new QPushButton(QStringLiteral("\u2699"), this);
    _settingsButton->setCursor(Qt::PointingHandCursor);
    connect(_settingsButton, &QPushButton::clicked, this, &StatusHeader::settingsClicked);
    layout->addWidget(_settingsButton);

    connect(FolderMan::instance(), &FolderMan::folderSyncStateChange, this, [this](Folder *) {
        auto anyRunning = FolderMan::instance()->isAnySyncRunning();
        _syncStatusLabel->setText(anyRunning ? QStringLiteral("\u25B6") : QStringLiteral("\u25CF"));
    });
}

} // namespace OCC
