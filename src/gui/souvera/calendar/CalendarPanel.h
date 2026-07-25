/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALENDARPANEL_H
#define CALENDARPANEL_H

#include <QWidget>
#include <QCalendarWidget>
#include <QListWidget>
#include <QSplitter>
#include <QPushButton>
#include <QVariantList>

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
    void onDateSelected(const QDate &date);
    void onNewEvent();
    void onCalendarsLoaded(const QVariantList &calendars);
    void onEventsLoaded(const QVariantList &events);
    void onEventCreated(bool success);

    QSplitter *_splitter = nullptr;
    QCalendarWidget *_calendar = nullptr;
    QListWidget *_eventList = nullptr;
    QPushButton *_newEventBtn = nullptr;

    CalDavSync *_calDavSync = nullptr;
    QString _currentCalendarUri;
};

} // namespace OCC

#endif // CALENDARPANEL_H
