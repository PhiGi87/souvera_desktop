/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "LeftSidebar.h"
#include <QVBoxLayout>
#include <QFont>

namespace OCC {

LeftSidebar::LeftSidebar(QWidget *parent)
    : QWidget(parent)
    , _list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    _list->setIconSize(QSize(22, 22));
    _list->setSpacing(2);
    _list->setFont(QFont(QStringLiteral("system-ui"), 11));
    _list->setStyleSheet(QStringLiteral(
        "QListWidget { background: #16213e; border: none; color: #a0b4d0; }"
        "QListWidget::item { padding: 10px 14px; border-left: 3px solid transparent; }"
        "QListWidget::item:hover { background: #1a2744; color: #e0e6f0; }"
        "QListWidget::item:selected { background: #1a2744; border-left-color: #4BBFEA; color: #ffffff; }"
    ));
    layout->addWidget(_list);
    _list->setMinimumWidth(200);
    _list->setMaximumWidth(240);

    connect(_list, &QListWidget::currentRowChanged, this, &LeftSidebar::currentChanged);
}

void LeftSidebar::addItem(const QString &icon, const QString &label)
{
    auto *item = new QListWidgetItem(icon + QLatin1String("  ") + label);
    item->setSizeHint(QSize(0, 44));
    _list->addItem(item);
}

void LeftSidebar::setCurrentIndex(int index)
{
    _list->setCurrentRow(index);
}

} // namespace OCC
