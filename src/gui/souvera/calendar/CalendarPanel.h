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

class QTextEdit;

namespace OCC {

class CalDavSync;

class CalendarPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CalendarPanel(QWidget *parent = nullptr);
    ~CalendarPanel() override = default;

private:
    void setupUi();
    void onDateSelected(const QDate &date);

    QSplitter *_splitter = nullptr;
    QCalendarWidget *_calendar = nullptr;
    QListWidget *_eventList = nullptr;
    QTextEdit *_eventDetail = nullptr;

    CalDavSync *_calDavSync = nullptr;
};

} // namespace OCC

#endif // CALENDARPANEL_H
