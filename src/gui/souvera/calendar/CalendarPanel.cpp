/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CalendarPanel.h"
#include "CalendarMonthView.h"
#include "CalDavSync.h"
#include "CalendarEventDialog.h"

#include "accountstate.h"
#include "theme/SouveraTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QDate>

Q_LOGGING_CATEGORY(lcCalendarPanel, "souvera.calendar.panel")

namespace OCC {

CalendarPanel::CalendarPanel(QWidget *parent)
    : QWidget(parent)
{
    _calDavSync = new CalDavSync(this);
    setupUi();

    connect(_calDavSync, &CalDavSync::calendarsLoaded, this, &CalendarPanel::onCalendarsLoaded);
    connect(_calDavSync, &CalDavSync::eventsLoaded, this, &CalendarPanel::onEventsLoaded);
    connect(_calDavSync, &CalDavSync::eventCreated, this, &CalendarPanel::onEventCreated);
    connect(_calDavSync, &CalDavSync::errorOccurred, this, [this](const QString &msg) {
        qCWarning(lcCalendarPanel) << "CalDAV error:" << msg;
    });
}

void CalendarPanel::setAccountState(AccountState *state)
{
    _calDavSync->setAccountState(state);
    if (state && state->account()) {
        _calDavSync->fetchCalendars();
    }
}

void CalendarPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 8, 16, 8);

    auto *title = new QLabel(QStringLiteral("Kalender"), toolbar);
    title->setObjectName(QStringLiteral("PanelTitle"));
    toolbarLayout->addWidget(title);

    toolbarLayout->addSpacing(12);

    auto *prevBtn = new QPushButton(QStringLiteral("\u2039"), toolbar);
    prevBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    prevBtn->setFixedWidth(36);
    connect(prevBtn, &QPushButton::clicked, this, [this]() { onMonthChanged(-1); });
    toolbarLayout->addWidget(prevBtn);

    _todayBtn = new QPushButton(QStringLiteral("Heute"), toolbar);
    _todayBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(_todayBtn, &QPushButton::clicked, this, &CalendarPanel::onGoToday);
    toolbarLayout->addWidget(_todayBtn);

    auto *nextBtn = new QPushButton(QStringLiteral("\u203A"), toolbar);
    nextBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    nextBtn->setFixedWidth(36);
    connect(nextBtn, &QPushButton::clicked, this, [this]() { onMonthChanged(1); });
    toolbarLayout->addWidget(nextBtn);

    toolbarLayout->addStretch();

    _newEventBtn = new QPushButton(QStringLiteral("+ Neuer Termin"), toolbar);
    _newEventBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(_newEventBtn, &QPushButton::clicked, this, &CalendarPanel::onNewEvent);
    toolbarLayout->addWidget(_newEventBtn);

    layout->addWidget(toolbar);

    _splitter = new QSplitter(Qt::Vertical, this);

    _monthView = new CalendarMonthView(_splitter);
    connect(_monthView, &CalendarMonthView::dateClicked, this, &CalendarPanel::onDateSelected);
    connect(_monthView, &CalendarMonthView::dateDoubleClicked, this, [this](const QDate &date) {
        _selectedDate = date;
        onNewEvent();
    });
    _splitter->addWidget(_monthView);

    _eventList = new QListWidget(_splitter);
    _eventList->setObjectName(QStringLiteral("RemoteFilesView"));
    _splitter->addWidget(_eventList);

    _splitter->setStretchFactor(0, 3);
    _splitter->setStretchFactor(1, 1);

    layout->addWidget(_splitter, 1);
}

void CalendarPanel::onMonthChanged(int months)
{
    if (!_monthView) return;
    const auto current = _monthView->month();
    _monthView->setMonth(current.addMonths(months));
    // Reload events for the new month (whole month range)
    if (!_currentCalendarUri.isEmpty()) {
        const auto from = QDate(current.year(), current.month(), 1).addMonths(months);
        const auto to = QDate(from.year(), from.month(), from.daysInMonth());
        _calDavSync->fetchEvents(_currentCalendarUri, from.addDays(-7), to.addDays(7));
    }
}

void CalendarPanel::onGoToday()
{
    if (!_monthView) return;
    const auto today = QDate::currentDate();
    _monthView->setMonth(today);
    _monthView->setSelectedDate(today);
    onDateSelected(today);
}

void CalendarPanel::onDateSelected(const QDate &date)
{
    _eventList->clear();
    qCInfo(lcCalendarPanel) << "Date selected:" << date;
    if (!_currentCalendarUri.isEmpty()) {
        _calDavSync->fetchEvents(_currentCalendarUri, date.addDays(-1), date.addDays(1));
    }
}

void CalendarPanel::onNewEvent()
{
    if (_currentCalendarUri.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Kalender"),
            QStringLiteral("Kein Kalender verfügbar. Bitte zuerst einen Kalender laden."));
        return;
    }

    auto *dialog = new CalendarEventDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        auto iCalData = dialog->getICalendar();
        _calDavSync->createEvent(_currentCalendarUri, iCalData);
    }
    dialog->deleteLater();
}

void CalendarPanel::onCalendarsLoaded(const QVariantList &calendars)
{
    qCInfo(lcCalendarPanel) << "Calendars loaded:" << calendars.size();
    if (calendars.isEmpty()) return;

    auto first = calendars.first().toMap();
    auto href = first.value(QStringLiteral("href")).toString();
    auto trimmed = href;
    if (trimmed.endsWith(QLatin1Char('/'))) trimmed.chop(1);
    auto lastSlash = trimmed.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0) {
        _currentCalendarUri = trimmed.mid(lastSlash + 1);
    } else {
        _currentCalendarUri = trimmed;
    }

    qCInfo(lcCalendarPanel) << "Using calendar:" << _currentCalendarUri
                           << first.value(QStringLiteral("displayname")).toString();

    auto today = QDate::currentDate();
    if (_monthView) {
        _monthView->setMonth(today);
        _monthView->setSelectedDate(today);
    }
    _calDavSync->fetchEvents(_currentCalendarUri, today, today);
}

void CalendarPanel::onEventsLoaded(const QVariantList &events)
{
    _eventList->clear();
    qCInfo(lcCalendarPanel) << "Events loaded:" << events.size();

    if (_monthView) {
        QVector<CalendarEventEntry> entries;
        for (const auto &evVal : events) {
            const auto ev = evVal.toMap();
            CalendarEventEntry entry;
            entry.uid = ev.value(QStringLiteral("uid")).toString();
            entry.summary = ev.value(QStringLiteral("summary")).toString();
            const auto dtStartStr = ev.value(QStringLiteral("dtstart")).toString();
            const auto dtEndStr = ev.value(QStringLiteral("dtend")).toString();
            entry.start = QDateTime::fromString(dtStartStr, QStringLiteral("yyyyMMdd'T'HHmmss"));
            entry.end = QDateTime::fromString(dtEndStr, QStringLiteral("yyyyMMdd'T'HHmmss"));
            entry.allDay = !dtStartStr.contains(QLatin1Char('T'));
            if (!entry.start.isValid() && dtStartStr.size() >= 8) {
                entry.start = QDateTime(QDate::fromString(dtStartStr.left(8), QStringLiteral("yyyyMMdd")),
                                        QTime(0, 0));
            }
            if (entry.summary.isEmpty()) entry.summary = QStringLiteral("(Unbenannter Termin)");
            entries.append(entry);
        }
        _monthView->setEvents(entries);
    }

    if (events.isEmpty()) {
        _eventList->addItem(QStringLiteral("(Keine Termine)"));
        return;
    }

    for (const auto &evVal : events) {
        auto ev = evVal.toMap();
        auto summary = ev.value(QStringLiteral("summary")).toString();
        auto dtstart = ev.value(QStringLiteral("dtstart")).toString();
        auto dtend = ev.value(QStringLiteral("dtend")).toString();

        auto timeStr = QString();
        if (!dtstart.isEmpty()) {
            auto tPos = dtstart.indexOf(QLatin1Char('T'));
            if (tPos >= 0) {
                auto timePart = dtstart.mid(tPos + 1, 6);
                if (timePart.length() == 6) {
                    timeStr = timePart.left(2) + QStringLiteral(":") + timePart.mid(2, 2);
                }
            }
            if (!dtend.isEmpty()) {
                auto tPos2 = dtend.indexOf(QLatin1Char('T'));
                if (tPos2 >= 0) {
                    auto timePart2 = dtend.mid(tPos2 + 1, 6);
                    if (timePart2.length() == 6) {
                        timeStr += QStringLiteral(" - ") + timePart2.left(2) + QStringLiteral(":") + timePart2.mid(2, 2);
                    }
                }
            }
        }

        auto displayText = timeStr.isEmpty() ? summary : timeStr + QStringLiteral(" ") + summary;
        if (summary.isEmpty()) displayText = timeStr.isEmpty() ? QStringLiteral("(Unbenannter Termin)") : timeStr;

        _eventList->addItem(displayText);
    }
}

void CalendarPanel::onEventCreated(bool success)
{
    if (success) {
        qCInfo(lcCalendarPanel) << "Event created successfully";
        auto today = QDate::currentDate();
        if (!_currentCalendarUri.isEmpty()) {
            _calDavSync->fetchEvents(_currentCalendarUri, today, today);
        }
    } else {
        qCWarning(lcCalendarPanel) << "Failed to create event";
        QMessageBox::warning(this, QStringLiteral("Fehler"),
            QStringLiteral("Termin konnte nicht erstellt werden."));
    }
}

} // namespace OCC
