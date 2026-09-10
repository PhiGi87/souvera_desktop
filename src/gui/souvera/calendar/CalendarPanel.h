/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALENDARPANEL_H
#define CALENDARPANEL_H

#include <QWidget>
#include <QDate>
#include <QListWidget>
#include <QSplitter>
#include <QPushButton>
#include <QVariantList>

class CalendarMonthView;

namespace OCC {

class CalDavSync;
class AccountState;

class CalendarPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CalendarPanel(QWidget *parent = nullptr);
    ~CalendarPanel() override = default;

    void setAccountState(AccountState *state);

private:
    void setupUi();
    void onMonthChanged(int months);
    void onGoToday();
    void onDateSelected(const QDate &date);
    void onNewEvent();
    void onCalendarsLoaded(const QVariantList &calendars);
    void onEventsLoaded(const QVariantList &events);
    void onEventCreated(bool success);

    QSplitter *_splitter = nullptr;
    CalendarMonthView *_monthView = nullptr;
    QPushButton *_todayBtn = nullptr;
    QDate _selectedDate;
    QListWidget *_eventList = nullptr;
    QPushButton *_newEventBtn = nullptr;

    CalDavSync *_calDavSync = nullptr;
    QString _currentCalendarUri;
};

} // namespace OCC

#endif // CALENDARPANEL_H
