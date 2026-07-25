/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILMESSAGEMODEL_H
#define MAILMESSAGEMODEL_H

#include <QAbstractListModel>
#include <QVector>

namespace OCC {

struct MailMessage {
    QString subject;
    QString sender;
    QString date;
    QString bodyHtml;
};

class MailMessageModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit MailMessageModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void loadFolder(const QString &folderName);

private:
    QVector<MailMessage> _messages;
};

} // namespace OCC

#endif // MAILMESSAGEMODEL_H
