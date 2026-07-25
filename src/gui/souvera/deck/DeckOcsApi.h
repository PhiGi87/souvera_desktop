/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKOCSAPI_H
#define DECKOCSAPI_H

#include <QObject>
#include <QJsonArray>
#include <QNetworkAccessManager>

namespace OCC {

class AccountState;

class DeckOcsApi : public QObject
{
    Q_OBJECT
public:
    explicit DeckOcsApi(QObject *parent = nullptr);

    void setAccountState(AccountState *state) { _accountState = state; }

    void fetchBoards();
    void fetchStacks(int boardId);
    void fetchCards(int stackId);

signals:
    void boardsReceived(const QJsonArray &boards);
    void stacksReceived(const QJsonArray &stacks);
    void cardsReceived(const QJsonArray &cards);

private:
    [[nodiscard]] QString apiUrl(const QString &path) const;

    QNetworkAccessManager *_nam = nullptr;
    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // DECKOCSAPI_H
