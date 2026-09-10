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

    QObject::connect(reply, &QNetworkReply::finished, reply, [guard, reply, onFinished]() {
        reply->deleteLater();
        if (!guard) {
            return;
        }
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
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
    runRequest(accountState, verb, QUrl(ocsPath),
               headers,
               body,
               [onJson, onError](QNetworkReply *reply, int status) {
        if (!reply) {
            onError(-1, QStringLiteral("Kein Konto verbunden."));
            return;
        }
        if (status < 200 || status >= 300) {
            onError(status, statusMessage(status, reply->errorString()));
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull()) {
            onError(status, QStringLiteral("Ung\u00FCltige Server-Antwort (kein JSON)."));
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
    runRequest(accountState, verb, url,
               {{QByteArray("Accept"), QByteArray("application/xml")},
                {QByteArray("Content-Type"), QByteArray("application/xml; charset=utf-8")}},
               xmlBody,
               [onBody, onError](QNetworkReply *reply, int status) {
        if (!reply) {
            onError(-1, QStringLiteral("Kein Konto verbunden."));
            return;
        }
        if (status < 200 || status >= 300) {
            onError(status, statusMessage(status, reply->errorString()));
            return;
        }
        onBody(reply->readAll(), status);
    });
}

} // namespace OCC
