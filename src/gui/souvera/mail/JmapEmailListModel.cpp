/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "JmapEmailListModel.h"

namespace OCC {

JmapEmailListModel::JmapEmailListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int JmapEmailListModel::rowCount(const QModelIndex &) const
{
    return _emails.size();
}

QVariant JmapEmailListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _emails.size()) return {};
    const auto &e = _emails.at(index.row());
    switch (role) {
    case IdRole: return e.id;
    case SubjectRole: return e.subject;
    case FromAddressRole: return e.fromAddress;
    case FromNameRole: return e.fromName.isEmpty() ? e.fromAddress : e.fromName;
    case ReceivedAtRole: return e.receivedAt;
    case IsReadRole: return e.isRead;
    case IsFlaggedRole: return e.isFlagged;
    case HasAttachmentRole: return e.hasAttachments;
    case PreviewRole: return e.preview;
    case Qt::DisplayRole: return e.subject.isEmpty() ? QLatin1String("(no subject)") : e.subject;
    default: return {};
    }
}

QHash<int, QByteArray> JmapEmailListModel::roleNames() const
{
    return {
        {IdRole, "emailId"},
        {SubjectRole, "subject"},
        {FromAddressRole, "fromAddress"},
        {FromNameRole, "fromName"},
        {ReceivedAtRole, "receivedAt"},
        {IsReadRole, "isRead"},
        {IsFlaggedRole, "isFlagged"},
        {HasAttachmentRole, "hasAttachment"},
        {PreviewRole, "preview"},
    };
}

bool JmapEmailListModel::matches(const QList<JmapEmail> &emails) const
{
    if (emails.size() != _emails.size()) return false;
    for (auto row = 0; row < _emails.size(); ++row) {
        const auto &a = _emails.at(row);
        const auto &b = emails.at(row);
        if (a.id != b.id || a.isRead != b.isRead || a.isFlagged != b.isFlagged
            || a.subject != b.subject || a.receivedAt != b.receivedAt
            || a.hasAttachments != b.hasAttachments) {
            return false;
        }
    }
    return true;
}

void JmapEmailListModel::setEmails(const QList<JmapEmail> &emails)
{
    beginResetModel();
    _emails = emails;
    endResetModel();
}

void JmapEmailListModel::removeEmail(const QString &emailId)
{
    for (int row = 0; row < _emails.size(); ++row) {
        if (_emails.at(row).id == emailId) {
            beginRemoveRows(QModelIndex(), row, row);
            _emails.removeAt(row);
            endRemoveRows();
            return;
        }
    }
}

QString JmapEmailListModel::emailIdForRow(int row) const
{
    if (row < 0 || row >= _emails.size()) return {};
    return _emails.at(row).id;
}

int JmapEmailListModel::rowForEmailId(const QString &emailId) const
{
    for (auto row = 0; row < _emails.size(); ++row) {
        if (_emails.at(row).id == emailId) return row;
    }
    return -1;
}

void JmapEmailListModel::setRead(int row, bool read)
{
    if (row < 0 || row >= _emails.size()) return;
    if (_emails[row].isRead == read) return;
    _emails[row].isRead = read;
    emit dataChanged(index(row), index(row), {IsReadRole});
}

} // namespace OCC
