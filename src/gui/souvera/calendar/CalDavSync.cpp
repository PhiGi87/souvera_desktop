/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CalDavSync.h"

#include "accountstate.h"
#include "account.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>

Q_LOGGING_CATEGORY(lcCalDavSync, "souvera.calendar.caldavsync")

namespace OCC {

CalDavSync::CalDavSync(QObject *parent)
    : QObject(parent)
    , _nam(new QNetworkAccessManager(this))
{
}

void CalDavSync::fetchCalendars()
{
    if (!_accountState || !_accountState->account()) return;

    const auto baseUrl = _accountState->account()->url().toString();
    const auto url = QStringLiteral("%1/remote.php/dav/calendars/%2/")
                         .arg(baseUrl, _accountState->account()->davUser());

    QNetworkRequest req(url);
    req.setRawHeader("Depth", "1");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml; charset=utf-8"));

    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        creds->prepareRequest(&req);
    }

    const QByteArray body =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:cs=\"http://calendarserver.org/ns/\">"
        "  <d:prop><d:displayname /><cs:getctag /></d:prop>"
        "</d:propfind>";

    auto *reply = _nam->sendCustomRequest(req, "PROPFIND", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcCalDavSync) << "fetchCalendars failed:" << reply->errorString();
            return;
        }
        qCInfo(lcCalDavSync) << "Calendars fetched successfully";
        emit calendarsLoaded();
    });
}

void CalDavSync::fetchEvents(const QString &calendarUri)
{
    Q_UNUSED(calendarUri)
    if (!_accountState || !_accountState->account()) return;

    const auto baseUrl = _accountState->account()->url().toString();
    const auto url = QStringLiteral("%1/remote.php/dav/calendars/%2/%3/")
                         .arg(baseUrl, _accountState->account()->davUser(), calendarUri);

    QNetworkRequest req(url);
    req.setRawHeader("Depth", "1");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml; charset=utf-8"));

    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        creds->prepareRequest(&req);
    }

    const QByteArray body =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:cal=\"urn:ietf:params:xml:ns:caldav\">"
        "  <d:prop><d:getetag /><cal:calendar-data /></d:prop>"
        "</d:propfind>";

    auto *reply = _nam->sendCustomRequest(req, "PROPFIND", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcCalDavSync) << "fetchEvents failed:" << reply->errorString();
            return;
        }
        qCInfo(lcCalDavSync) << "Events fetched successfully";
        emit eventsLoaded();
    });
}

} // namespace OCC
