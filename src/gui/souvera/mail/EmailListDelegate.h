/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef EMAILLISTDELEGATE_H
#define EMAILLISTDELEGATE_H

#include <QStyledItemDelegate>

namespace OCC {

/**
 * @brief Renders mail list rows: sender, date, subject, preview and status dots.
 */
class EmailListDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    void setShowPreview(bool show) { _showPreview = show; }
    [[nodiscard]] bool showPreview() const { return _showPreview; }
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const override;

private:
    bool _showPreview = true;
    [[nodiscard]] static QString formatDate(const QDateTime &received);
};

} // namespace OCC

#endif // EMAILLISTDELEGATE_H
