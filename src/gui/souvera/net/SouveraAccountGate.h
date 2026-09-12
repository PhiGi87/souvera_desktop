/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SOUVERAACCOUNTGATE_H
#define SOUVERAACCOUNTGATE_H

#include <functional>

class QObject;

namespace OCC {

class AccountState;

namespace Sou {

/**
 * @brief Runs a callback once the account's credentials are usable.
 *
 * Panel requests issued before the asynchronous keychain read completes go
 * out with empty credentials and can wedge permanently, so every panel-side
 * fetch must be routed through this gate. The callback runs immediately
 * (synchronously) when credentials are already ready; otherwise it runs on
 * the account's credentialsFetched signal, delivered in the context of
 * @p context (the panel), and only once.
 *
 * If @p state or its account is null, the callback never runs.
 */
void whenCredentialsReady(AccountState *state, QObject *context, const std::function<void()> &callback);

} // namespace Sou

} // namespace OCC

#endif // SOUVERAACCOUNTGATE_H
