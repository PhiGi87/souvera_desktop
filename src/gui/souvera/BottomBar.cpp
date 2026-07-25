/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BottomBar.h"

#include <array>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcBottomBar, "souvera.bottombar")

namespace OCC {

struct TabInfo {
    QString icon;
    QString label;
};

static const auto TabData = std::array{
    TabInfo{QStringLiteral("\U0001F4C1"), QStringLiteral("Files")},
    TabInfo{QStringLiteral("\u2709"), QStringLiteral("Mail")},
    TabInfo{QStringLiteral("\U0001F4AC"), QStringLiteral("Talk")},
    TabInfo{QStringLiteral("\U0001F4CB"), QStringLiteral("Deck")},
    TabInfo{QStringLiteral("\U0001F4C5"), QStringLiteral("Calendar")},
};

BottomBar::BottomBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("BottomBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addStretch();

    for (auto i = 0; const auto &tab : TabData) {
        auto *btn = new QPushButton(this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);

        auto *btnLayout = new QVBoxLayout(btn);
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(2);
        btnLayout->setAlignment(Qt::AlignCenter);

        auto *iconLabel = new QLabel(tab.icon, btn);
        iconLabel->setAlignment(Qt::AlignCenter);
        auto *textLabel = new QLabel(tab.label, btn);
        textLabel->setAlignment(Qt::AlignCenter);

        btnLayout->addWidget(iconLabel);
        btnLayout->addWidget(textLabel);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            setCurrentIndex(i);
            emit currentChanged(i);
        });

        layout->addWidget(btn);
        _buttons.append(btn);
        ++i;
    }

    layout->addStretch();

    if (!_buttons.isEmpty()) {
        _buttons.first()->setChecked(true);
    }
}

void BottomBar::setCurrentIndex(int index)
{
    for (auto i = 0; i < _buttons.size(); ++i) {
        _buttons[i]->setChecked(i == index);
    }
}

} // namespace OCC
