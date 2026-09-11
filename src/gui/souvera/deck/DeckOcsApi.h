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
    void createStack(int boardId, const QString &title);
    void updateCard(int boardId, int stackId, int cardId, const QString &title,
                    const QString &description);
    void createCard(int boardId, int stackId, const QString &title, const QString &description);
    void moveCard(int boardId, int sourceStackId, int targetStackId, int cardId, int order);
    void deleteCard(int boardId, int stackId, int cardId);

signals:
    void boardsReceived(const QJsonArray &boards);
    void stacksReceived(const QJsonArray &stacks);
    void cardCreated(const QJsonObject &card, int boardId, int stackId);
    void cardMoved();
    void cardUpdated(const QJsonObject &card, int boardId, int stackId);
    void stackCreated(const QJsonObject &stack, int boardId);
    void cardDeleted(int boardId, int stackId, int cardId);
    void apiError(const QString &message);

private:
    [[nodiscard]] QString apiUrl(const QString &path) const;

    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // DECKOCSAPI_H
