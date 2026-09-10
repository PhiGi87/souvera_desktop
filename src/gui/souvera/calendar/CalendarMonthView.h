/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALENDARMONTHVIEW_H
#define CALENDARMONTHVIEW_H

#include <QDate>
#include <QWidget>
#include <QVector>

namespace OCC {

struct CalendarEventEntry {
    QString uid;
    QString summary;
    QDateTime start;
    QDateTime end;
    bool allDay = false;
};

/**
 * @brief Thunderbird-style month grid with events painted inside the cells.
 */
class CalendarMonthView : public QWidget
{
    Q_OBJECT
public:
    explicit CalendarMonthView(QWidget *parent = nullptr);

    void setMonth(const QDate &month);
    [[nodiscard]] QDate month() const { return _month; }
    void setEvents(const QVector<CalendarEventEntry> &events);
    void setSelectedDate(const QDate &date);
    [[nodiscard]] QDate selectedDate() const { return _selectedDate; }

signals:
    void dateClicked(const QDate &date);
    void dateDoubleClicked(const QDate &date);
    void eventClicked(const QString &uid);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    [[nodiscard]] int dayCellIndex(const QPoint &pos) const;
    [[nodiscard]] QRect cellRect(int index) const;
    [[nodiscard]] QDate dateForIndex(int index) const;
    void rebuildLayout();

    QDate _month;       // any day within the displayed month
    QDate _selectedDate;
    QVector<CalendarEventEntry> _events;
    QVector<QList<CalendarEventEntry>> _eventsByCell;
    QDate _gridStart;
    int _cellCount = 0;
    int _hoverCell = -1;
};

} // namespace OCC

#endif // CALENDARMONTHVIEW_H
