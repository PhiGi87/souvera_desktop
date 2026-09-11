/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckCardWidget.h"
#include "theme/SouveraTheme.h"

#include <QContextMenuEvent>
#include <QJsonArray>
#include <QDate>
#include <QDateTime>
#include <QDrag>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMetaType>
#include <QMouseEvent>
#include <QMimeData>
#include <QVBoxLayout>

namespace OCC {

namespace {
constexpr auto DeckCardMimeType = "application/x-souvera-deck-card";
}

DeckCardWidget::DeckCardWidget(const QJsonObject &cardData, int stackId, QWidget *parent)
    : QFrame(parent)
    , _cardData(cardData)
    , _cardId(cardData.value(QStringLiteral("id")).toInt())
    , _stackId(stackId)
{
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(5);

    _labelsContainer = new QWidget(this);
    _labelsLayout = new QHBoxLayout(_labelsContainer);
    _labelsLayout->setContentsMargins(0, 0, 0, 0);
    _labelsLayout->setSpacing(4);
    _labelsLayout->setAlignment(Qt::AlignLeft);
    _labelsContainer->setVisible(false);
    layout->addWidget(_labelsContainer);

    _titleLabel = new QLabel(this);
    _titleLabel->setWordWrap(true);
    layout->addWidget(_titleLabel);

    const auto description = cardData.value(QStringLiteral("description")).toString();
    if (!description.isEmpty()) {
        _descriptionLabel = new QLabel(this);
        _descriptionLabel->setWordWrap(true);
        _descriptionLabel->setMaximumHeight(34);
        layout->addWidget(_descriptionLabel);
    }

    _metaContainer = new QWidget(this);
    _metaLayout = new QHBoxLayout(_metaContainer);
    _metaLayout->setContentsMargins(0, 0, 0, 0);
    _metaLayout->setSpacing(6);
    _metaLayout->setAlignment(Qt::AlignLeft);
    layout->addWidget(_metaContainer);

    setTitle(cardData.value(QStringLiteral("title")).toString());
    setDescription(description);
    renderLabels();
    renderMeta();

    applyTheme();
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyTheme();
    });
}

void DeckCardWidget::setTitle(const QString &title)
{
    _titleLabel->setText(title);
}

void DeckCardWidget::setDescription(const QString &description)
{
    if (_descriptionLabel) {
        _descriptionLabel->setText(description);
        _descriptionLabel->setVisible(!description.isEmpty());
    }
}

void DeckCardWidget::updateFrom(const QJsonObject &cardData)
{
    _cardData = cardData;
    _cardId = cardData.value(QStringLiteral("id")).toInt();
    setTitle(cardData.value(QStringLiteral("title")).toString());
    setDescription(cardData.value(QStringLiteral("description")).toString());
    renderLabels();
    renderMeta();
}

void DeckCardWidget::renderLabels()
{
    while (auto *item = _labelsLayout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const auto labels = _cardData.value(QStringLiteral("labels")).toArray();
    for (const auto &labelVal : labels) {
        const auto labelObj = labelVal.toObject();
        const auto color = QColor(labelObj.value(QStringLiteral("color")).toString());
        if (!color.isValid()) continue;

        auto *chip = new QLabel(labelObj.value(QStringLiteral("title")).toString(), _labelsContainer);
        // Dark text on bright label colors, light text on dark ones.
        const auto isBright = color.lightness() > 140;
        chip->setStyleSheet(QStringLiteral(
            "background-color: %1; color: %2; border-radius: 4px;"
            "font-size: 10px; font-weight: bold; padding: 1px 6px;")
            .arg(color.name(),
                 isBright ? QStringLiteral("#1c2430") : QStringLiteral("#ffffff")));
        chip->setVisible(!chip->text().isEmpty() || true);
        _labelsLayout->addWidget(chip);
    }
    _labelsContainer->setVisible(!labels.isEmpty());
}

void DeckCardWidget::renderMeta()
{
    while (auto *item = _metaLayout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    _dueLabel = nullptr;

    // Due date badge — red when overdue, muted otherwise. The Deck API has
    // shipped both an ISO date string and an object with a timestamp.
    QDateTime due;
    const auto dueVal = _cardData.value(QStringLiteral("duedate"));
    if (dueVal.isObject()) {
        const auto dueMs = static_cast<qint64>(dueVal.toObject().value(QStringLiteral("timestamp")).toDouble());
        if (dueMs > 0) due = QDateTime::fromMSecsSinceEpoch(dueMs * 1000);
    } else if (dueVal.isString() && !dueVal.toString().isEmpty()) {
        due = QDateTime::fromString(dueVal.toString(), Qt::ISODate);
    }
    if (due.isValid()) {
        const auto overdue = due < QDateTime::currentDateTime();
        _dueLabel = new QLabel(QStringLiteral("\u23F0 ") + due.toString(QStringLiteral("dd.MM. HH:mm")),
                               _metaContainer);
        _dueLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; padding: 1px 6px;"
            "background-color: %2; border-radius: 4px;")
            .arg(overdue ? QStringLiteral("#fecaca") : SouveraTheme::instance()->color(SouveraTheme::Color::TextMuted).name(),
                 overdue ? QStringLiteral("#b91c1c") : "rgba(128,128,128,40)"));
        _metaLayout->addWidget(_dueLabel);
    }

    const auto description = _cardData.value(QStringLiteral("description")).toString();
    if (!description.isEmpty()) {
        auto *descIcon = new QLabel(QStringLiteral("\u2261"), _metaContainer);
        descIcon->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: rgba(128,128,128,160); background: transparent;"));
        _metaLayout->addWidget(descIcon);
    }
    _metaContainer->setVisible(_metaLayout->count() > 0);
}

void DeckCardWidget::applyTheme()
{
    const auto *theme = SouveraTheme::instance();
    setStyleSheet(QStringLiteral(
        "DeckCardWidget { background-color: %1; border-radius: 10px; border: 1px solid %2; }"
        "DeckCardWidget:hover { border-color: %3; background-color: %4; }")
        .arg(theme->color(SouveraTheme::Color::Surface).name(),
             theme->color(SouveraTheme::Color::Border).name(),
             theme->color(SouveraTheme::Color::Accent).name(),
             theme->color(SouveraTheme::Color::SurfaceHover).name()));

    _titleLabel->setStyleSheet(QStringLiteral(
        "font-weight: 600; font-size: 13px; color: %1; background: transparent;")
        .arg(theme->color(SouveraTheme::Color::TextPrimary).name()));
    if (_descriptionLabel) {
        _descriptionLabel->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: %1; background: transparent;")
            .arg(theme->color(SouveraTheme::Color::TextSecondary).name()));
    }
}

void DeckCardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _pressPos = event->pos();
        _dragArmed = true;
    }
    QFrame::mousePressEvent(event);
}

void DeckCardWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!_dragArmed || !(event->buttons() & Qt::LeftButton)) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - _pressPos).manhattanLength() < 12) {
        return;
    }
    _dragArmed = false;

    auto *mime = new QMimeData;
    mime->setData(QLatin1String(DeckCardMimeType),
                  QStringLiteral("%1:%2").arg(_cardId).arg(_stackId).toUtf8());
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(grab().scaled(180, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->exec(Qt::MoveAction);

    QFrame::mouseMoveEvent(event);
}

void DeckCardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    _dragArmed = false;
    QFrame::mouseReleaseEvent(event);
}

void DeckCardWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    emit editRequested(_cardId, _stackId);
    QFrame::mouseDoubleClickEvent(event);
}

void DeckCardWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(QStringLiteral("\u270F\uFE0F Bearbeiten"), this, [this]() {
        emit editRequested(_cardId, _stackId);
    });
    menu.addAction(QStringLiteral("\U0001F5D1\uFE0F L\u00F6schen"), this, [this]() {
        emit deleteRequested(_cardId, _stackId);
    });
    menu.exec(event->globalPos());
}

} // namespace OCC
