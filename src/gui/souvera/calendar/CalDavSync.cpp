/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CalDavSync.h"

#include "accountstate.h"
#include "account.h"

#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QXmlStreamReader>
#include <QUuid>

Q_LOGGING_CATEGORY(lcCalDavSync, "souvera.calendar.caldavsync")

namespace OCC {

CalDavSync::CalDavSync(QObject *parent)
    : QObject(parent)
{
}

void CalDavSync::setAccountState(AccountState *state)
{
    _accountState = state;
}

void CalDavSync::fetchCalendars()
{
    if (!_accountState || !_accountState->account()) {
        emit errorOccurred(QStringLiteral("No account state"));
        return;
    }

    auto url = makeCalendarUrl(QString());
    QNetworkRequest req(url);
    req.setRawHeader("Depth", "1");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml; charset=utf-8"));

    auto body = buildPropfindBody();
    auto *reply = _accountState->account()->sendRawRequest("PROPFIND", url, req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        auto calendars = parseCalendars(reply->readAll());
        emit calendarsLoaded(calendars);
    });
}

void CalDavSync::fetchEvents(const QString &calendarUri, const QDate &from, const QDate &to)
{
    if (!_accountState || !_accountState->account()) {
        emit errorOccurred(QStringLiteral("No account state"));
        return;
    }

    auto url = makeCalendarUrl(calendarUri);
    QNetworkRequest req(url);
    req.setRawHeader("Depth", "1");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml; charset=utf-8"));

    auto body = buildReportBody(from, to);
    auto *reply = _accountState->account()->sendRawRequest("REPORT", url, req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        auto events = parseEvents(reply->readAll());
        emit eventsLoaded(events);
    });
}

void CalDavSync::createEvent(const QString &calendarUri, const QByteArray &iCalData)
{
    if (!_accountState || !_accountState->account()) {
        emit errorOccurred(QStringLiteral("No account state"));
        return;
    }

    auto uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto url = makeEventUrl(calendarUri, uid);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/calendar; charset=utf-8"));

    auto *reply = _accountState->account()->sendRawRequest("PUT", url, req, iCalData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit eventCreated(false);
            return;
        }
        auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto success = (statusCode == 201 || statusCode == 204);
        if (!success) {
            qCWarning(lcCalDavSync) << "createEvent returned" << statusCode;
        }
        emit eventCreated(success);
    });
}

QByteArray CalDavSync::buildPropfindBody()
{
    return QByteArray(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:cs=\"http://calendarserver.org/ns/\">"
        "  <d:prop>"
        "    <d:displayname/>"
        "    <cs:getctag/>"
        "  </d:prop>"
        "</d:propfind>");
}

QByteArray CalDavSync::buildReportBody(const QDate &from, const QDate &to)
{
    auto startStr = from.toString(QStringLiteral("yyyyMMdd")) + QStringLiteral("T000000Z");
    auto endStr = to.toString(QStringLiteral("yyyyMMdd")) + QStringLiteral("T235959Z");

    auto body = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<cal:calendar-query xmlns:d=\"DAV:\" xmlns:cal=\"urn:ietf:params:xml:ns:caldav\">"
        "  <d:prop>"
        "    <d:getetag/>"
        "    <cal:calendar-data/>"
        "  </d:prop>"
        "  <cal:filter>"
        "    <cal:comp-filter name=\"VCALENDAR\">"
        "      <cal:comp-filter name=\"VEVENT\">"
        "        <cal:time-range start=\"%1\" end=\"%2\"/>"
        "      </cal:comp-filter>"
        "    </cal:comp-filter>"
        "  </cal:filter>"
        "</cal:calendar-query>")
        .arg(startStr, endStr);

    return body.toUtf8();
}

QUrl CalDavSync::makeCalendarUrl(const QString &calendarUri) const
{
    auto base = _accountState->account()->url().toString();
    if (!base.endsWith(QLatin1Char('/'))) base += QLatin1Char('/');
    auto path = QStringLiteral("remote.php/dav/calendars/%1/").arg(_accountState->account()->davUser());
    if (!calendarUri.isEmpty()) path += calendarUri + QLatin1Char('/');
    return QUrl(base + path);
}

QUrl CalDavSync::makeEventUrl(const QString &calendarUri, const QString &uid) const
{
    auto base = _accountState->account()->url().toString();
    if (!base.endsWith(QLatin1Char('/'))) base += QLatin1Char('/');
    auto path = QStringLiteral("remote.php/dav/calendars/%1/%2/%3.ics")
                    .arg(_accountState->account()->davUser(), calendarUri, uid);
    return QUrl(base + path);
}

QVariantList CalDavSync::parseCalendars(const QByteArray &data)
{
    QVariantList calendars;
    QXmlStreamReader xml(data);

    static const auto davNs = QStringLiteral("DAV:");
    static const auto csNs = QStringLiteral("http://calendarserver.org/ns/");

    QVariantMap current;

    while (!xml.atEnd()) {
        auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            auto ns = xml.namespaceUri().toString();
            auto name = xml.name().toString();
            if (ns == davNs && name == QStringLiteral("response")) {
                current = QVariantMap();
            } else if (ns == davNs && name == QStringLiteral("href")) {
                current[QStringLiteral("href")] = xml.readElementText();
            } else if (ns == davNs && name == QStringLiteral("displayname")) {
                auto text = xml.readElementText();
                current[QStringLiteral("displayname")] = text;
            } else if (ns == csNs && name == QStringLiteral("getctag")) {
                current[QStringLiteral("ctag")] = xml.readElementText();
            }
        } else if (token == QXmlStreamReader::EndElement) {
            auto ns = xml.namespaceUri().toString();
            auto name = xml.name().toString();
            if (ns == davNs && name == QStringLiteral("response")) {
                if (!current.isEmpty() && current.contains(QStringLiteral("href"))
                    && !current.value(QStringLiteral("displayname")).toString().isEmpty()) {
                    calendars.append(current);
                }
                current = QVariantMap();
            }
        }
        if (xml.hasError()) {
            qCWarning(lcCalDavSync) << "XML parse error in calendars:" << xml.errorString();
            break;
        }
    }

    return calendars;
}

QVariantList CalDavSync::parseEvents(const QByteArray &data)
{
    QVariantList events;
    QXmlStreamReader xml(data);

    static const auto davNs = QStringLiteral("DAV:");
    static const auto calNs = QStringLiteral("urn:ietf:params:xml:ns:caldav");

    QVariantMap current;

    while (!xml.atEnd()) {
        auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            auto ns = xml.namespaceUri().toString();
            auto name = xml.name().toString();
            if (ns == davNs && name == QStringLiteral("response")) {
                current = QVariantMap();
            } else if (ns == davNs && name == QStringLiteral("href")) {
                current[QStringLiteral("href")] = xml.readElementText();
            } else if (ns == davNs && name == QStringLiteral("getetag")) {
                current[QStringLiteral("etag")] = xml.readElementText();
            } else if (ns == calNs && name == QStringLiteral("calendar-data")) {
                auto rawICal = xml.readElementText();
                current[QStringLiteral("icaldata")] = rawICal;
                auto parsed = parseICalendar(rawICal);
                for (auto it = parsed.constBegin(); it != parsed.constEnd(); ++it) {
                    current[it.key()] = it.value();
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            auto ns = xml.namespaceUri().toString();
            auto name = xml.name().toString();
            if (ns == davNs && name == QStringLiteral("response")) {
                if (!current.isEmpty()) events.append(current);
                current = QVariantMap();
            }
        }
        if (xml.hasError()) {
            qCWarning(lcCalDavSync) << "XML parse error in events:" << xml.errorString();
            break;
        }
    }

    return events;
}

QVariantMap CalDavSync::parseICalendar(const QString &iCalText)
{
    QVariantMap result;
    auto lines = iCalText.split(QStringLiteral("\r\n"));
    auto inVevent = false;
    for (const auto &line : lines) {
        if (line == QStringLiteral("BEGIN:VEVENT")) {
            inVevent = true;
            continue;
        }
        if (line == QStringLiteral("END:VEVENT")) break;
        if (!inVevent) continue;

        auto colonPos = line.indexOf(QLatin1Char(':'));
        if (colonPos < 0) continue;

        auto key = line.left(colonPos);
        auto value = line.mid(colonPos + 1);

        if (key == QStringLiteral("SUMMARY")) {
            result[QStringLiteral("summary")] = value;
        } else if (key == QStringLiteral("DESCRIPTION")) {
            result[QStringLiteral("description")] = value;
        } else if (key == QStringLiteral("DTSTART")) {
            result[QStringLiteral("dtstart")] = value;
        } else if (key == QStringLiteral("DTEND")) {
            result[QStringLiteral("dtend")] = value;
        } else if (key == QStringLiteral("UID")) {
            result[QStringLiteral("uid")] = value;
        } else if (key == QStringLiteral("LOCATION")) {
            result[QStringLiteral("location")] = value;
        }
    }
    return result;
}

} // namespace OCC
