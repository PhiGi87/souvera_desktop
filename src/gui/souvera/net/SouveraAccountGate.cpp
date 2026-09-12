/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SouveraAccountGate.h"

#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"

namespace OCC::Sou {

void whenCredentialsReady(AccountState *state, QObject *context, const std::function<void()> &callback)
{
    const auto acc = state ? state->account() : nullptr;
    const auto creds = acc ? acc->credentials() : nullptr;
    if (!acc || !creds) return;

    if (creds->ready()) {
        callback();
        return;
    }

    // Single-shot: fires once when the keychain read (or the login flow)
    // finishes; dropped automatically if the panel or the account dies.
    QObject::connect(acc.data(), &Account::credentialsFetched, context,
                     [callback]() { callback(); }, Qt::SingleShotConnection);
}

} // namespace OCC::Sou
