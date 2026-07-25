/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKOCSAPI_H
#define DECKOCSAPI_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

namespace OCC {

class AccountState;

class DeckOcsApi : public QObject
{
    Q_OBJECT
public:
    explicit DeckOcsApi(QObject *parent = nullptr);
    ~DeckOcsApi() override = default;

    void setAccountState(AccountState *state);

    void fetchBoards();
    void fetchStacks(int boardId);
    void fetchCards(int stackId);
    void createCard(int stackId, const QString &title, const QString &description);
    void moveCard(int cardId, int stackId);

signals:
    void boardsReceived(const QJsonArray &boards);
    void stacksReceived(const QJsonArray &stacks);
    void cardsReceived(const QJsonArray &cards, int stackId);
    void cardCreated(const QJsonObject &card, int stackId);
    void cardMoved();
    void apiError(const QString &message);

private:
    [[nodiscard]] QString apiUrl(const QString &path) const;

    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // DECKOCSAPI_H
