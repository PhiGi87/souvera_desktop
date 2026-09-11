/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "EmojiPicker.h"

#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace OCC {

namespace {
const QStringList kSmileys = {
    QStringLiteral("\U0001F600"), QStringLiteral("\U0001F602"), QStringLiteral("\U0001F605"),
    QStringLiteral("\U0001F609"), QStringLiteral("\U0001F60A"), QStringLiteral("\U0001F607"),
    QStringLiteral("\U0001F60D"), QStringLiteral("\U0001F618"), QStringLiteral("\U0001F61C"),
    QStringLiteral("\U0001F62D"), QStringLiteral("\U0001F621"), QStringLiteral("\U0001F631"),
    QStringLiteral("\U0001F60E"), QStringLiteral("\U0001F971"), QStringLiteral("\U0001F634"),
    QStringLiteral("\U0001F62C"),
};
const QStringList kGestures = {
    QStringLiteral("\U0001F44D"), QStringLiteral("\U0001F44C"), QStringLiteral("\U0001F44F"),
    QStringLiteral("\U0001F64F"), QStringLiteral("\U0001F44B"), QStringLiteral("\U0001F4AA"),
    QStringLiteral("\U0001F91D"), QStringLiteral("\U0001F446"), QStringLiteral("\U0001F447"),
    QStringLiteral("\U0001F64C"), QStringLiteral("\U0001F929"), QStringLiteral("\U0001F9BE"),
};
const QStringList kHearts = {
    QStringLiteral("\u2764\uFE0F"), QStringLiteral("\U0001F9E1"), QStringLiteral("\U0001F49B"),
    QStringLiteral("\U0001F49A"), QStringLiteral("\U0001F499"), QStringLiteral("\U0001F49C"),
    QStringLiteral("\U0001F494"), QStringLiteral("\U0001F495"), QStringLiteral("\U0001F496"),
};
const QStringList kObjects = {
    QStringLiteral("\u2705"), QStringLiteral("\u274C"), QStringLiteral("\u2757"),
    QStringLiteral("\U0001F525"), QStringLiteral("\u2B50"), QStringLiteral("\U0001F389"),
    QStringLiteral("\U0001F381"), QStringLiteral("\U0001F4C5"), QStringLiteral("\U0001F4CB"),
    QStringLiteral("\U0001F4BB"), QStringLiteral("\U0001F4B0"), QStringLiteral("\U0001F4DE"),
    QStringLiteral("\U0001F4E7"), QStringLiteral("\U0001F680"), QStringLiteral("\U0001F441\uFE0F"),
    QStringLiteral("\U0001F512"),
};
}

EmojiPicker::EmojiPicker(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("EmojiPicker"));
    setFrameShape(QFrame::StyledPanel);
    setWindowFlags(Qt::Popup);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    _grid = new QGridLayout;
    _grid->setSpacing(2);

    int row = 0;
    addCategory(QStringLiteral("Smileys"), kSmileys, row);
    addCategory(QStringLiteral("Gesten"), kGestures, row);
    addCategory(QStringLiteral("Herzen"), kHearts, row);
    addCategory(QStringLiteral("Objekte"), kObjects, row);

    layout->addLayout(_grid);
}

void EmojiPicker::addCategory(const QString &title, const QStringList &emojis, int &row)
{
    auto *label = new QLabel(title, this);
    label->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px; padding-top: 4px;"));
    _grid->addWidget(label, row++, 0, 1, 8);

    int col = 0;
    for (const auto &emoji : emojis) {
        auto *btn = new QPushButton(emoji, this);
        btn->setFixedSize(34, 34);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { border: none; border-radius: 6px; font-size: 17px; background: transparent; }"
            "QPushButton:hover { background: rgba(128,128,128,40); }"));
        connect(btn, &QPushButton::clicked, this, [this, emoji]() {
            emit emojiClicked(emoji);
            close();
        });
        _grid->addWidget(btn, row, col);
        ++col;
        if (col >= 8) {
            col = 0;
            ++row;
        }
    }
    ++row;
}

void EmojiPicker::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    setFocus();
}

bool EmojiPicker::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            close();
            return true;
        }
    }
    return QFrame::event(event);
}

} // namespace OCC
