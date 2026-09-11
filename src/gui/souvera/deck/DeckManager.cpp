/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckManager.h"
#include "account.h"
#include "accountstate.h"
#include "net/OcsDavClient.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QStandardPaths>

namespace OCC {

Q_LOGGING_CATEGORY(lcDeckManager, "souvera.deck.manager", QtInfoMsg)

namespace {
int versionComponent(const QString &version, int index)
{
    const auto parts = version.split(QLatin1Char('.'));
    return index < parts.size() ? parts.at(index).toInt() : 0;
}

QString cacheFilePath()
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base + QStringLiteral("/deck-cache"));
    return base + QStringLiteral("/deck-cache/board-%1.json");
}
}

DeckManager *DeckManager::instance()
{
    static DeckManager manager;
    return &manager;
}

DeckManager::DeckManager(QObject *parent)
    : QObject(parent)
{
}

void DeckManager::setAccountState(AccountState *accountState)
{
    if (_accountState.data() == accountState) return;
    _accountState = accountState;
    _capabilitiesFetched = false;
    _deckVersion.clear();
    fetchCapabilities();
}

void DeckManager::fetchCapabilities()
{
    if (!_accountState || _capabilitiesFetched) return;
    _capabilitiesFetched = true;

    OcsDavClient::ocsRequest(_accountState, "GET",
        QStringLiteral("/ocs/v2.php/cloud/capabilities"), {},
        [this](const QJsonValue &payload, int) {
            const auto capabilities = payload.toObject()
                .value(QStringLiteral("capabilities")).toObject();
            const auto deck = capabilities.value(QStringLiteral("deck")).toObject();
            _deckVersion = deck.value(QStringLiteral("version")).toString();
            const auto major = versionComponent(_deckVersion, 0);
            const auto minor = versionComponent(_deckVersion, 1);
            _ownerAsString = major > 1 || (major == 1 && minor >= 17);
            _startDate = major > 1 || (major == 1 && minor >= 18);
            _cardColor = _startDate;
            qCInfo(lcDeckManager) << "Deck capabilities:" << _deckVersion
                                  << "ownerAsString:" << _ownerAsString;
            emit capabilitiesReady();
        },
        [this](int status, const QString &message) {
            qCWarning(lcDeckManager) << "Capabilities fetch failed:" << status << message;
            emit capabilitiesReady();
        });
}

QJsonArray DeckManager::cachedStacks(int boardId) const
{
    QFile file(cacheFilePath().arg(boardId));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).array();
}

void DeckManager::storeStacksCache(int boardId, const QJsonArray &stacks)
{
    QFile file(cacheFilePath().arg(boardId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    file.write(QJsonDocument(stacks).toJson(QJsonDocument::Compact));
}

} // namespace OCC
