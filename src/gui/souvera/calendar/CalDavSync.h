/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALDAVSYNC_H
#define CALDAVSYNC_H

#include <QObject>
#include <QNetworkAccessManager>

namespace OCC {

class AccountState;

class CalDavSync : public QObject
{
    Q_OBJECT
public:
    explicit CalDavSync(QObject *parent = nullptr);

    void setAccountState(AccountState *state) { _accountState = state; }

    void fetchCalendars();
    void fetchEvents(const QString &calendarUri);

signals:
    void calendarsLoaded();
    void eventsLoaded();

private:
    QNetworkAccessManager *_nam = nullptr;
    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // CALDAVSYNC_H
