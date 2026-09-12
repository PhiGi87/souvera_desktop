/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include <QObject>
#include <QPointer>
#include <QJsonObject>

#include "DeckModels.h"

namespace OCC {

class AccountState;

/**
 * @brief Account-bound Deck hub: server capability gate + board cache.
 *
 * - Queries /cloud/capabilities once per account and exposes the Deck app
 *   version gates (owner-as-string since 1.17.0, start date / card color
 *   since 1.18.0, done since 1.17.0).
 * - Persists the last board state as JSON so switching back to the tab
 *   renders instantly from cache before the refresh arrives.
 */
class DeckManager : public QObject
{
    Q_OBJECT
public:
    static DeckManager *instance();

    void setAccountState(AccountState *accountState);

    /** Deck app version reported by the server, empty when unknown. */
    [[nodiscard]] QString deckVersion() const { return _deckVersion; }
    [[nodiscard]] bool supportsOwnerAsString() const { return _ownerAsString; }
    [[nodiscard]] bool supportsStartDate() const { return _startDate; }
    [[nodiscard]] bool supportsCardColor() const { return _cardColor; }

    /** Instant-load cache for a board (empty document when nothing cached). */
    [[nodiscard]] QJsonArray cachedStacks(int boardId) const;
    void storeStacksCache(int boardId, const QJsonArray &stacks);

signals:
    void capabilitiesReady();

private:
    explicit DeckManager(QObject *parent = nullptr);
    void fetchCapabilities();

    QPointer<AccountState> _accountState;
    QString _deckVersion;
    bool _ownerAsString = true;
    bool _startDate = true;
    bool _cardColor = true;
    bool _capabilitiesFetched = false;
};

} // namespace OCC

#endif // DECKMANAGER_H
