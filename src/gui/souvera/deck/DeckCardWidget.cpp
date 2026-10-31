/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckCardWidget.h"
#include "DeckModels.h"
#include "theme/SouveraMetrics.h"
#include "theme/SouveraTheme.h"

#include <QContextMenuEvent>
#include <QDate>
#include <QDateTime>
#include <QDrag>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QEnterEvent>
#include <QToolButton>
#include <QMetaType>
#include <QMouseEvent>
#include <QMimeData>
#include <QResizeEvent>
#include <QTextLayout>
#include <QTextOption>
#include <QVBoxLayout>

namespace OCC {

namespace Metrics = Sou::Metrics;

namespace {
constexpr auto DeckCardMimeType = "application/x-souvera-deck-card";

constexpr int TitlePixelSize = 13;
constexpr int BodyPixelSize = 11; //!< meta row, due chip, counters and snippet
constexpr int AvatarPixelSize = 10;
constexpr int AvatarSize = 18; //!< circle diameter, matches the 9px radius
constexpr int AvatarOverlap = 6; //!< neighbouring avatars overlap by this much
constexpr qsizetype MaxVisibleAvatars = 3;
constexpr int MaxDescriptionLines = 2;

// The 1px QSS card border plus a small safety gap so elided text can never
// wrap back into an additional line inside the label.
constexpr int ElideWidthSlack = 4;

// Leading letters of the first two words of the name, uppercased.
QString avatarInitials(const QString &name)
{
    const auto words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString initials;
    if (!words.isEmpty()) {
        initials += words.first().left(1);
    }
    if (words.size() > 1) {
        initials += words.at(1).left(1);
    }
    return initials.toUpper();
}

// "Heute"/"Morgen" for those days, otherwise a short localized date.
QString dueChipText(const QDate &dueDate)
{
    const auto today = QDate::currentDate();
    if (dueDate == today) {
        return QStringLiteral("Heute");
    }
    if (dueDate == today.addDays(1)) {
        return QStringLiteral("Morgen");
    }
    return QLocale().toString(dueDate, QStringLiteral("d. MMM"));
}

// Word-wraps the text and returns it unchanged while it fits into two lines;
// otherwise the second line is shortened until it ends with an ellipsis.
QString elidedTwoLines(const QString &text, const QFont &font, const QFontMetrics &fm, int width)
{
    if (width <= 0) {
        return text;
    }

    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    QVector<int> lineStarts;
    layout.beginLayout();
    while (lineStarts.size() <= MaxDescriptionLines) {
        auto line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        lineStarts.append(line.textStart());
    }
    layout.endLayout();

    if (lineStarts.size() <= MaxDescriptionLines) {
        return text;
    }

    const auto firstLine = text.mid(lineStarts.at(0), lineStarts.at(1) - lineStarts.at(0));
    auto lastLine = text.mid(lineStarts.at(1), lineStarts.at(2) - lineStarts.at(1));
    while (!lastLine.isEmpty()
           && fm.horizontalAdvance(lastLine + QStringLiteral("\u2026")) > width) {
        lastLine.chop(1);
    }
    return firstLine + lastLine.trimmed() + QStringLiteral("\u2026");
}
}

DeckCardWidget::DeckCardWidget(const QJsonObject &cardData, int stackId, QWidget *parent)
    : QFrame(parent)
    , _cardData(cardData)
    , _cardId(cardData.value(QStringLiteral("id")).toInt())
    , _stackId(stackId)
{
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Metrics::SpacingM, Metrics::SpacingS, Metrics::SpacingM, Metrics::SpacingS);
    layout->setSpacing(Metrics::SpacingS);

    _titleLabel = new QLabel(this);
    _titleLabel->setWordWrap(false);
    layout->addWidget(_titleLabel);

    _labelsContainer = new QWidget(this);
    _labelsLayout = new QHBoxLayout(_labelsContainer);
    _labelsLayout->setContentsMargins(0, 0, 0, 0);
    _labelsLayout->setSpacing(4);
    _labelsLayout->setAlignment(Qt::AlignLeft);
    _labelsContainer->setVisible(false);
    layout->addWidget(_labelsContainer);

    _metaContainer = new QWidget(this);
    _metaLayout = new QHBoxLayout(_metaContainer);
    _metaLayout->setContentsMargins(0, 0, 0, 0);
    _metaLayout->setSpacing(Metrics::SpacingS);
    layout->addWidget(_metaContainer);

    // Round done toggle at the card bottom-left (rendered in renderMeta).

    // Hover quick actions (edit/delete), shown on enterEvent.
    _hoverActions = new QWidget(this);
    auto *hoverLayout = new QHBoxLayout(_hoverActions);
    hoverLayout->setContentsMargins(0, 0, 0, 0);
    hoverLayout->setSpacing(Metrics::SpacingXS);
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

    setDescription(cardData.value(QStringLiteral("description")).toString());
    setTitle(cardData.value(QStringLiteral("title")).toString());
    renderLabels();
    renderMeta();

    applyTheme();
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyTheme();
    });
}

void DeckCardWidget::setTitle(const QString &title)
{
    // "Done" is expressed via strikethrough + dimmed colors — a
    // QGraphicsOpacityEffect would force offscreen rendering and blur text.
    const auto isDone = _cardData.value(QStringLiteral("done")).toBool();
    _titleFull = title;
    QFont f = _titleLabel->font();
    f.setStrikeOut(isDone);
    _titleLabel->setFont(f);
    updateElides();
}

void DeckCardWidget::setDescription(const QString &description)
{
    // Qt wraps at spaces only — long slash-joined tokens (paths like
    // "Kommune/Position/Stadtentwässerung") would overflow and get cut
    // mid-glyph. Zero-width break opportunities make them wrap cleanly.
    _descriptionFull = description;
    _descriptionFull.replace(QLatin1Char('/'), QStringLiteral("/\u200B"));
    if (_descriptionFull.isEmpty()) {
        if (_descriptionLabel) {
            _descriptionLabel->setText(QString());
            _descriptionLabel->hide();
        }
        return;
    }
    if (!_descriptionLabel) {
        _descriptionLabel = new QLabel(this);
        _descriptionLabel->setWordWrap(true);
        // The snippet sits between title and label chips row.
        if (auto *box = qobject_cast<QVBoxLayout *>(layout())) {
            box->insertWidget(1, _descriptionLabel);
        }
    }
    _descriptionLabel->show();
    updateElides();
}

void DeckCardWidget::updateFrom(const QJsonObject &cardData)
{
    _cardData = cardData;
    _cardId = cardData.value(QStringLiteral("id")).toInt();
    setTitle(cardData.value(QStringLiteral("title")).toString());
    setDescription(cardData.value(QStringLiteral("description")).toString());
    renderLabels();
    renderMeta();
    applyTheme();
}

void DeckCardWidget::renderLabels()
{
    while (auto *item = _labelsLayout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const auto card = DeckCard::fromJson(_cardData);
    for (const auto &label : card.labels) {
        if (!label.color.isValid()) continue;

        auto *chip = new QLabel(label.title, _labelsContainer);
        // Dark text on bright label colors, light text on dark ones.
        const auto isBright = label.color.lightness() > 140;
        chip->setStyleSheet(QStringLiteral(
            "background-color: %1; color: %2; border-radius: 4px;"
            "font-size: 10px; font-weight: bold; padding: 1px 6px;")
            .arg(label.color.name(),
                 isBright ? QStringLiteral("#1c2430") : QStringLiteral("#ffffff")));
        _labelsLayout->addWidget(chip);
    }
    _labelsContainer->setVisible(!card.labels.isEmpty());
}

void DeckCardWidget::renderMeta()
{
    while (auto *item = _metaLayout->takeAt(0)) {
        // The done toggle persists across rebuilds, everything else is recreated.
        if (auto *w = item->widget(); w && w != _doneCircle) {
            w->deleteLater();
        }
        delete item;
    }
    _dueLabel = nullptr;
    _attachLabel = nullptr;
    _commentLabel = nullptr;
    _avatarContainer = nullptr;

    const auto card = DeckCard::fromJson(_cardData);

    // Round done toggle — click emits doneToggleRequested with the flipped state.
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
        doneBtn->setText(card.done ? QStringLiteral("\u2713") : QString());
    }
    _metaLayout->addWidget(_doneCircle);

    // Due pill — colored by styleMeta (overdue / due today / future).
    _dueDate = card.dueDate;
    if (_dueDate.isValid()) {
        _dueLabel = new QLabel(dueChipText(_dueDate.date()), _metaContainer);
        _dueLabel->setToolTip(QLocale().toString(_dueDate, QLocale::ShortFormat));
        _metaLayout->addWidget(_dueLabel);
    }

    // Assignee initials, overlapping, capped with a "+N" overflow circle.
    if (!card.assignees.isEmpty()) {
        _avatarContainer = new QWidget(_metaContainer);
        auto *avatarLayout = new QHBoxLayout(_avatarContainer);
        avatarLayout->setContentsMargins(0, 0, 0, 0);
        avatarLayout->setSpacing(-AvatarOverlap);
        const auto visibleCount = qMin(MaxVisibleAvatars, card.assignees.size());
        for (qsizetype i = 0; i < visibleCount; ++i) {
            const auto &user = card.assignees.at(i);
            const auto name = user.displayName.isEmpty() ? user.uid : user.displayName;
            auto *avatar = new QLabel(avatarInitials(name), _avatarContainer);
            avatar->setFixedSize(AvatarSize, AvatarSize);
            avatar->setAlignment(Qt::AlignCenter);
            avatar->setToolTip(name);
            avatarLayout->addWidget(avatar);
        }
        if (card.assignees.size() > MaxVisibleAvatars) {
            auto *more = new QLabel(
                QStringLiteral("+%1").arg(card.assignees.size() - MaxVisibleAvatars),
                _avatarContainer);
            more->setFixedSize(AvatarSize, AvatarSize);
            more->setAlignment(Qt::AlignCenter);
            more->setProperty("moreAvatar", true);
            avatarLayout->addWidget(more);
        }
        _metaLayout->addWidget(_avatarContainer);
    }

    _metaLayout->addStretch(1);

    // The list endpoints ship an empty attachments array plus a count field
    // (both "attachmentcount" and "attachmentCount" occur), so take the max.
    const auto jsonAttachmentCount = _cardData.value(QStringLiteral("attachmentcount"))
                                         .toInt(_cardData.value(QStringLiteral("attachmentCount")).toInt(0));
    const auto attachmentCount = qMax<qsizetype>(card.attachments.count(), jsonAttachmentCount);
    if (attachmentCount > 0) {
        _attachLabel = new QLabel(QStringLiteral("\U0001F4CE ") + QString::number(attachmentCount),
                                  _metaContainer);
        _metaLayout->addWidget(_attachLabel);
    }
    if (card.commentCount > 0) {
        _commentLabel = new QLabel(QStringLiteral("\U0001F4AC ") + QString::number(card.commentCount),
                                   _metaContainer);
        _metaLayout->addWidget(_commentLabel);
    }
}

void DeckCardWidget::styleMeta()
{
    const auto *theme = SouveraTheme::instance();

    if (_doneCircle) {
        if (auto *doneBtn = _doneCircle->property("doneButton").value<QToolButton *>()) {
            const auto isDone = _cardData.value(QStringLiteral("done")).toBool();
            doneBtn->setStyleSheet(QStringLiteral(
                "QToolButton { border: 2px solid %1; border-radius: 9px; background: %2;"
                "  color: #ffffff; font-weight: bold; font-size: %3px; }")
                .arg(theme->color(SouveraTheme::Color::Border).name(),
                     isDone ? theme->color(SouveraTheme::Color::Success).name()
                            : QStringLiteral("transparent"),
                     QString::number(BodyPixelSize)));
        }
    }

    if (_dueLabel) {
        // Overdue wins over "due today": a card due earlier today is overdue.
        const auto isDone = _cardData.value(QStringLiteral("done")).toBool();
        const auto overdue = !isDone && _dueDate < QDateTime::currentDateTime();
        const auto dueToday = _dueDate.date() == QDate::currentDate();
        const auto background = overdue ? theme->color(SouveraTheme::Color::Danger)
                             : dueToday ? theme->color(SouveraTheme::Color::Warning)
                                        : theme->color(SouveraTheme::Color::Surface);
        const auto foreground = overdue ? QStringLiteral("#ffffff")
                              : dueToday ? QStringLiteral("#1f2937")
                                         : theme->color(SouveraTheme::Color::TextSecondary).name();
        _dueLabel->setStyleSheet(QStringLiteral(
            "font-size: %3px; font-weight: 600; color: %1; background-color: %2;"
            "border-radius: 999px; padding: 1px 8px;")
            .arg(foreground, background.name(), QString::number(BodyPixelSize)));
    }

    if (_avatarContainer) {
        const auto makeAvatarStyle = [&theme](const QColor &textColor) {
            return QStringLiteral(
                       "background-color: %1; color: %2; border: 1px solid %3; border-radius: 9px;"
                       "font-size: %4px; font-weight: bold;")
                .arg(theme->color(SouveraTheme::Color::Surface).name(),
                     textColor.name(),
                     theme->color(SouveraTheme::Color::BorderStrong).name(),
                     QString::number(AvatarPixelSize));
        };
        const auto primaryStyle = makeAvatarStyle(theme->color(SouveraTheme::Color::TextPrimary));
        const auto mutedStyle = makeAvatarStyle(theme->color(SouveraTheme::Color::TextSecondary));
        for (auto *avatar : _avatarContainer->findChildren<QLabel *>()) {
            avatar->setStyleSheet(avatar->property("moreAvatar").toBool() ? mutedStyle : primaryStyle);
        }
    }

    const auto counterStyle = QStringLiteral(
        "font-size: %1px; color: %2; background: transparent;")
        .arg(QString::number(BodyPixelSize),
             theme->color(SouveraTheme::Color::TextMuted).name());
    if (_attachLabel) {
        _attachLabel->setStyleSheet(counterStyle);
    }
    if (_commentLabel) {
        _commentLabel->setStyleSheet(counterStyle);
    }
}

void DeckCardWidget::updateElides()
{
    const auto available = qMax(0, width() - 2 * Metrics::SpacingM - ElideWidthSlack);

    auto titleFont = _titleLabel->font();
    titleFont.setPixelSize(TitlePixelSize);
    titleFont.setWeight(QFont::DemiBold);
    const QFontMetrics titleMetrics(titleFont);
    _titleLabel->setText(available > 0
            ? titleMetrics.elidedText(_titleFull, Qt::ElideRight, available)
            : _titleFull);

    if (_descriptionLabel && !_descriptionFull.isEmpty()) {
        auto descriptionFont = _descriptionLabel->font();
        descriptionFont.setPixelSize(BodyPixelSize);
        const QFontMetrics descriptionMetrics(descriptionFont);
        _descriptionLabel->setText(
            elidedTwoLines(_descriptionFull, descriptionFont, descriptionMetrics, available));
    }
}

void DeckCardWidget::applyTheme()
{
    const auto *theme = SouveraTheme::instance();
    setStyleSheet(QStringLiteral(
        "DeckCardWidget { background-color: %1; border-radius: %5px; border: 1px solid %2; }"
        "DeckCardWidget:hover { border-color: %3; background-color: %4; }")
        .arg(theme->color(SouveraTheme::Color::Surface).name(),
             theme->color(SouveraTheme::Color::Border).name(),
             theme->color(SouveraTheme::Color::Accent).name(),
             theme->color(SouveraTheme::Color::SurfaceHover).name(),
             QString::number(Metrics::CardRadius)));

    _titleLabel->setStyleSheet(QStringLiteral(
        "font-weight: 600; font-size: %2px; color: %1; background: transparent;")
        .arg(theme->color(SouveraTheme::Color::TextPrimary).name(),
             QString::number(TitlePixelSize)));
    if (_descriptionLabel) {
        _descriptionLabel->setStyleSheet(QStringLiteral(
            "font-size: %2px; color: %1; background: transparent;")
            .arg(theme->color(SouveraTheme::Color::TextSecondary).name(),
                 QString::number(BodyPixelSize)));
    }
    styleMeta();
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

void DeckCardWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateElides();
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
