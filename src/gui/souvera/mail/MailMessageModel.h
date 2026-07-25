/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILMESSAGEMODEL_H
#define MAILMESSAGEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QDateTime>

namespace OCC {

struct MailMessage {
    int uid = 0;
    QString from;
    QString subject;
    QDateTime dateTime;
    QString bodyHtml;
    QString bodyPlain;
    bool unread = true;
    bool hasAttachments = false;
};

class MailMessageModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        FromRole = Qt::DisplayRole,
        SubjectRole = Qt::UserRole + 1,
        DateTimeRole,
        BodyHtmlRole,
        BodyPlainRole,
        UnreadRole,
        HasAttachmentsRole,
        UidRole,
        DateDisplayRole,
        SummaryRole
    };

    explicit MailMessageModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void loadMessages(const QList<MailMessage> &messages);
    void clear();

    [[nodiscard]] const MailMessage *messageAt(int row) const;

private:
    QList<MailMessage> _messages;
};

} // namespace OCC

#endif // MAILMESSAGEMODEL_H
