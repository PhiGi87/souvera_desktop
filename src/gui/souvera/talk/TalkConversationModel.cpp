/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkConversationModel.h"

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

    switch (role) {
    case DisplayNameRole:
        return conv.value(QStringLiteral("displayName")).toString();
    case LastMessageRole: {
        const auto lastMsg = conv.value(QStringLiteral("lastMessage")).toObject();
        if (lastMsg.isEmpty()) return QStringLiteral("No messages yet");
        const auto author = lastMsg.value(QStringLiteral("actorDisplayName")).toString();
        const auto text = lastMsg.value(QStringLiteral("message")).toString();
        return QStringLiteral("%1: %2").arg(author, text);
    }
    case UnreadCountRole:
        return conv.value(QStringLiteral("unreadMessages")).toInt(0);
    case TokenRole:
        return conv.value(QStringLiteral("token")).toString();
    case IsFavoriteRole:
        return conv.value(QStringLiteral("isFavorite")).toBool(false);
    case HasUnreadMentionRole:
        return conv.value(QStringLiteral("hasUnreadMention")).toBool(false);
    default:
        return {};
    }
}

QHash<int, QByteArray> TalkConversationModel::roleNames() const
{
    return {
        { DisplayNameRole, "displayName" },
        { LastMessageRole, "lastMessage" },
        { UnreadCountRole, "unreadCount" },
        { TokenRole, "token" },
        { IsFavoriteRole, "isFavorite" },
        { HasUnreadMentionRole, "hasUnreadMention" },
    };
}

void TalkConversationModel::setConversations(const QJsonArray &conversations)
{
    beginResetModel();
    _conversations = conversations;
    endResetModel();
    qCInfo(lcTalkConvModel) << "Model updated with" << _conversations.size() << "conversations";
}

} // namespace OCC
