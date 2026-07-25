/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKCONVERSATIONMODEL_H
#define TALKCONVERSATIONMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

namespace OCC {

class TalkConversationModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        DisplayNameRole = Qt::DisplayRole,
        LastMessageRole = Qt::UserRole + 1,
        UnreadCountRole,
        TokenRole,
        IsFavoriteRole,
        HasUnreadMentionRole,
    };

    explicit TalkConversationModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setConversations(const QJsonArray &conversations);

private:
    QJsonArray _conversations;
};

} // namespace OCC

#endif // TALKCONVERSATIONMODEL_H
