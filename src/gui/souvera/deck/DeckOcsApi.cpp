/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckOcsApi.h"

#include "accountstate.h"
#include "account.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(lcDeckOcsApi, "souvera.deck.ocsapi")

namespace OCC {

DeckOcsApi::DeckOcsApi(QObject *parent)
    : QObject(parent)
    , _nam(new QNetworkAccessManager(this))
{
}

QString DeckOcsApi::apiUrl(const QString &path) const
{
    if (!_accountState || !_accountState->account()) return {};
    const auto base = _accountState->account()->url().toString();
    return QStringLiteral("%1/index.php/apps/deck/api/v1.0%2").arg(base, path);
}

void DeckOcsApi::fetchBoards()
{
    const auto url = apiUrl(QStringLiteral("/boards"));
    if (url.isEmpty()) return;

    QNetworkRequest req(url);
    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        creds->prepareRequest(&req);
    }

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "fetchBoards failed:" << reply->errorString();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit boardsReceived(doc.array());
    });
}

void DeckOcsApi::fetchStacks(int boardId)
{
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks").arg(boardId));
    if (url.isEmpty()) return;

    QNetworkRequest req(url);
    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        creds->prepareRequest(&req);
    }

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "fetchStacks failed:" << reply->errorString();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit stacksReceived(doc.array());
    });
}

void DeckOcsApi::fetchCards(int stackId)
{
    const auto url = apiUrl(QStringLiteral("/stacks/%1/cards").arg(stackId));
    if (url.isEmpty()) return;

    QNetworkRequest req(url);
    if (_accountState && _accountState->account()) {
        const auto creds = _accountState->account()->credentials();
        creds->prepareRequest(&req);
    }

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "fetchCards failed:" << reply->errorString();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit cardsReceived(doc.array());
    });
}

} // namespace OCC
