/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkConversationModel.h"

#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTalkConvModel, "souvera.talk.conversationmodel")

namespace OCC {

TalkConversationModel::TalkConversationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TalkConversationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return _conversations.size();
}

QVariant TalkConversationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _conversations.size()) return {};
    const auto conv = _conversations.at(index.row()).toObject();
    if (role == Qt::DisplayRole) {
        return conv.value(QStringLiteral("displayName")).toString(
            conv.value(QStringLiteral("token")).toString());
    }
    if (role == Qt::UserRole) {
        return conv.value(QStringLiteral("token")).toString();
    }
    return {};
}

void TalkConversationModel::setConversations(const QJsonArray &conversations)
{
    beginResetModel();
    _conversations = conversations;
    endResetModel();
}

} // namespace OCC
