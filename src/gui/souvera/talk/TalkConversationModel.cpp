/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
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
    case DisplayNameRole: {
        auto name = conv.value(QStringLiteral("displayName")).toString();
        if (name.isEmpty()) {
            name = conv.value(QStringLiteral("token")).toString();
        }
        return name;
    }
    case LastMessageRole: {
        const auto lastMsg = conv.value(QStringLiteral("lastMessage")).toObject();
        if (lastMsg.isEmpty()) return QStringLiteral("Noch keine Nachrichten");
        const auto author = lastMsg.value(QStringLiteral("actorDisplayName")).toString();
        const auto text = lastMsg.value(QStringLiteral("message")).toString();
        const auto messageText = lastMsg.value(QStringLiteral("messageParameters")).toObject().isEmpty()
            ? text : text;
        return author.isEmpty() ? messageText : QStringLiteral("%1: %2").arg(author, messageText);
    }
    case LastTimestampRole: {
        const auto lastMsg = conv.value(QStringLiteral("lastMessage")).toObject();
        return static_cast<qint64>(lastMsg.value(QStringLiteral("timestamp")).toDouble());
    }
    case UnreadCountRole:
        return conv.value(QStringLiteral("unreadMessages")).toInt(0);
    case TokenRole:
        return conv.value(QStringLiteral("token")).toString();
    case IsFavoriteRole:
        return conv.value(QStringLiteral("isFavorite")).toBool(false);
    case HasUnreadMentionRole:
        return conv.value(QStringLiteral("hasUnreadMention")).toBool(false);
    case IsGroupRole: {
        // type: 1=one-to-one, 2=group, 3=public, 4=changelog
        return conv.value(QStringLiteral("type")).toInt(1) != 1;
    }
    case AvatarInitialRole: {
        auto name = conv.value(QStringLiteral("displayName")).toString();
        if (name.isEmpty()) name = QStringLiteral("#");
        return name.left(1).toUpper();
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> TalkConversationModel::roleNames() const
{
    return {
        { DisplayNameRole, "displayName" },
        { LastMessageRole, "lastMessage" },
        { LastTimestampRole, "lastTimestamp" },
        { UnreadCountRole, "unreadCount" },
        { TokenRole, "token" },
        { IsFavoriteRole, "isFavorite" },
        { HasUnreadMentionRole, "hasUnreadMention" },
        { IsGroupRole, "isGroup" },
        { AvatarInitialRole, "avatarInitial" },
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
