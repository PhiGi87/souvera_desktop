/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckTodayView.h"
#include "DeckManager.h"
#include "DeckModels.h"
#include "theme/SouveraMetrics.h"
#include "theme/SouveraTheme.h"

#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QLoggingCategory>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace OCC {

namespace Metrics = Sou::Metrics;

Q_LOGGING_CATEGORY(lcDeckToday, "souvera.deck.todayview")

DeckTodayView::DeckTodayView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setObjectName(QStringLiteral("PanelScroll"));
    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    _content = new QWidget(_scrollArea);
    _contentLayout = new QVBoxLayout(_content);
    _contentLayout->setContentsMargins(Metrics::CardMargin, Metrics::CardMargin,
        Metrics::CardMargin, Metrics::CardMargin);
    _contentLayout->setSpacing(Metrics::SpacingS);

    _summaryLabel = new QLabel(_content);
    _summaryLabel->hide();
    _contentLayout->addWidget(_summaryLabel);

    _placeholderLabel = new QLabel(_content);
    _placeholderLabel->setObjectName(QStringLiteral("PanelPlaceholder"));
    _placeholderLabel->hide();
    _contentLayout->addWidget(_placeholderLabel);

    // Sections are inserted before this stretch so they stay top-aligned.
    _contentLayout->addStretch(1);

    _scrollArea->setWidget(_content);
    layout->addWidget(_scrollArea);

    applyTheme();
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyTheme();
        rebuildFromCache();
    });
}

DeckTodayView::~DeckTodayView() = default;

void DeckTodayView::setBoards(const QJsonArray &boards)
{
    _boards = boards;
    rebuildFromCache();
}

void DeckTodayView::setCurrentUser(const QString &uid)
{
    _currentUserUid = uid;
    rebuildFromCache();
}

void DeckTodayView::setSearchText(const QString &text)
{
    _searchText = text;
    rebuildFromCache();
}

void DeckTodayView::rebuildFromCache()
{
    clearSections();

    auto *manager = DeckManager::instance();
    const auto today = QDate::currentDate();

    QVector<QJsonObject> overdue;
    QVector<QJsonObject> dueToday;
    QVector<QJsonObject> dueThisWeek;
    QVector<QJsonObject> assignedToMe;
    QSet<int> seenCardIds;
    int sectionTotal = 0;

    qCDebug(lcDeckToday) << "Rebuilding today view from stacks cache for boards:" << _boards.size();

    for (const auto &boardVal : _boards) {
        const auto board = DeckBoard::fromJson(boardVal.toObject());
        if (board.archived) continue;
        const auto stacks = manager->cachedStacks(board.id);
        for (const auto &stackVal : stacks) {
            const auto stack = DeckStack::fromJson(stackVal.toObject());
            if (stack.archived) continue;
            for (const auto &card : stack.cards) {
                if (card.done || card.archived) continue;
                if (seenCardIds.contains(card.id)) continue;
                seenCardIds.insert(card.id);

                const auto entry = makeEntry(card, board.id, board.title, stack.id, stack.title);
                const auto due = card.dueDate.date();

                // Exactly one section per card — first match wins, so the
                // remaining branches know the earlier ones did not match.
                if (due.isValid() && due < today) {
                    overdue.append(entry);
                } else if (due.isValid() && due == today) {
                    dueToday.append(entry);
                } else if (due.isValid() && due <= today.addDays(7)) {
                    dueThisWeek.append(entry);
                } else if (isAssignedToCurrentUser(card)) {
                    assignedToMe.append(entry);
                } else {
                    continue;
                }
                ++sectionTotal;
            }
        }
    }

    const auto sortByDueDate = [](const QJsonObject &a, const QJsonObject &b) {
        const auto aMs = a.value(QStringLiteral("dueMs")).toDouble(-1.0);
        const auto bMs = b.value(QStringLiteral("dueMs")).toDouble(-1.0);
        if (aMs < 0 || bMs < 0) return aMs >= 0 && bMs < 0; // cards without a due date go last
        return aMs < bMs;
    };
    const auto matchesSearch = [this](const QJsonObject &entry) {
        if (_searchText.isEmpty()) return true;
        const auto card = entry.value(QStringLiteral("card")).toObject();
        return card.value(QStringLiteral("title")).toString().contains(_searchText, Qt::CaseInsensitive)
            || card.value(QStringLiteral("description")).toString().contains(_searchText, Qt::CaseInsensitive);
    };
    const auto applyFilterAndSort = [&matchesSearch, &sortByDueDate](QVector<QJsonObject> &entries) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [&matchesSearch](const QJsonObject &entry) { return !matchesSearch(entry); }),
            entries.end());
        std::sort(entries.begin(), entries.end(), sortByDueDate);
    };
    applyFilterAndSort(overdue);
    applyFilterAndSort(dueToday);
    applyFilterAndSort(dueThisWeek);
    applyFilterAndSort(assignedToMe);

    const auto addSection = [this](QWidget *section) {
        _contentLayout->insertWidget(2 + _sectionWidgets.size(), section);
        _sectionWidgets.append(section);
    };
    if (!overdue.isEmpty()) {
        addSection(buildSection(QStringLiteral("\u00DCberf\u00E4llig"),
            SouveraTheme::instance()->color(SouveraTheme::Color::Danger), overdue, DueChip::Overdue));
    }
    if (!dueToday.isEmpty()) {
        addSection(buildSection(QStringLiteral("Heute"),
            SouveraTheme::instance()->color(SouveraTheme::Color::Warning), dueToday, DueChip::Today));
    }
    if (!dueThisWeek.isEmpty()) {
        addSection(buildSection(QStringLiteral("Diese Woche"),
            SouveraTheme::instance()->color(SouveraTheme::Color::TextSecondary), dueThisWeek, DueChip::Soon));
    }
    if (!assignedToMe.isEmpty()) {
        addSection(buildSection(QStringLiteral("Mir zugewiesen"),
            SouveraTheme::instance()->color(SouveraTheme::Color::TextSecondary), assignedToMe, DueChip::Calm));
    }

    const auto visibleTotal = overdue.size() + dueToday.size() + dueThisWeek.size() + assignedToMe.size();
    if (visibleTotal == 0) {
        _summaryLabel->hide();
        _placeholderLabel->setText(sectionTotal == 0
            ? QStringLiteral("Keine offenen Karten \u2014 alles erledigt.")
            : QStringLiteral("Keine Treffer."));
        _placeholderLabel->show();
    } else {
        _summaryLabel->setText(QStringLiteral("%1 \u00FCberf\u00E4llig \u00B7 %2 heute \u00B7 %3 diese Woche")
            .arg(overdue.size()).arg(dueToday.size()).arg(dueThisWeek.size()));
        _summaryLabel->show();
        _placeholderLabel->hide();
    }

    qCDebug(lcDeckToday) << "Today view rebuilt:" << overdue.size() << "overdue" << dueToday.size()
        << "today" << dueThisWeek.size() << "this week" << assignedToMe.size() << "assigned";
}

void DeckTodayView::applyTheme()
{
    const auto *theme = SouveraTheme::instance();
    _summaryLabel->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: %1; background: transparent;")
        .arg(theme->color(SouveraTheme::Color::TextSecondary).name()));
    _placeholderLabel->setStyleSheet(QStringLiteral(
        "font-size: 15px; color: %1; background: transparent; padding: %2px 0;")
        .arg(theme->color(SouveraTheme::Color::TextMuted).name(),
             QString::number(Metrics::SpacingL)));
}

void DeckTodayView::clearSections()
{
    for (auto *section : std::as_const(_sectionWidgets)) {
        _contentLayout->removeWidget(section);
        section->deleteLater();
    }
    _sectionWidgets.clear();
}

bool DeckTodayView::isAssignedToCurrentUser(const DeckCard &card) const
{
    if (_currentUserUid.isEmpty()) return false;
    return std::any_of(card.assignees.cbegin(), card.assignees.cend(),
        [this](const DeckUser &assignee) {
            return assignee.uid.compare(_currentUserUid, Qt::CaseInsensitive) == 0;
        });
}

QJsonObject DeckTodayView::makeEntry(const DeckCard &card, int boardId,
    const QString &boardTitle, int stackId, const QString &stackTitle) const
{
    // The card keeps its raw cached JSON untouched — that exact object is
    // what the signals must hand back to the panel.
    QJsonObject entry;
    entry.insert(QStringLiteral("card"), card.raw);
    entry.insert(QStringLiteral("boardId"), boardId);
    entry.insert(QStringLiteral("boardTitle"), boardTitle);
    entry.insert(QStringLiteral("stackId"), stackId);
    entry.insert(QStringLiteral("stackTitle"), stackTitle);
    entry.insert(QStringLiteral("dueMs"),
        card.dueDate.isValid() ? double(card.dueDate.toMSecsSinceEpoch()) : -1.0);
    return entry;
}

QWidget *DeckTodayView::buildSection(const QString &title, const QColor &headerColor,
    const QVector<QJsonObject> &entries, DueChip chipKind)
{
    auto *section = new QWidget(_content);
    auto *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(Metrics::SpacingS);

    auto *header = new QLabel(title, section);
    header->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    header->setStyleSheet(QStringLiteral(
        "font-size: 13px; font-weight: 700; color: %1; background: transparent;")
        .arg(headerColor.name()));
    sectionLayout->addWidget(header);

    for (const auto &entry : entries) {
        sectionLayout->addWidget(buildRow(entry, chipKind));
    }
    return section;
}

QWidget *DeckTodayView::buildRow(const QJsonObject &entry, DueChip chipKind)
{
    const auto card = entry.value(QStringLiteral("card")).toObject();
    const auto boardId = entry.value(QStringLiteral("boardId")).toInt();
    const auto stackId = entry.value(QStringLiteral("stackId")).toInt();
    const auto cardId = card.value(QStringLiteral("id")).toInt(-1);
    const auto *theme = SouveraTheme::instance();

    auto *row = new QWidget(_content);
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setObjectName(QStringLiteral("TodayCardRow"));
    row->setCursor(Qt::PointingHandCursor);
    row->setProperty("todayBoardId", boardId);
    row->setProperty("todayStackId", stackId);
    row->setProperty("todayCardId", cardId);
    row->setProperty("todayCardJson", card);
    row->installEventFilter(this);
    row->setStyleSheet(QStringLiteral(
        "QWidget#TodayCardRow { background-color: %1; border-radius: 8px; border: 1px solid %2; }"
        "QWidget#TodayCardRow:hover { background-color: %3; }")
        .arg(theme->color(SouveraTheme::Color::Surface).name(),
             theme->color(SouveraTheme::Color::Border).name(),
             theme->color(SouveraTheme::Color::SurfaceHover).name()));

    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(Metrics::SpacingS, Metrics::SpacingS,
        Metrics::SpacingS, Metrics::SpacingS);
    rowLayout->setSpacing(Metrics::SpacingS);

    // Done toggle — flips "done" on the complete cached card object; the row
    // itself drops out of the next rebuild once the toggle has landed.
    auto *doneBtn = new QToolButton(row);
    doneBtn->setFixedSize(16, 16);
    doneBtn->setCursor(Qt::PointingHandCursor);
    doneBtn->setToolTip(QStringLiteral("Als erledigt markieren"));
    doneBtn->setStyleSheet(QStringLiteral(
        "QToolButton { border: 2px solid %1; border-radius: 8px; background: transparent; }")
        .arg(theme->color(SouveraTheme::Color::Border).name()));
    connect(doneBtn, &QToolButton::clicked, this, [this, boardId, stackId, cardId, card]() {
        auto flipped = card;
        flipped.insert(QStringLiteral("done"), !card.value(QStringLiteral("done")).toBool());
        qCDebug(lcDeckToday) << "Done toggle requested for card:" << cardId;
        emit doneToggleRequested(boardId, stackId, cardId, flipped);
    });
    rowLayout->addWidget(doneBtn);

    auto *textCol = new QWidget(row);
    textCol->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto *textLayout = new QVBoxLayout(textCol);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);
    textLayout->addWidget(makeElidedLabel(card.value(QStringLiteral("title")).toString(),
        QStringLiteral("font-size: 13px; font-weight: 600; color: %1; background: transparent;")
            .arg(theme->color(SouveraTheme::Color::TextPrimary).name()), textCol));
    textLayout->addWidget(makeElidedLabel(QStringLiteral("%1 \u00B7 %2")
        .arg(entry.value(QStringLiteral("boardTitle")).toString(),
             entry.value(QStringLiteral("stackTitle")).toString()),
        QStringLiteral("font-size: 11px; color: %1; background: transparent;")
            .arg(theme->color(SouveraTheme::Color::TextMuted).name()), textCol));
    rowLayout->addWidget(textCol, 1);

    auto *metaRow = new QWidget(row);
    metaRow->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto *metaLayout = new QHBoxLayout(metaRow);
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->setSpacing(Metrics::SpacingXS);

    // Due chip styling follows the section the card landed in; cards in
    // "Mir zugewiesen" without any due date get no chip at all.
    const auto dueMs = entry.value(QStringLiteral("dueMs")).toDouble(-1.0);
    const auto dueDate = dueMs >= 0
        ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(dueMs)).date()
        : QDate();
    const auto locale = QLocale(QLocale::German);
    auto chipText = QString();
    auto chipBg = QColor();
    auto chipFg = QColor();
    switch (chipKind) {
    case DueChip::Overdue:
        chipText = locale.toString(dueDate, QStringLiteral("d. MMM"));
        chipBg = theme->color(SouveraTheme::Color::Danger);
        // White matches the overdue due chip of DeckCardWidget — the theme's
        // OnAccent is dark navy and has poor contrast on the red background.
        chipFg = QColor(QStringLiteral("#ffffff"));
        break;
    case DueChip::Today:
        chipText = QStringLiteral("Heute");
        chipBg = theme->color(SouveraTheme::Color::Warning);
        chipFg = QColor(QStringLiteral("#1f2937"));
        break;
    case DueChip::Soon:
        chipText = dueDate == QDate::currentDate().addDays(1)
            ? QStringLiteral("Morgen")
            : locale.toString(dueDate, QStringLiteral("d. MMM"));
        chipBg = theme->color(SouveraTheme::Color::Surface);
        chipFg = theme->color(SouveraTheme::Color::TextSecondary);
        break;
    case DueChip::Calm:
        if (dueDate.isValid()) {
            chipText = locale.toString(dueDate, QStringLiteral("d. MMM"));
            chipBg = theme->color(SouveraTheme::Color::Surface);
            chipFg = theme->color(SouveraTheme::Color::TextSecondary);
        }
        break;
    }
    if (!chipText.isEmpty()) {
        metaLayout->addWidget(makeChip(chipText, chipBg, chipFg, metaRow));
    }

    // Label chips — same luminance rule as DeckCardWidget::renderLabels.
    const auto labels = card.value(QStringLiteral("labels")).toArray();
    for (const auto &labelVal : labels) {
        const auto labelObj = labelVal.toObject();
        const auto labelColor = QColor(labelObj.value(QStringLiteral("color")).toString());
        const auto labelTitle = labelObj.value(QStringLiteral("title")).toString();
        if (!labelColor.isValid() || labelTitle.isEmpty()) continue;
        const auto isBright = labelColor.lightness() > 140;
        metaLayout->addWidget(makeChip(labelTitle, labelColor,
            QColor(isBright ? QStringLiteral("#1c2430") : QStringLiteral("#ffffff")), metaRow));
    }
    rowLayout->addWidget(metaRow);

    return row;
}

QLabel *DeckTodayView::makeElidedLabel(const QString &text, const QString &styleSheet,
    QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    label->setStyleSheet(styleSheet);
    label->setProperty("todayFullTitle", text);
    // The layout must not shrink the label to the (elided) text width, or
    // re-eliding on resize would oscillate.
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    label->installEventFilter(this);
    return label;
}

QLabel *DeckTodayView::makeChip(const QString &text, const QColor &background,
    const QColor &foreground, QWidget *parent) const
{
    auto *chip = new QLabel(text, parent);
    chip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    chip->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; border-radius: 9px; font-size: 11px;"
        "font-weight: bold; padding: 2px 8px;")
        .arg(background.name(), foreground.name()));
    return chip;
}

void DeckTodayView::emitCardActivated(QWidget *row)
{
    const auto boardId = row->property("todayBoardId").toInt();
    const auto stackId = row->property("todayStackId").toInt();
    const auto cardId = row->property("todayCardId").toInt();
    const auto cardJson = row->property("todayCardJson").value<QJsonObject>();
    qCDebug(lcDeckToday) << "Card activated from today view:" << cardId;
    emit cardActivated(boardId, stackId, cardId, cardJson);
}

bool DeckTodayView::eventFilter(QObject *watched, QEvent *event)
{
    // Title/context labels re-elide whenever the layout hands them a new width.
    if (event->type() == QEvent::Resize) {
        auto *label = qobject_cast<QLabel *>(watched);
        if (label && label->property("todayFullTitle").isValid()) {
            const auto width = static_cast<QResizeEvent *>(event)->size().width();
            label->setText(label->fontMetrics().elidedText(
                label->property("todayFullTitle").toString(), Qt::ElideRight, qMax(width, 0)));
        }
        return QWidget::eventFilter(watched, event);
    }

    auto *row = qobject_cast<QWidget *>(watched);
    if (!row || !row->property("todayCardJson").isValid()) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress:
        // Anchors the implicit mouse grab on the row so the release comes
        // back here even if the cursor wanders.
        return true;
    case QEvent::MouseButtonDblClick:
        // The first release already activated the card; swallow the double
        // click and its trailing release so it is not activated twice.
        _suppressNextClickRelease = true;
        return true;
    case QEvent::MouseButtonRelease:
        if (_suppressNextClickRelease) {
            _suppressNextClickRelease = false;
        } else {
            emitCardActivated(row);
        }
        return true;
    default:
        return QWidget::eventFilter(watched, event);
    }
}

} // namespace OCC
