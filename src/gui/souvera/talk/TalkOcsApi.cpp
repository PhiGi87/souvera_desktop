/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkOcsApi.h"

#include "accountstate.h"
#include "account.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcTalkOcsApi, "souvera.talk.ocsapi")

namespace OCC {

TalkOcsApi::TalkOcsApi(QObject *parent)
    : QObject(parent)
    , _nam(new QNetworkAccessManager(this))
{
}

QString TalkOcsApi::ocsUrl(const QString &path) const
{
    if (!_accountState || !_accountState->account()) return {};
    const auto base = _accountState->account()->url().toString();
    return QStringLiteral("%1/ocs/v2.php/apps/spreed/api/v1%2").arg(base, path);
}

void TalkOcsApi::fetchConversations()
{
    const auto url = ocsUrl(QStringLiteral("/room"));
    if (url.isEmpty()) return;

    QNetworkRequest req(url);
    req.setRawHeader("OCS-APIRequest", "true");

    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        // creds auth handled by account/AbstractNetworkJob
    }

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcTalkOcsApi) << "fetchConversations failed:" << reply->errorString();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto data = doc.object().value(QStringLiteral("ocs")).toObject()
                              .value(QStringLiteral("data")).toArray();
        emit conversationsReceived(data);
    });
}

void TalkOcsApi::fetchMessages(const QString &token)
{
    const auto url = ocsUrl(QStringLiteral("/chat/%1").arg(token));
    if (url.isEmpty()) return;

    QNetworkRequest req(url);
    req.setRawHeader("OCS-APIRequest", "true");

    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        // creds auth handled by account/AbstractNetworkJob
    }

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcTalkOcsApi) << "fetchMessages failed:" << reply->errorString();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto data = doc.object().value(QStringLiteral("ocs")).toObject()
                              .value(QStringLiteral("data")).toArray();
        emit messagesReceived(data);
    });
}

void TalkOcsApi::sendMessage(const QString &token, const QString &text)
{
    const auto url = ocsUrl(QStringLiteral("/chat/%1").arg(token));
    if (url.isEmpty()) return;

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("OCS-APIRequest", "true");

    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        // creds auth handled by account/AbstractNetworkJob
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("message"), text);
    auto *reply = _nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcTalkOcsApi) << "sendMessage failed:" << reply->errorString();
            return;
        }
        emit messageSent();
    });
}

} // namespace OCC
