/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALDAVSYNC_H
#define CALDAVSYNC_H

#include <QObject>
#include <QVariantList>
#include <QDate>

namespace OCC {

class AccountState;

class CalDavSync : public QObject
{
    Q_OBJECT
public:
    explicit CalDavSync(QObject *parent = nullptr);

    void setAccountState(AccountState *state);
    void fetchCalendars();
    void fetchEvents(const QString &calendarUri, const QDate &from, const QDate &to);
    void createEvent(const QString &calendarUri, const QByteArray &iCalData);

signals:
    void calendarsLoaded(const QVariantList &calendars);
    void eventsLoaded(const QVariantList &events);
    void eventCreated(bool success);
    void errorOccurred(const QString &message);

private:
    QVariantList parseCalendars(const QByteArray &data);
    QVariantList parseEvents(const QByteArray &data);
    static QVariantMap parseICalendar(const QString &iCalText);
    QByteArray buildPropfindBody();
    QByteArray buildReportBody(const QDate &from, const QDate &to);
    QUrl makeCalendarUrl(const QString &calendarUri) const;
    QUrl makeEventUrl(const QString &calendarUri, const QString &uid) const;

    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // CALDAVSYNC_H
