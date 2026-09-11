/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKMODELS_H
#define DECKMODELS_H

#include <QColor>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace OCC {

/** A Deck board member / card owner / assignee. */
class DeckUser
{
public:
    QString uid;
    QString displayName;

    static DeckUser fromJson(const QJsonObject &json)
    {
        DeckUser u;
        u.uid = json.value(QStringLiteral("primaryKey")).toString(
            json.value(QStringLiteral("uid")).toString());
        u.displayName = json.value(QStringLiteral("displayName")).toString();
        return u;
    }
};

/** A board/card label with its color. */
class DeckLabel
{
public:
    int id = -1;
    QString title;
    QColor color;

    static DeckLabel fromJson(const QJsonObject &json)
    {
        DeckLabel l;
        l.id = json.value(QStringLiteral("id")).toInt(-1);
        l.title = json.value(QStringLiteral("title")).toString();
        l.color = QColor(json.value(QStringLiteral("color")).toString());
        return l;
    }
};

/** A card attachment (file, image, …). */
class DeckAttachment
{
public:
    int id = -1;
    int cardId = -1;
    QString name;
    QString mimeType;
    QString type;
    qint64 size = 0;
    QDateTime lastModified;
    QString dirPath;

    static DeckAttachment fromJson(const QJsonObject &json)
    {
        DeckAttachment a;
        a.id = json.value(QStringLiteral("id")).toInt(-1);
        a.cardId = json.value(QStringLiteral("cardId")).toInt(-1);
        a.name = json.value(QStringLiteral("name")).toString();
        a.mimeType = json.value(QStringLiteral("mimetype")).toString();
        a.type = json.value(QStringLiteral("type")).toString();
        a.size = static_cast<qint64>(json.value(QStringLiteral("filesize")).toDouble());
        a.dirPath = json.value(QStringLiteral("dirPath")).toString();
        const auto extendedData = json.value(QStringLiteral("extendedData")).toObject();
        a.lastModified = QDateTime::fromString(
            extendedData.value(QStringLiteral("lastModifiedDate")).toString(), Qt::ISODate);
        if (!a.lastModified.isValid()) {
            a.lastModified = QDateTime::fromString(
                json.value(QStringLiteral("lastModified")).toString(), Qt::ISODate);
        }
        return a;
    }
};

/**
 * A Kanban card. Keeps the raw server JSON around so update-PUTs can return
 * the complete object the Deck API validates so strictly (missing fields
 * cause HTTP 400).
 */
class DeckCard
{
public:
    int id = -1;
    int stackId = -1;
    QString title;
    QString description;
    QString type = QStringLiteral("plain");
    int order = 0;
    bool archived = false;
    bool done = false;
    QDateTime dueDate;
    QDateTime startDate;
    QColor color;
    DeckUser owner;
    QVector<DeckUser> assignees;
    QVector<DeckLabel> labels;
    QVector<DeckAttachment> attachments;
    int commentCount = 0;
    qint64 lastModified = 0;
    QJsonObject raw;

    [[nodiscard]] static DeckCard fromJson(const QJsonObject &json)
    {
        DeckCard c;
        c.raw = json;
        c.id = json.value(QStringLiteral("id")).toInt(-1);
        c.stackId = json.value(QStringLiteral("stackId")).toInt(-1);
        c.title = json.value(QStringLiteral("title")).toString();
        c.description = json.value(QStringLiteral("description")).toString();
        c.type = json.value(QStringLiteral("type")).toString(QStringLiteral("plain"));
        c.order = json.value(QStringLiteral("order")).toInt(0);
        c.archived = json.value(QStringLiteral("archived")).toBool(false);
        c.done = json.value(QStringLiteral("done")).toBool(false);
        c.dueDate = parseDate(json.value(QStringLiteral("duedate")));
        c.startDate = parseDate(json.value(QStringLiteral("startdate")));
        c.color = QColor(json.value(QStringLiteral("color")).toString());
        c.owner = DeckUser::fromJson(json.value(QStringLiteral("owner")).toObject());
        for (const auto &v : json.value(QStringLiteral("assignedUsers")).toArray()) {
            c.assignees.append(DeckUser::fromJson(v.toObject()
                .value(QStringLiteral("participant")).toObject()));
        }
        for (const auto &v : json.value(QStringLiteral("labels")).toArray()) {
            c.labels.append(DeckLabel::fromJson(v.toObject()));
        }
        for (const auto &v : json.value(QStringLiteral("attachments")).toArray()) {
            c.attachments.append(DeckAttachment::fromJson(v.toObject()));
        }
        c.commentCount = json.value(QStringLiteral("commentsUnread")).toInt(0);
        c.lastModified = static_cast<qint64>(json.value(QStringLiteral("lastModified")).toDouble());
        return c;
    }

    /**
     * Rebuilds the complete JSON object for PUT (create/update). The Deck
     * server rejects partial objects with HTTP 400, so every field the server
     * knows about must be present.
     */
    [[nodiscard]] QJsonObject toFullJson(bool ownerAsString) const
    {
        auto body = raw;
        if (body.isEmpty()) {
            body = QJsonObject{};
        }
        body.insert(QStringLiteral("title"), title);
        body.insert(QStringLiteral("description"), description);
        body.insert(QStringLiteral("type"), type);
        body.insert(QStringLiteral("order"), order);
        body.insert(QStringLiteral("archived"), archived);
        body.insert(QStringLiteral("done"), done);
        body.insert(QStringLiteral("duedate"), serializeDate(dueDate));
        body.insert(QStringLiteral("startdate"), serializeDate(startDate));
        if (color.isValid()) {
            body.insert(QStringLiteral("color"), color.name().mid(1));
        }
        // Deck < 1.17.0 requires a nested owner object; newer versions a
        // plain uid string.
        if (ownerAsString) {
            body.insert(QStringLiteral("owner"), owner.uid);
        } else if (!owner.uid.isEmpty()) {
            QJsonObject ownerObj;
            ownerObj.insert(QStringLiteral("primaryKey"), owner.uid);
            ownerObj.insert(QStringLiteral("uid"), owner.uid);
            ownerObj.insert(QStringLiteral("displayname"), owner.displayName);
            body.insert(QStringLiteral("owner"), ownerObj);
        }
        return body;
    }

private:
    [[nodiscard]] static QDateTime parseDate(const QJsonValue &value)
    {
        if (value.isObject()) {
            const auto ms = static_cast<qint64>(value.toObject().value(QStringLiteral("timestamp")).toDouble());
            return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms * 1000) : QDateTime();
        }
        const auto str = value.toString();
        return str.isEmpty() ? QDateTime() : QDateTime::fromString(str, Qt::ISODate);
    }

    [[nodiscard]] static QJsonValue serializeDate(const QDateTime &date)
    {
        return date.isValid() ? QJsonValue(date.toString(Qt::ISODate)) : QJsonValue(QString());
    }
};

/** A card comment. */
class DeckComment
{
public:
    int id = -1;
    int cardId = -1;
    QString actorId;
    QString actorDisplayName;
    QString message;
    QDateTime creationDateTime;
    QVector<DeckComment> replies;

    [[nodiscard]] static DeckComment fromJson(const QJsonObject &json)
    {
        DeckComment c;
        c.id = json.value(QStringLiteral("id")).toInt(-1);
        c.cardId = json.value(QStringLiteral("objectId")).toInt(-1);
        c.actorId = json.value(QStringLiteral("actorId")).toString();
        c.actorDisplayName = json.value(QStringLiteral("actorDisplayName")).toString();
        c.message = json.value(QStringLiteral("message")).toString();
        c.creationDateTime = QDateTime::fromString(
            json.value(QStringLiteral("creationDateTime")).toString(), Qt::ISODate);
        return c;
    }
};

/** A column ("stack") holding cards. */
class DeckStack
{
public:
    int id = -1;
    QString title;
    int order = 0;
    bool archived = false;
    QVector<DeckCard> cards;

    [[nodiscard]] static DeckStack fromJson(const QJsonObject &json)
    {
        DeckStack s;
        s.id = json.value(QStringLiteral("id")).toInt(-1);
        s.title = json.value(QStringLiteral("title")).toString();
        s.order = json.value(QStringLiteral("order")).toInt(0);
        s.archived = json.value(QStringLiteral("archived")).toBool(false);
        for (const auto &v : json.value(QStringLiteral("cards")).toArray()) {
            s.cards.append(DeckCard::fromJson(v.toObject()));
        }
        return s;
    }
};

/** A board with its stacks, labels and members. */
class DeckBoard
{
public:
    int id = -1;
    QString title;
    QColor color;
    bool archived = false;
    bool favorite = false;
    QVector<DeckStack> stacks;
    QVector<DeckLabel> labels;
    QVector<DeckUser> members;

    [[nodiscard]] static DeckBoard fromJson(const QJsonObject &json)
    {
        DeckBoard b;
        b.id = json.value(QStringLiteral("id")).toInt(-1);
        b.title = json.value(QStringLiteral("title")).toString();
        b.color = QColor(json.value(QStringLiteral("color")).toString());
        b.archived = json.value(QStringLiteral("archived")).toBool(false);
        b.favorite = json.value(QStringLiteral("favorite")).toBool(false);
        for (const auto &v : json.value(QStringLiteral("labels")).toArray()) {
            b.labels.append(DeckLabel::fromJson(v.toObject()));
        }
        for (const auto &v : json.value(QStringLiteral("users")).toArray()) {
            b.members.append(DeckUser::fromJson(v.toObject()
                .value(QStringLiteral("participant")).toObject()));
        }
        return b;
    }
};

} // namespace OCC

#endif // DECKMODELS_H
