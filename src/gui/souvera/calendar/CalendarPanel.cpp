/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CalendarPanel.h"
#include "CalDavSync.h"
#include "CalendarEventDialog.h"

#include "accountstate.h"

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
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);

    auto *title = new QLabel(QStringLiteral("Kalender"), toolbar);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();

    _newEventBtn = new QPushButton(QStringLiteral("+ Neuer Termin"), toolbar);
    _newEventBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4a90d9; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #357abd; }"));
    connect(_newEventBtn, &QPushButton::clicked, this, &CalendarPanel::onNewEvent);
    toolbarLayout->addWidget(_newEventBtn);

    layout->addWidget(toolbar);

    _splitter = new QSplitter(Qt::Vertical, this);

    _calendar = new QCalendarWidget(_splitter);
    _calendar->setGridVisible(true);
    _calendar->setStyleSheet(QStringLiteral(
        "QCalendarWidget { background-color: white; }"
        "QCalendarWidget QAbstractItemView:enabled { font-size: 13px; }"));
    connect(_calendar, &QCalendarWidget::clicked, this, &CalendarPanel::onDateSelected);
    _splitter->addWidget(_calendar);

    _eventList = new QListWidget(_splitter);
    _eventList->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid #ddd; border-radius: 4px; }"
        "QListWidget::item { padding: 6px 8px; }"));
    _splitter->addWidget(_eventList);

    _splitter->setStretchFactor(0, 2);
    _splitter->setStretchFactor(1, 3);

    layout->addWidget(_splitter, 1);
}

void CalendarPanel::onDateSelected(const QDate &date)
{
    _eventList->clear();
    qCInfo(lcCalendarPanel) << "Date selected:" << date;
    if (!_currentCalendarUri.isEmpty()) {
        _calDavSync->fetchEvents(_currentCalendarUri, date, date);
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
    _calendar->setSelectedDate(today);
    _calDavSync->fetchEvents(_currentCalendarUri, today, today);
}

void CalendarPanel::onEventsLoaded(const QVariantList &events)
{
    _eventList->clear();
    qCInfo(lcCalendarPanel) << "Events loaded:" << events.size();

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
        auto today = _calendar->selectedDate();
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
