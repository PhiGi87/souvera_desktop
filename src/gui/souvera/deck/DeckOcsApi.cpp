/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckOcsApi.h"
#include "net/OcsDavClient.h"

#include "accountstate.h"
#include "account.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

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

    OcsDavClient::jsonRequest(_accountState, "GET", QUrl(url), {},
        [this](const QJsonDocument &doc, int) {
            if (!doc.isArray() || doc.array().isEmpty()) {
                qCWarning(lcDeckOcsApi) << "Boards response is not a non-empty array; head:"
                                        << QString::fromUtf8(doc.toJson(QJsonDocument::Compact).left(200));
            }
            emit boardsReceived(doc.array());
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "fetchBoards failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::fetchStacks(int boardId)
{
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks").arg(boardId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "GET", QUrl(url), {},
        [this](const QJsonDocument &doc, int) {
            emit stacksReceived(doc.array());
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "fetchStacks failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::createCard(int boardId, int stackId, const QString &title, const QString &description)
{
    // POST /boards/{boardId}/stacks/{stackId}/cards (Android DeckAPI.createCard)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards").arg(boardId).arg(stackId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("title")] = title;
    body[QStringLiteral("description")] = description;
    body[QStringLiteral("type")] = QStringLiteral("plain");

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "POST", QUrl(url), payload,
        [this, boardId, stackId](const QJsonDocument &doc, int) {
            emit cardCreated(doc.object(), boardId, stackId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "createCard failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::moveCard(int boardId, int stackId, int cardId, int order)
{
    // PUT /boards/{boardId}/stacks/{stackId}/cards/{cardId}/reorder (Android DeckAPI.moveCard)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3/reorder")
                                .arg(boardId).arg(stackId).arg(cardId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("order")] = order;

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "PUT", QUrl(url), payload,
        [this](const QJsonDocument &, int) {
            emit cardMoved();
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "moveCard failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::deleteCard(int boardId, int stackId, int cardId)
{
    // DELETE /boards/{boardId}/stacks/{stackId}/cards/{cardId} (Android DeckAPI.deleteCard)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3")
                                .arg(boardId).arg(stackId).arg(cardId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "DELETE", QUrl(url), {},
        [this, boardId, stackId, cardId](const QJsonDocument &, int) {
            emit cardDeleted(boardId, stackId, cardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "deleteCard failed:" << message;
            emit apiError(message);
        });
}

} // namespace OCC
