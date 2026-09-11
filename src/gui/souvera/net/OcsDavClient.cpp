/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OcsDavClient.h"

#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcOcsDavClient, "souvera.net.ocsdav")

namespace OCC {

namespace {

QByteArray authBytes(AccountState *accountState)
{
    const auto acc = accountState ? accountState->account() : nullptr;
    const auto creds = acc ? acc->credentials() : nullptr;
    if (!creds) return {};
    return QStringLiteral("%1:%2").arg(creds->user(), creds->password()).toUtf8().toBase64();
}

QString statusMessage(int status, const QString &fallback)
{
    if (status == 0) {
        return QStringLiteral("Keine Verbindung zum Server: %1").arg(fallback);
    }
    if (status == 401 || status == 403) {
        return QStringLiteral("Zugriff verweigert (%1). Bitte erneut anmelden.").arg(status);
    }
    if (status == 404) {
        return QStringLiteral("Nicht gefunden (404) \u2014 ist die App auf dem Server aktiv?");
    }
    if (status >= 500) {
        return QStringLiteral("Serverfehler (%1). Bitte sp\u00E4ter erneut versuchen.").arg(status);
    }
    return QStringLiteral("Anfrage fehlgeschlagen (HTTP %1): %2").arg(status).arg(fallback);
}

void runRequest(AccountState *accountState, const QByteArray &verb, const QUrl &url,
                const QList<QPair<QByteArray, QByteArray>> &headers, const QByteArray &body,
                const std::function<void(QNetworkReply *, int)> &onFinished)
{
    const QPointer<AccountState> guard(accountState);
    const auto acc = accountState ? accountState->account() : nullptr;
    const auto auth = authBytes(accountState);
    if (!acc || auth.isEmpty()) {
        qCWarning(lcOcsDavClient) << "Request aborted: no account or empty credentials."
                                  << "Account:" << (acc ? "present" : "null")
                                  << "Auth length:" << auth.size();
        onFinished(nullptr, -1);
        return;
    }

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Basic " + auth);
    req.setTransferTimeout(20000);
    for (const auto &h : headers) {
        req.setRawHeader(h.first, h.second);
    }

    QNetworkReply *reply = nullptr;
    auto *nam = acc->networkAccessManager();
    if (verb == "GET" && body.isEmpty()) {
        reply = nam->get(req);
    } else if (verb == "POST") {
        reply = nam->post(req, body);
    } else if (verb == "PUT") {
        reply = nam->put(req, body);
    } else if (verb == "DELETE" && body.isEmpty()) {
        reply = nam->deleteResource(req);
    } else {
        reply = nam->sendCustomRequest(req, verb, body);
    }

    qCInfo(lcOcsDavClient) << ">>>" << verb << url.toString();

    QObject::connect(reply, &QNetworkReply::finished, reply, [guard, reply, onFinished, verb, url]() {
        reply->deleteLater();
        if (!guard) {
            return;
        }
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto errorStr = reply->errorString();

        if (status == 0 || (status < 200 || status >= 300)) {
            qCWarning(lcOcsDavClient) << "<<<" << verb << url.toString()
                                       << "status:" << status
                                       << "error:" << errorStr;
        } else {
            qCInfo(lcOcsDavClient) << "<<<" << verb << url.toString()
                                    << "status:" << status;
        }

        onFinished(reply, status);
    });
}

} // namespace

void OcsDavClient::ocsRequest(AccountState *accountState, const QByteArray &verb,
                              const QString &ocsPath, const QByteArray &body,
                              const JsonCallback &onJson, const ErrorCallback &onError,
                              const HeaderList &extraHeaders)
{
    HeaderList headers = {{QByteArray("OCS-APIRequest"), QByteArray("true")},
                          {QByteArray("Accept"), QByteArray("application/json")}};
    if (!extraHeaders.isEmpty()) {
        headers.append(extraHeaders);
    }

    // Force JSON: some Nextcloud servers ignore the Accept header and
    // return XML unless format=json is in the query string.
    QUrl url(ocsPath);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query);

    runRequest(accountState, verb, url,
               headers,
               body,
               [onJson, onError, ocsPath](QNetworkReply *reply, int status) {
        if (!reply) {
            onError(-1, QStringLiteral("Kein Konto verbunden."));
            return;
        }
        if (status < 200 || status >= 300) {
            onError(status, statusMessage(status, reply->errorString()));
            return;
        }

        const auto rawData = reply->readAll();
        const auto doc = QJsonDocument::fromJson(rawData);
        if (doc.isNull()) {
            // Log the first 300 bytes so the user report tells us exactly
            // what the server returned (HTML login page, XML, empty, etc.)
            const auto preview = QString::fromUtf8(rawData.left(300));
            qCWarning(lcOcsDavClient) << "OCS non-JSON response for" << ocsPath
                                      << "status:" << status
                                      << "body:" << preview;
            onError(status, QStringLiteral(
                "Ung\u00FCltige Server-Antwort. Der Server hat kein JSON zur\u00FCckgegeben.\n"
                "Antwort-Anfang: %1").arg(preview.left(120)));
            return;
        }
        const auto root = doc.object();
        const auto meta = root.value(QStringLiteral("meta")).toObject();
        const auto metaStatus = meta.value(QStringLiteral("status")).toInt(0);
        const auto payload = root.value(QStringLiteral("data"));

        // OCS wraps errors in meta.statuscode even on HTTP 200.
        if (metaStatus != 0 && (metaStatus < 200 || metaStatus >= 300)) {
            onError(metaStatus, meta.value(QStringLiteral("message")).toString(
                   QStringLiteral("OCS-Fehler %1").arg(metaStatus)));
            return;
        }
        onJson(payload.toObject(), status);
    });
}

void OcsDavClient::davRequestDepth(AccountState *accountState, const QByteArray &verb,
                                   const QUrl &url, const QByteArray &xmlBody, int depth,
                                   const RawCallback &onBody, const ErrorCallback &onError)
{
    HeaderList headers = {{QByteArray("Accept"), QByteArray("application/xml")},
                          {QByteArray("Content-Type"), QByteArray("application/xml; charset=utf-8")},
                          {QByteArray("Depth"), QByteArray::number(depth)}};
    runRequest(accountState, verb, url,
               headers,
               xmlBody,
               [onBody, onError, url](QNetworkReply *reply, int status) {
        if (!reply) {
            onError(-1, QStringLiteral("Kein Konto verbunden."));
            return;
        }
        if (status < 200 || status >= 300) {
            const auto bodyPreview = QString::fromUtf8(reply->readAll().left(200));
            qCWarning(lcOcsDavClient) << "DAV request failed:" << status
                                      << "url:" << url.toString()
                                      << "body:" << bodyPreview;
            onError(status, statusMessage(status, reply->errorString()));
            return;
        }
        onBody(reply->readAll(), status);
    });
}

void OcsDavClient::jsonRequest(AccountState *accountState, const QByteArray &verb,
                               const QUrl &url, const QByteArray &jsonBody,
                               const std::function<void(const QJsonDocument &, int)> &onJson,
                               const ErrorCallback &onError)
{
    runRequest(accountState, verb, url,
               {{QByteArray("Accept"), QByteArray("application/json")},
                {QByteArray("Content-Type"), QByteArray("application/json")}},
               jsonBody,
               [onJson, onError](QNetworkReply *reply, int status) {
        if (!reply) {
            onError(-1, QStringLiteral("Kein Konto verbunden."));
            return;
        }
        if (status < 200 || status >= 300) {
            onError(status, statusMessage(status, reply->errorString()));
            return;
        }
        onJson(QJsonDocument::fromJson(reply->readAll()), status);
    });
}

void OcsDavClient::davRequest(AccountState *accountState, const QByteArray &verb,
                              const QUrl &url, const QByteArray &xmlBody,
                              const RawCallback &onBody, const ErrorCallback &onError)
{
    // PROPFIND/REPORT require a Depth header; default to 0 (collection only).
    const auto isPropfind = (verb == "PROPFIND");
    HeaderList headers = {{QByteArray("Accept"), QByteArray("application/xml")},
                          {QByteArray("Content-Type"), QByteArray("application/xml; charset=utf-8")},
                          {QByteArray("Depth"), isPropfind ? QByteArray("0") : QByteArray("1")}};
    runRequest(accountState, verb, url,
               headers,
               xmlBody,
               [onBody, onError, url](QNetworkReply *reply, int status) {
        if (!reply) {
            onError(-1, QStringLiteral("Kein Konto verbunden."));
            return;
        }
        if (status < 200 || status >= 300) {
            const auto bodyPreview = QString::fromUtf8(reply->readAll().left(200));
            qCWarning(lcOcsDavClient) << "DAV request failed:" << status
                                      << "url:" << url.toString()
                                      << "body:" << bodyPreview;
            onError(status, statusMessage(status, reply->errorString()));
            return;
        }
        onBody(reply->readAll(), status);
    });
}

} // namespace OCC
