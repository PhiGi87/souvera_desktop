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
#include <QEnterEvent>
#include <QGraphicsOpacityEffect>
#include <QToolButton>
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

    // Round done toggle at the card bottom-left (rendered in renderMeta).

    // Hover quick actions (edit/delete), shown on enterEvent.
    _hoverActions = new QWidget(this);
    auto *hoverLayout = new QHBoxLayout(_hoverActions);
    hoverLayout->setContentsMargins(0, 0, 0, 0);
    hoverLayout->setSpacing(2);
    auto *editBtn = new QToolButton(_hoverActions);
    editBtn->setText(QStringLiteral("\u270F\uFE0F"));
    editBtn->setToolTip(QStringLiteral("Bearbeiten"));
    editBtn->setFixedSize(22, 22);
    connect(editBtn, &QToolButton::clicked, this, [this]() {
        emit editRequested(_cardId, _stackId);
    });
    auto *deleteBtn = new QToolButton(_hoverActions);
    deleteBtn->setText(QStringLiteral("\U0001F5D1\uFE0F"));
    deleteBtn->setToolTip(QStringLiteral("L\u00F6schen"));
    deleteBtn->setFixedSize(22, 22);
    connect(deleteBtn, &QToolButton::clicked, this, [this]() {
        emit deleteRequested(_cardId, _stackId);
    });
    hoverLayout->addWidget(editBtn);
    hoverLayout->addWidget(deleteBtn);
    _hoverActions->hide();
    _hoverActions->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; border-radius: 6px; background: transparent; font-size: 11px; }"
        "QToolButton:hover { background: rgba(128,128,128,50); }"));

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
    const auto isDone = _cardData.value(QStringLiteral("done")).toBool();
    _titleLabel->setText(title);
    QFont f = _titleLabel->font();
    f.setStrikeOut(isDone);
    _titleLabel->setFont(f);
    setGraphicsEffect(nullptr);
    if (isDone) {
        auto *opacity = new QGraphicsOpacityEffect(this);
        opacity->setOpacity(0.7);
        setGraphicsEffect(opacity);
    }
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

    // Round done toggle
    if (!_doneCircle) {
        _doneCircle = new QWidget(_metaContainer);
        auto *doneLayout = new QHBoxLayout(_doneCircle);
        doneLayout->setContentsMargins(0, 0, 0, 0);
        auto *doneBtn = new QToolButton(_doneCircle);
        doneBtn->setFixedSize(18, 18);
        doneBtn->setToolTip(QStringLiteral("Als erledigt markieren"));
        connect(doneBtn, &QToolButton::clicked, this, [this]() {
            emit doneToggleRequested(_cardId, _stackId, !_cardData.value(QStringLiteral("done")).toBool());
        });
        doneLayout->addWidget(doneBtn);
        _doneCircle->setProperty("doneButton", QVariant::fromValue<QToolButton *>(doneBtn));
    }
    if (auto *doneBtn = _doneCircle->property("doneButton").value<QToolButton *>()) {
        const auto isDone = _cardData.value(QStringLiteral("done")).toBool();
        doneBtn->setText(isDone ? QStringLiteral("\u2713") : QString());
        doneBtn->setStyleSheet(QStringLiteral(
            "QToolButton { border: 2px solid %1; border-radius: 9px; background: %2;"
            "  color: #ffffff; font-weight: bold; font-size: 11px; }")
            .arg(SouveraTheme::instance()->color(SouveraTheme::Color::Border).name(),
                 isDone ? SouveraTheme::instance()->color(SouveraTheme::Color::Success).name()
                        : QStringLiteral("transparent")));
    }
    _metaLayout->addWidget(_doneCircle);

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

    // Attachment + comment counters
    const auto attachmentCount = _cardData.value(QStringLiteral("attachmentCount")).toInt(
        _cardData.value(QStringLiteral("attachments")).toArray().size());
    if (attachmentCount > 0) {
        auto *attachIcon = new QLabel(QStringLiteral("\U0001F4CE ") + QString::number(attachmentCount),
                                      _metaContainer);
        attachIcon->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: rgba(128,128,128,180); background: transparent;"));
        _metaLayout->addWidget(attachIcon);
    }
    const auto commentCount = _cardData.value(QStringLiteral("commentsUnread")).toInt(0);
    if (commentCount > 0) {
        auto *commentIcon = new QLabel(QStringLiteral("\U0001F4AC ") + QString::number(commentCount),
                                       _metaContainer);
        commentIcon->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: rgba(128,128,128,180); background: transparent;"));
        _metaLayout->addWidget(commentIcon);
    }

    // Assignee initial avatars
    const auto assignees = _cardData.value(QStringLiteral("assignedUsers")).toArray();
    for (const auto &assigneeVal : assignees) {
        const auto participant = assigneeVal.toObject().value(QStringLiteral("participant")).toObject();
        const auto displayName = participant.value(QStringLiteral("displayName")).toString();
        const auto uid = participant.value(QStringLiteral("primaryKey")).toString(
            participant.value(QStringLiteral("uid")).toString());
        const auto initial = (displayName.isEmpty() ? uid : displayName).left(1).toUpper();
        auto *avatar = new QLabel(initial, _metaContainer);
        avatar->setFixedSize(18, 18);
        avatar->setAlignment(Qt::AlignCenter);
        const auto hue = qHash(uid.isEmpty() ? displayName : uid) % 360;
        avatar->setStyleSheet(QStringLiteral(
            "background-color: %1; color: #ffffff; border-radius: 9px;"
            "font-size: 10px; font-weight: bold;")
            .arg(QColor::fromHsl(hue, 120, 110).name()));
        avatar->setToolTip(displayName);
        _metaLayout->addWidget(avatar);
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

void DeckCardWidget::enterEvent(QEnterEvent *event)
{
    if (_hoverActions) {
        _hoverActions->move(width() - _hoverActions->sizeHint().width() - 8, 6);
        _hoverActions->raise();
        _hoverActions->show();
    }
    QFrame::enterEvent(event);
}

void DeckCardWidget::leaveEvent(QEvent *event)
{
    if (_hoverActions) _hoverActions->hide();
    QFrame::leaveEvent(event);
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
