/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "LeftSidebar.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "account.h"
#include "creds/abstractcredentials.h"
#include "theme/SouveraTheme.h"

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
    _bottomLayout->setSpacing(8);
    mainLayout->addLayout(_bottomLayout);

    setupThemeToggle();
    setupAvatar();

    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyIconTints();
        updateThemeToggleIcon();
    });
}

void LeftSidebar::addItem(const QString &iconName, const QString &label)
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

    auto *iconLabel = new QLabel(btn);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setObjectName(QStringLiteral("SidebarIcon"));
    iconLabel->setFixedSize(32, 32);

    auto *textLabel = new QLabel(label, btn);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setObjectName(QStringLiteral("SidebarText"));

    inner->addWidget(iconLabel, 0, Qt::AlignHCenter);
    inner->addWidget(textLabel);

    const auto idx = _buttons.size();
    connect(btn, &QPushButton::clicked, this, [this, idx]() {
        if (idx != _currentIndex) {
            setCurrentIndex(idx);
            emit currentChanged(idx);
        }
    });

    _topLayout->addWidget(btn);

    SidebarButton entry;
    entry.button = btn;
    entry.icon = iconLabel;
    entry.text = textLabel;
    entry.iconName = iconName;
    _buttons.append(entry);

    if (_buttons.size() == 1) {
        btn->setChecked(true);
    }

    applyIconTints();
}

void LeftSidebar::setCurrentIndex(int index)
{
    if (index < 0 || index >= _buttons.size()) return;
    _currentIndex = index;
    for (auto i = 0; i < _buttons.size(); ++i) {
        _buttons[i].button->setChecked(i == index);
    }
    applyIconTints();
}

void LeftSidebar::setupAvatar()
{
    const auto accounts = AccountManager::instance()->accounts();
    if (accounts.isEmpty()) {
        return;
    }

    const auto acc = accounts.first()->account();
    const auto creds = acc->credentials();
    const auto user = creds ? creds->user() : QString();
    const auto initial = user.isEmpty() ? QStringLiteral("?") : user.left(1).toUpper();

    auto *avatar = new QPushButton(initial, this);
    avatar->setObjectName(QStringLiteral("SidebarAvatar"));
    avatar->setCursor(Qt::PointingHandCursor);
    avatar->setToolTip(user);
    _bottomLayout->addWidget(avatar, 0, Qt::AlignHCenter);
}

void LeftSidebar::setupThemeToggle()
{
    _themeToggle = new QPushButton(this);
    _themeToggle->setObjectName(QStringLiteral("ThemeToggleButton"));
    _themeToggle->setCursor(Qt::PointingHandCursor);
    _themeToggle->setToolTip(QStringLiteral("Hell/Dunkel umschalten"));
    _themeToggle->setFixedSize(40, 40);
    updateThemeToggleIcon();

    connect(_themeToggle, &QPushButton::clicked, this, []() {
        SouveraTheme::instance()->toggleTheme();
    });

    _bottomLayout->addWidget(_themeToggle, 0, Qt::AlignHCenter);
}

void LeftSidebar::updateThemeToggleIcon()
{
    if (!_themeToggle) {
        return;
    }

    const auto *theme = SouveraTheme::instance();
    const auto iconName = theme->theme() == SouveraTheme::Theme::Dark
        ? QStringLiteral("moon")
        : QStringLiteral("sun");
    _themeToggle->setIcon(theme->icon(iconName, SouveraTheme::Color::TextMuted));
    _themeToggle->setIconSize(QSize(20, 20));
}

void LeftSidebar::applyIconTints()
{
    const auto *theme = SouveraTheme::instance();
    for (auto i = 0; i < _buttons.size(); ++i) {
        const auto tint = i == _currentIndex
            ? SouveraTheme::Color::Accent
            : SouveraTheme::Color::TextSecondary;
        _buttons[i].icon->setPixmap(theme->pixmap(_buttons[i].iconName, tint, 24));
    }
}

} // namespace OCC
