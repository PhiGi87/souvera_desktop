/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "LeftSidebar.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "account.h"
#include "creds/abstractcredentials.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace OCC {

LeftSidebar::LeftSidebar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("LeftSidebar"));
    setFixedWidth(64);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    _topLayout = new QVBoxLayout;
    _topLayout->setContentsMargins(0, 12, 0, 0);
    _topLayout->setSpacing(2);
    mainLayout->addLayout(_topLayout);

    mainLayout->addStretch();

    _bottomLayout = new QVBoxLayout;
    _bottomLayout->setContentsMargins(0, 0, 0, 12);
    _bottomLayout->setSpacing(2);
    mainLayout->addLayout(_bottomLayout);

    auto accounts = AccountManager::instance()->accounts();
    if (!accounts.isEmpty()) {
        const auto acc = accounts.first()->account();
        const auto user = acc->credentials()->user();
        const auto initial = user.isEmpty() ? QStringLiteral("?") : user.left(1).toUpper();

        auto *avatar = new QPushButton(initial, this);
        avatar->setObjectName(QStringLiteral("SidebarAvatar"));
        avatar->setCursor(Qt::PointingHandCursor);
        avatar->setToolTip(user);
        _bottomLayout->addWidget(avatar);
    }
}

void LeftSidebar::addItem(const QString &icon, const QString &label)
{
    auto *btn = new QPushButton(this);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setObjectName(QStringLiteral("SidebarButton"));
    btn->setToolTip(label);

    auto *inner = new QVBoxLayout(btn);
    inner->setContentsMargins(0, 8, 0, 6);
    inner->setSpacing(2);
    inner->setAlignment(Qt::AlignCenter);

    auto *iconLabel = new QLabel(icon, btn);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setObjectName(QStringLiteral("SidebarIcon"));

    auto *textLabel = new QLabel(label, btn);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setObjectName(QStringLiteral("SidebarText"));

    inner->addWidget(iconLabel);
    inner->addWidget(textLabel);

    const auto idx = _buttons.size();
    connect(btn, &QPushButton::clicked, this, [this, idx]() {
        if (idx != _currentIndex) {
            setCurrentIndex(idx);
            emit currentChanged(idx);
        }
    });

    _topLayout->addWidget(btn);
    _buttons.append(btn);

    if (_buttons.size() == 1) {
        btn->setChecked(true);
    }
}

void LeftSidebar::setCurrentIndex(int index)
{
    if (index < 0 || index >= _buttons.size()) return;
    _currentIndex = index;
    for (auto i = 0; i < _buttons.size(); ++i) {
        _buttons[i]->setChecked(i == index);
    }
}

} // namespace OCC
