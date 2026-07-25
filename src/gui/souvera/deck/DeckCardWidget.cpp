/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckCardWidget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

namespace OCC {

DeckCardWidget::DeckCardWidget(const QString &title, const QString &description, QWidget *parent)
    : QFrame(parent)
{
    setStyleSheet(QStringLiteral(
        "DeckCardWidget { background-color: white; border-radius: 8px; border: 1px solid #e0e0e0; }"
        "DeckCardWidget:hover { border-color: #4a90d9; background-color: #fafafa; }"));

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(6);
    shadow->setOffset(0, 1);
    shadow->setColor(QColor(0, 0, 0, 20));
    setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    _labelsContainer = new QWidget(this);
    auto *labelsLayout = new QHBoxLayout(_labelsContainer);
    labelsLayout->setContentsMargins(0, 0, 0, 0);
    labelsLayout->setSpacing(4);
    _labelsContainer->setVisible(false);
    layout->addWidget(_labelsContainer);

    _titleLabel = new QLabel(title, this);
    _titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #1a1a1a;"));
    _titleLabel->setWordWrap(true);
    layout->addWidget(_titleLabel);

    if (!description.isEmpty()) {
        _descriptionLabel = new QLabel(description, this);
        _descriptionLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #666666;"));
        _descriptionLabel->setWordWrap(true);
        layout->addWidget(_descriptionLabel);
    }

    layout->addStretch();
}

void DeckCardWidget::setTitle(const QString &title)
{
    _titleLabel->setText(title);
}

void DeckCardWidget::setDescription(const QString &description)
{
    if (!_descriptionLabel) {
        _descriptionLabel = new QLabel(description, this);
        _descriptionLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #666666;"));
        _descriptionLabel->setWordWrap(true);
        auto *parentLayout = qobject_cast<QVBoxLayout *>(layout());
        if (parentLayout) {
            parentLayout->insertWidget(parentLayout->count() - 1, _descriptionLabel);
        }
    } else {
        _descriptionLabel->setText(description);
    }
    _descriptionLabel->setVisible(!description.isEmpty());
}

void DeckCardWidget::addLabel(const QString &color, const QString &labelTitle)
{
    auto *tag = new QFrame(_labelsContainer);
    tag->setFixedHeight(18);
    tag->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #%1; border-radius: 3px; }").arg(color));

    auto *tagLayout = new QHBoxLayout(tag);
    tagLayout->setContentsMargins(5, 1, 5, 1);

    auto *tagLabel = new QLabel(labelTitle, tag);
    tagLabel->setStyleSheet(QStringLiteral("font-size: 9px; color: white; font-weight: bold; background: transparent;"));
    tagLayout->addWidget(tagLabel);

    auto *containerLayout = qobject_cast<QHBoxLayout *>(_labelsContainer->layout());
    if (containerLayout) {
        containerLayout->addWidget(tag);
    }
    _labelsContainer->setVisible(true);
}

void DeckCardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QFrame::mousePressEvent(event);
}

} // namespace OCC
