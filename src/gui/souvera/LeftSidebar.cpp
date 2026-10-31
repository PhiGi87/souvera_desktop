/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "LeftSidebar.h"
#include "theme/SouveraMetrics.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "account.h"
#include "creds/abstractcredentials.h"
#include "theme/SouveraTheme.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace OCC {

namespace Metrics = Sou::Metrics;

namespace {
constexpr QSize BadgeCircleSize(16, 16);
constexpr QSize BadgeWideSize(24, 16); // "99+" needs a wider pill
} // namespace

QString badgeStyleSheet()
{
    const auto danger = SouveraTheme::instance()->color(SouveraTheme::Color::Danger).name();
    return QStringLiteral(
        "QLabel { background: %1; color: #FFFFFF; font-size: 10px; font-weight: bold;"
        " border: none; border-radius: 8px; }")
        .arg(danger);
}

LeftSidebar::LeftSidebar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("LeftSidebar"));
    setFixedWidth(72);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    _topLayout = new QVBoxLayout;
    _topLayout->setContentsMargins(0, Metrics::SpacingM, 0, 0);
    _topLayout->setSpacing(Metrics::SpacingXS);
    mainLayout->addLayout(_topLayout);

    mainLayout->addStretch();

    _bottomLayout = new QVBoxLayout;
    _bottomLayout->setContentsMargins(0, 0, 0, Metrics::SpacingM);
    _bottomLayout->setSpacing(Metrics::SpacingS);
    mainLayout->addLayout(_bottomLayout);

    setupThemeToggle();
    setupAvatar();

    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyIconTints();
        updateThemeToggleIcon();
        refreshBadgeStyles();
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

void LeftSidebar::setBadge(int index, int count)
{
    if (index < 0 || index >= _buttons.size()) return;
    auto &entry = _buttons[index];
    if (!entry.badge) {
        entry.badge = new QLabel(entry.button);
        entry.badge->setObjectName(QStringLiteral("SidebarBadge"));
        entry.badge->setAlignment(Qt::AlignCenter);
        entry.button->installEventFilter(this);
    }

    const auto clamped = qMax(0, count);
    entry.badgeCount = clamped;
    if (clamped == 0) {
        entry.badge->hide();
        return;
    }

    const auto text = clamped > 99 ? QStringLiteral("99+") : QString::number(clamped);
    entry.badge->setText(text);
    entry.badge->setStyleSheet(badgeStyleSheet());
    entry.badge->setFixedSize(text == QLatin1String("99+") ? BadgeWideSize : BadgeCircleSize);
    positionBadge(index);
    entry.badge->raise();
    entry.badge->show();
}

bool LeftSidebar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        for (auto i = 0; i < _buttons.size(); ++i) {
            if (_buttons.at(i).button == watched) {
                positionBadge(i);
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LeftSidebar::positionBadge(int index)
{
    const auto &entry = _buttons.at(index);
    if (!entry.badge || !entry.button) return;
    // The icon is a fixed 32x32 label centered at the top of the button
    // (see addItem()); the badge sits on its top-right corner.
    const auto iconLeft = (entry.button->width() - entry.icon->width()) / 2;
    const auto iconTop = 8; // top margin of the button's inner layout
    entry.badge->move(iconLeft + entry.icon->width() - entry.badge->width() + 4, iconTop - 4);
}

void LeftSidebar::refreshBadgeStyles()
{
    for (const auto &entry : _buttons) {
        if (entry.badge && entry.badgeCount > 0) {
            entry.badge->setStyleSheet(badgeStyleSheet());
        }
    }
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
