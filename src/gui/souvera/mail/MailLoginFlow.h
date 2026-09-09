/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILLOGINFLOW_H
#define MAILLOGINFLOW_H

#include <QString>
#include <functional>

namespace OCC {

class AccountState;

struct CombinedAppPassword {
    QString loginName;
    QString appPassword;
    QString stalwartId;
};

/**
 * @brief Mints the combined Nextcloud+Stalwart mail credential.
 *
 * Wraps souvera_mail's `POST /apps/souvera_mail/app-passwords/login-flow`
 * endpoint, mirroring the Android/iOS clients: the Nextcloud login hands the
 * client an app password X (used for Files/DAV). This endpoint mints an
 * ADDITIONAL combined password Y that Stalwart (JMAP/IMAP/SMTP) also
 * accepts, used ONLY for the mail client. X is deliberately NOT revoked.
 *
 * The minted credential is cached in QSettings; a re-mint is forced after
 * server-side reprovisioning (401) via clearCachedPassword().
 */
class MailLoginFlow
{
public:
    using SuccessFn = std::function<void(const CombinedAppPassword &)>;
    using ErrorFn = std::function<void(const QString &)>;

    /**
     * Returns the cached combined password, or an empty string when none is
     * stored (or it belongs to a different credential generation).
     */
    [[nodiscard]] static QString cachedPassword(AccountState *accountState);

    /** The stalwartId stored alongside the cached password (may be empty). */
    [[nodiscard]] static QString cachedStalwartId(AccountState *accountState);

    /** Clears the cached combined password (e.g. after a 401 on the mail server). */
    static void clearCachedPassword(AccountState *accountState);

    /**
     * Best-effort server-side deletion of the combined password. Call before
     * re-minting so repeated mints do not accumulate stale rows.
     */
    static void deleteCombinedPassword(AccountState *accountState, const QString &stalwartId);

    /**
     * Mints (or returns the cached) combined mail password and invokes the
     * matching callback on the GUI thread.
     */
    static void ensureCombinedPassword(AccountState *accountState,
                                       const SuccessFn &onSuccess,
                                       const ErrorFn &onError);
};

} // namespace OCC

#endif // MAILLOGINFLOW_H
