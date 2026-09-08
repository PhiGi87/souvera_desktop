/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckCardWidget.h"
#include "theme/SouveraTheme.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>

namespace OCC {

DeckCardWidget::DeckCardWidget(const QString &title, const QString &description, QWidget *parent)
    : QFrame(parent)
{
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
    _titleLabel->setWordWrap(true);
    layout->addWidget(_titleLabel);

    if (!description.isEmpty()) {
        _descriptionLabel = new QLabel(description, this);
        _descriptionLabel->setWordWrap(true);
        layout->addWidget(_descriptionLabel);
    }

    layout->addStretch();

    applyTheme();
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyTheme();
    });
}

void DeckCardWidget::applyTheme()
{
    const auto *theme = SouveraTheme::instance();
    setStyleSheet(QStringLiteral(
        "DeckCardWidget { background-color: %1; border-radius: 8px; border: 1px solid %2; }"
        "DeckCardWidget:hover { border-color: %3; }")
        .arg(theme->color(SouveraTheme::Color::Surface).name(),
             theme->color(SouveraTheme::Color::Border).name(),
             theme->color(SouveraTheme::Color::Accent).name()));

    _titleLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 13px; color: %1; background: transparent;")
        .arg(theme->color(SouveraTheme::Color::TextPrimary).name()));

    if (_descriptionLabel) {
        applyLabelTheme(_descriptionLabel, false);
    }
}

void DeckCardWidget::applyLabelTheme(QLabel *label, bool bold)
{
    const auto *theme = SouveraTheme::instance();
    label->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: %2; background: transparent; font-weight: %3;")
        .arg(bold ? 13 : 11)
        .arg(theme->color(bold ? SouveraTheme::Color::TextPrimary
                               : SouveraTheme::Color::TextSecondary).name(),
             bold ? "bold" : "normal"));
}

void DeckCardWidget::setTitle(const QString &title)
{
    _titleLabel->setText(title);
}

void DeckCardWidget::setDescription(const QString &description)
{
    if (!_descriptionLabel) {
        _descriptionLabel = new QLabel(description, this);
        _descriptionLabel->setWordWrap(true);
        applyLabelTheme(_descriptionLabel, false);
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
