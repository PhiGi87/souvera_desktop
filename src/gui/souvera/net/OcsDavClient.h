/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCSDAVCLIENT_H
#define OCSDAVCLIENT_H

#include <QByteArray>
#include <QList>
#include <QPair>
#include <functional>

class QByteArray;
class QJsonDocument;
class QJsonObject;
class QJsonValue;
class QString;
class QUrl;

namespace OCC {

class AccountState;

/**
 * @brief Central HTTP layer for OCS and DAV requests of the workspace panels.
 *
 * All requests carry explicit Basic credentials (the Nextcloud app password),
 * the JSON/XML accept headers and a transfer timeout. Failures are reported
 * with user-readable German messages instead of being silently dropped.
 */
class OcsDavClient
{
public:
    using JsonCallback = std::function<void(const QJsonValue &payload, int httpStatus)>;
    using ErrorCallback = std::function<void(int httpStatus, const QString &message)>;
    using RawCallback = std::function<void(const QByteArray &body, int httpStatus)>;
    using HeaderList = QList<QPair<QByteArray, QByteArray>>;

    /**
     * OCS request (Talk, Deck, ...). The payload passed to onJson is the
     * parsed OCS "data" element. Non-2xx and OCS meta errors call onError.
     */
    static void ocsRequest(AccountState *accountState, const QByteArray &verb,
                           const QString &ocsPath, const QByteArray &body,
                           const JsonCallback &onJson, const ErrorCallback &onError,
                           const HeaderList &extraHeaders = {},
                           bool addFormatJson = true);

    /**
     * Plain JSON request (Deck REST API returns bare arrays). The callback
     * receives the parsed JSON document root.
     */
    static void jsonRequest(AccountState *accountState, const QByteArray &verb,
                            const QUrl &url, const QByteArray &jsonBody,
                            const std::function<void(const QJsonDocument &, int)> &onJson,
                            const ErrorCallback &onError);

    /**
     * Raw binary GET/POST returning the untouched response body (file
     * downloads etc.).
     */
    static void binaryRequest(AccountState *accountState, const QByteArray &verb,
                              const QUrl &url, const QByteArray &body,
                              const std::function<void(const QByteArray &, int)> &onBody,
                              const ErrorCallback &onError);

    /**
     * Multipart form upload (Deck attachment upload). The callback receives
     * the parsed JSON response root.
     */
    static void multipartRequest(AccountState *accountState, const QUrl &url,
                                 class QHttpMultiPart *multiPart,
                                 const std::function<void(const QJsonDocument &, int)> &onJson,
                                 const ErrorCallback &onError);

    /** DAV request (PROPFIND/REPORT/...) returning the raw XML body. */
    static void davRequest(AccountState *accountState, const QByteArray &verb,
                           const QUrl &url, const QByteArray &xmlBody,
                           const RawCallback &onBody, const ErrorCallback &onError);

    /** DAV request with explicit Depth header (0=collection only, 1=children too). */
    static void davRequestDepth(AccountState *accountState, const QByteArray &verb,
                                const QUrl &url, const QByteArray &xmlBody, int depth,
                                const RawCallback &onBody, const ErrorCallback &onError);
};

} // namespace OCC

#endif // OCSDAVCLIENT_H
