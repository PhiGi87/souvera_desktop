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
{
}

void DeckOcsApi::setAccountState(AccountState *state)
{
    _accountState = state;
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

    const auto nam = _accountState->account()->networkAccessManager();
    auto *reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "fetchBoards failed:" << reply->errorString();
            emit apiError(reply->errorString());
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

    const auto nam = _accountState->account()->networkAccessManager();
    auto *reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "fetchStacks failed:" << reply->errorString();
            emit apiError(reply->errorString());
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

    const auto nam = _accountState->account()->networkAccessManager();
    auto *reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, stackId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "fetchCards failed:" << reply->errorString();
            emit apiError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit cardsReceived(doc.array(), stackId);
    });
}

void DeckOcsApi::createCard(int stackId, const QString &title, const QString &description)
{
    const auto url = apiUrl(QStringLiteral("/stacks/%1/cards").arg(stackId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("title")] = title;
    body[QStringLiteral("description")] = description;
    body[QStringLiteral("type")] = QStringLiteral("plain");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const auto nam = _accountState->account()->networkAccessManager();
    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto *reply = nam->post(req, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, stackId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "createCard failed:" << reply->errorString();
            emit apiError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit cardCreated(doc.object(), stackId);
    });
}

void DeckOcsApi::moveCard(int cardId, int stackId)
{
    const auto url = apiUrl(QStringLiteral("/cards/%1/stack/%2").arg(cardId).arg(stackId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("order")] = 999;

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const auto nam = _accountState->account()->networkAccessManager();
    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto *reply = nam->put(req, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDeckOcsApi) << "moveCard failed:" << reply->errorString();
            emit apiError(reply->errorString());
            return;
        }
        emit cardMoved();
    });
}

} // namespace OCC
