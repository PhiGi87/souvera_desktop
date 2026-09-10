/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CalendarMonthView.h"
#include "theme/SouveraTheme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace OCC {

namespace {
constexpr int HeaderRow = 28;
constexpr int MaxEventsPerCell = 3;
constexpr int WeekdayCount = 7;
constexpr int DayNames[7] = {2, 3, 4, 5, 6, 7, 1}; // Mon..Sun (Qt::Monday = 1)
}

CalendarMonthView::CalendarMonthView(QWidget *parent)
    : QWidget(parent)
{
    _month = QDate::currentDate();
    _selectedDate = QDate::currentDate();
    setMouseTracking(true);
    setMinimumSize(560, 360);
}

void CalendarMonthView::setMonth(const QDate &month)
{
    if (!month.isValid()) return;
    _month = QDate(month.year(), month.month(), 1);
    rebuildLayout();
    update();
}

void CalendarMonthView::setSelectedDate(const QDate &date)
{
    if (date.isValid()) {
        _selectedDate = date;
        update();
    }
}

void CalendarMonthView::setEvents(const QVector<CalendarEventEntry> &events)
{
    _events = events;
    _eventsByCell.assign(_cellCount, {});

    for (const auto &event : _events) {
        if (!event.start.isValid()) continue;
        const auto days = qMax(1, static_cast<int>(event.start.date().daysTo(event.end.isValid() ? event.end.date() : event.start.date())) + 1);
        auto cursor = event.start.date();
        for (auto d = 0; d < qMin(days, 31); ++d) {
            const auto dayNumber = static_cast<int>(_gridStart.daysTo(cursor));
            if (cursor >= _gridStart && dayNumber >= 0 && dayNumber < _cellCount) {
                _eventsByCell[dayNumber].append(event);
            }
            cursor = cursor.addDays(1);
        }
    }
    update();
}

void CalendarMonthView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto *theme = SouveraTheme::instance();
    const auto background = theme->color(SouveraTheme::Color::ContentBackground);
    const auto surface = theme->color(SouveraTheme::Color::Surface);
    const auto border = theme->color(SouveraTheme::Color::Border);
    const auto textPrimary = theme->color(SouveraTheme::Color::TextPrimary);
    const auto textMuted = theme->color(SouveraTheme::Color::TextMuted);
    const auto accent = theme->color(SouveraTheme::Color::Accent);

    painter.fillRect(rect(), background);

    // Weekday header
    painter.setFont(font());
    const auto dayNames = QStringList{QStringLiteral("Mo"), QStringLiteral("Di"),
        QStringLiteral("Mi"), QStringLiteral("Do"), QStringLiteral("Fr"),
        QStringLiteral("Sa"), QStringLiteral("So")};
    for (auto col = 0; col < WeekdayCount; ++col) {
        const auto r = QRect(col * width() / WeekdayCount, 0, width() / WeekdayCount, HeaderRow);
        painter.setPen(textMuted);
        painter.drawText(r, Qt::AlignCenter, dayNames.at(col));
    }

    const auto today = QDate::currentDate();
    const auto weeks = (_cellCount + WeekdayCount - 1) / WeekdayCount;
    const auto cellH = (height() - HeaderRow) / qMax(1, weeks);
    const auto cellW = width() / WeekdayCount;

    for (auto i = 0; i < _cellCount; ++i) {
        const auto cell = cellRect(i);
        const auto date = dateForIndex(i);
        if (!date.isValid()) continue;

        const auto inMonth = date.month() == _month.month();
        const auto isToday = date == today;
        const auto isSelected = date == _selectedDate;

        // Cell background
        if (isToday) {
            auto todayBg = accent;
            todayBg.setAlpha(28);
            painter.fillRect(cell, todayBg);
        } else if (!inMonth) {
            painter.fillRect(cell, background.darker(105));
        }

        // Selected outline
        if (isSelected) {
            painter.setPen(QPen(accent, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(cell.adjusted(1, 1, -2, -2), 4, 4);
        }

        // Cell border
        painter.setPen(border);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(cell);

        // Day number
        auto dayNumFont = font();
        dayNumFont.setBold(isToday);
        painter.setFont(dayNumFont);
        painter.setPen(isToday ? accent : (inMonth ? textPrimary : textMuted));
        painter.drawText(cell.adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft,
                         QString::number(date.day()));

        // Events
        if (i < _eventsByCell.size()) {
            const auto &dayEvents = _eventsByCell.at(i);
            auto eventY = cell.top() + 24;
            const auto eventH = 18;
            const auto maxEvents = qMin(dayEvents.size(), qMax(1, (cellH - 30) / eventH));

            auto eventFont = font();
            eventFont.setPointSizeF(font().pointSizeF() * 0.85);
            painter.setFont(eventFont);

            for (auto e = 0; e < maxEvents; ++e) {
                const auto &ev = dayEvents.at(e);
                const auto eventRect = QRect(cell.left() + 4, eventY, cell.width() - 8, eventH - 2);

                auto eventBg = accent;
                eventBg.setAlpha(60);
                painter.setPen(Qt::NoPen);
                painter.setBrush(eventBg);
                painter.drawRoundedRect(eventRect, 3, 3);

                painter.setPen(textPrimary);
                painter.setClipRect(eventRect);
                painter.drawText(eventRect.adjusted(5, 0, -3, 0),
                                 Qt::AlignVCenter | Qt::AlignLeft, ev.summary);
                painter.setClipping(false);

                eventY += eventH;
            }
            if (dayEvents.size() > maxEvents) {
                painter.setPen(textMuted);
                painter.drawText(QPoint(cell.left() + 4, eventY + eventH),
                                 QStringLiteral("+%1 mehr").arg(dayEvents.size() - maxEvents));
            }
        }
    }
}

int CalendarMonthView::dayCellIndex(const QPoint &pos) const
{
    if (pos.y() < HeaderRow) return -1;
    for (auto i = 0; i < _cellCount; ++i) {
        if (cellRect(i).contains(pos)) return i;
    }
    return -1;
}

QRect CalendarMonthView::cellRect(int index) const
{
    const auto weeks = (_cellCount + WeekdayCount - 1) / WeekdayCount;
    const auto cellH = (height() - HeaderRow) / qMax(1, weeks);
    const auto cellW = width() / WeekdayCount;
    return QRect((index % WeekdayCount) * cellW, HeaderRow + (index / WeekdayCount) * cellH,
                 cellW, cellH);
}

QDate CalendarMonthView::dateForIndex(int index) const
{
    if (index < 0 || index >= _cellCount) return {};
    return _gridStart.addDays(index);
}

void CalendarMonthView::rebuildLayout()
{
    const auto firstOfMonth = QDate(_month.year(), _month.month(), 1);
    const auto dayOfWeek = firstOfMonth.dayOfWeek();
    _gridStart = firstOfMonth.addDays(-(dayOfWeek - 1));

    QDate lastOfMonth(_month.year(), _month.month(), _month.daysInMonth());
    const auto trailing = WeekdayCount - lastOfMonth.dayOfWeek();
    const auto totalDays = static_cast<int>(_gridStart.daysTo(lastOfMonth)) + 1 + trailing;
    _cellCount = ((totalDays + WeekdayCount - 1) / WeekdayCount) * WeekdayCount;

    setEvents(_events);
}

void CalendarMonthView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    update(); // cells are computed from current size in paintEvent
}

void CalendarMonthView::mousePressEvent(QMouseEvent *event)
{
    const auto cell = dayCellIndex(event->pos());
    if (cell < 0) return;
    _selectedDate = dateForIndex(cell);
    update();
    emit dateClicked(_selectedDate);
}

void CalendarMonthView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const auto cell = dayCellIndex(event->pos());
    if (cell < 0) return;
    _selectedDate = dateForIndex(cell);
    emit dateDoubleClicked(_selectedDate);
}

} // namespace OCC
