/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailLoginFlow.h"

#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>

Q_LOGGING_CATEGORY(lcMailLoginFlow, "souvera.mail.loginflow")

namespace OCC {

namespace {

constexpr auto CredVersionC = "2";
constexpr auto DescriptionC = "Souvera Desktop";

QByteArray authBytes(AccountState *accountState)
{
    const auto acc = accountState->account();
    const auto creds = acc ? acc->credentials() : nullptr;
    if (!creds) return {};
    return QStringLiteral("%1:%2").arg(creds->user(), creds->password()).toUtf8().toBase64();
}

QString settingsGroup(AccountState *accountState)
{
    const auto acc = accountState->account();
    const auto creds = acc ? acc->credentials() : nullptr;
    if (!acc || !creds) return QString();
    const auto user = creds->user();
    if (user.contains(QLatin1Char('/'))) {
        qCWarning(lcMailLoginFlow) << "User name contains '/' - cannot build settings key";
        return QString();
    }
    return QStringLiteral("souvera/mail-credentials/%1@%2").arg(user, acc->url().host());
}

void requestMint(AccountState *accountState, bool useIndexPhp,
                 const std::function<void(const CombinedAppPassword &)> &onSuccess,
                 const std::function<void(int, const QString &)> &onFailure)
{
    const QPointer<AccountState> guard(accountState);
    const auto acc = accountState ? accountState->account() : nullptr;
    const auto auth = authBytes(accountState);
    if (!acc || auth.isEmpty()) {
        onFailure(0, QStringLiteral("Konto nicht bereit."));
        return;
    }

    const auto path = useIndexPhp
        ? QStringLiteral("/index.php/apps/souvera_mail/app-passwords/login-flow")
        : QStringLiteral("/apps/souvera_mail/app-passwords/login-flow");
    auto url = acc->url();
    url.setPath(acc->url().path() + path);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Basic " + auth);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(15000);

    const auto body = QJsonDocument(QJsonObject{{QStringLiteral("description"), QLatin1String(DescriptionC)}}).toJson();

    auto *reply = acc->networkAccessManager()->post(req, body);
    QObject::connect(reply, &QNetworkReply::finished, reply, [guard, reply, onSuccess, onFailure, useIndexPhp]() {
        reply->deleteLater();
        if (!guard) {
            return; // account gone while the request was in flight
        }
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 404 && !useIndexPhp) {
            // Server without URL-rewriting: retry once with /index.php prefix.
            requestMint(guard.data(), true, onSuccess, onFailure);
            return;
        }
        // NOTE: Qt maps HTTP errors to error() != NoError, so status comes first.
        if (status == 401 || status == 403) {
            onFailure(status, QStringLiteral("Mail-Login abgelehnt (%1). Bitte erneut anmelden.").arg(status));
            return;
        }
        if (status >= 200 && status < 300) {
            const auto doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isNull()) {
                qCWarning(lcMailLoginFlow) << "Mail-Login response is not valid JSON";
            }
            const auto obj = doc.object();
            CombinedAppPassword result;
            result.loginName = obj.value(QStringLiteral("loginName")).toString();
            result.appPassword = obj.value(QStringLiteral("appPassword")).toString();
            result.stalwartId = obj.value(QStringLiteral("stalwartId")).toString();

            if (result.appPassword.isEmpty()) {
                onFailure(status, QStringLiteral("Mail-Login lieferte kein Passwort (ung\u00FCltige Server-Antwort)."));
                return;
            }
            onSuccess(result);
            return;
        }
        if (status == 0) {
            onFailure(0, QStringLiteral("Netzwerkfehler beim Mail-Login: %1").arg(reply->errorString()));
            return;
        }
        onFailure(status, QStringLiteral("Mail-Login fehlgeschlagen (HTTP %1).").arg(status));
    });
}

} // namespace

QString MailLoginFlow::cachedPassword(AccountState *accountState)
{
    const auto group = settingsGroup(accountState);
    if (group.isEmpty()) return QString();
    QSettings settings;
    settings.beginGroup(group);
    const auto password = settings.value(QStringLiteral("appPassword")).toString();
    const auto version = settings.value(QStringLiteral("version")).toString();
    if (password.isEmpty() || version != QLatin1String(CredVersionC)) {
        return QString();
    }
    return password;
}

QString MailLoginFlow::cachedStalwartId(AccountState *accountState)
{
    const auto group = settingsGroup(accountState);
    if (group.isEmpty()) return QString();
    QSettings settings;
    settings.beginGroup(group);
    return settings.value(QStringLiteral("stalwartId")).toString();
}

void MailLoginFlow::clearCachedPassword(AccountState *accountState)
{
    const auto group = settingsGroup(accountState);
    if (group.isEmpty()) return;
    QSettings settings;
    settings.beginGroup(group);
    settings.remove(QString());
}

void MailLoginFlow::deleteCombinedPassword(AccountState *accountState, const QString &stalwartId)
{
    if (stalwartId.isEmpty() || !accountState) return;
    const auto acc = accountState->account();
    const auto auth = authBytes(accountState);
    if (auth.isEmpty()) return;

    auto url = acc->url();
    url.setPath(acc->url().path() + QStringLiteral("/apps/souvera_mail/app-passwords/%1").arg(stalwartId));

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Basic " + auth);

    auto *reply = acc->networkAccessManager()->deleteResource(req);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply]() {
        // Best effort - a stale mapping row on the server is not fatal.
        reply->deleteLater();
    });
}

void MailLoginFlow::ensureCombinedPassword(AccountState *accountState,
                                           const SuccessFn &onSuccess,
                                           const ErrorFn &onError)
{
    if (!accountState) {
        onError(QStringLiteral("Kein Konto verbunden."));
        return;
    }

    const auto cached = cachedPassword(accountState);
    if (!cached.isEmpty()) {
        onSuccess(CombinedAppPassword{QString(), cached, QString()});
        return;
    }

    qCInfo(lcMailLoginFlow) << "Minting combined mail password";
    requestMint(accountState, false,
                [accountState, onSuccess, onError](const CombinedAppPassword &result) {
                    // Persist before reporting success.
                    const auto group = settingsGroup(accountState);
                    if (!group.isEmpty()) {
                        QSettings settings;
                        settings.beginGroup(group);
                        settings.setValue(QStringLiteral("appPassword"), result.appPassword);
                        settings.setValue(QStringLiteral("stalwartId"), result.stalwartId);
                        settings.setValue(QStringLiteral("loginName"), result.loginName);
                        settings.setValue(QStringLiteral("version"), QLatin1String(CredVersionC));
                    }
                    onSuccess(result);
                },
                [accountState, onError](int status, const QString &message) {
                    if (status != 404 && status != 401 && status != 403 && status != 0) {
                        // The mint itself failed (not a routing/auth retry case):
                        // clear any stale cache so the next attempt mints fresh.
                        MailLoginFlow::clearCachedPassword(accountState);
                    }
                    onError(message);
                });
}

} // namespace OCC
