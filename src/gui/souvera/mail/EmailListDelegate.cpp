/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "EmailListDelegate.h"
#include "JmapEmailListModel.h"
#include "theme/SouveraTheme.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QPainter>

namespace OCC {

namespace {
constexpr int RowHeight = 76;
constexpr int RowHeightCompact = 58;
constexpr int Padding = 14;
constexpr int AvatarDiameter = 34;
}

QString EmailListDelegate::formatDate(const QDateTime &received)
{
    if (!received.isValid()) return QString();
    const auto now = QDateTime::currentDateTime();
    if (received.date() == now.date()) {
        return received.toString(QStringLiteral("HH:mm"));
    }
    if (received.date().year() == now.date().year()) {
        return received.toString(QStringLiteral("dd.MM."));
    }
    return received.toString(QStringLiteral("dd.MM.yy"));
}

void EmailListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    if (!index.isValid()) return;

    const auto *theme = SouveraTheme::instance();
    const bool dark = theme->theme() == SouveraTheme::Theme::Dark;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const auto &rect = option.rect;

    // Background
    if (option.state & QStyle::State_Selected) {
        auto accent = theme->color(SouveraTheme::Color::Accent);
        accent.setAlpha(dark ? 36 : 30);
        painter->fillRect(rect, accent);
    } else if (option.state & QStyle::State_MouseOver) {
        auto overlay = dark ? QColor(255, 255, 255) : QColor(0, 0, 0);
        overlay.setAlpha(dark ? 12 : 10);
        painter->fillRect(rect, overlay);
    }

    const auto fromName = index.data(JmapEmailListModel::FromNameRole).toString();
    const auto subject = index.data(JmapEmailListModel::SubjectRole).toString();
    const auto preview = index.data(JmapEmailListModel::PreviewRole).toString();
    const auto received = index.data(JmapEmailListModel::ReceivedAtRole).toDateTime();
    const auto isRead = index.data(JmapEmailListModel::IsReadRole).toBool();
    const auto isFlagged = index.data(JmapEmailListModel::IsFlaggedRole).toBool();
    const auto hasAttachment = index.data(JmapEmailListModel::HasAttachmentRole).toBool();

    const auto unread = !isRead;
    const auto dateText = formatDate(received);

    // Avatar circle with the sender initial (like the Nextcloud Mail app).
    const auto avatarCx = rect.left() + Padding + AvatarDiameter / 2;
    const auto avatarCy = rect.center().y();
    const auto hue = qHash(fromName.isEmpty() ? QStringLiteral("?") : fromName) % 360;
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor::fromHsl(hue, 120, 110));
    painter->drawEllipse(QPoint(avatarCx, avatarCy), AvatarDiameter / 2, AvatarDiameter / 2);
    {
        QFont avatarFont = option.font;
        avatarFont.setPixelSize(14);
        avatarFont.setBold(true);
        painter->setFont(avatarFont);
        painter->setPen(Qt::white);
        const auto initial = fromName.isEmpty()
            ? QStringLiteral("?") : fromName.left(1).toUpper();
        painter->drawText(QRect(avatarCx - AvatarDiameter / 2, avatarCy - AvatarDiameter / 2,
                                AvatarDiameter, AvatarDiameter),
                          Qt::AlignCenter, initial);
    }

    // Date (top right)
    QFont dateFont = option.font;
    dateFont.setPointSizeF(option.font.pointSizeF() * 0.85);
    const auto dateFm = QFontMetrics(dateFont);
    const auto dateX = rect.right() - Padding - dateFm.horizontalAdvance(dateText);
    painter->setFont(dateFont);
    painter->setPen(theme->color(SouveraTheme::Color::TextMuted));
    painter->drawText(QPoint(dateX, rect.top() + Padding + dateFm.ascent()), dateText);

    // Text starts right of the avatar; unread adds a small accent dot.
    auto textX = rect.left() + Padding + AvatarDiameter + 10;
    if (unread) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(theme->color(SouveraTheme::Color::Accent));
        painter->drawEllipse(QPoint(textX - 5, rect.top() + Padding + 8), 3, 3);
        textX += 8;
    }
    const auto textRight = rect.right() - Padding;

    // Sender (top line)
    QFont senderFont = option.font;
    senderFont.setBold(unread);
    const auto senderFm = QFontMetrics(senderFont);
    painter->setFont(senderFont);
    painter->setPen(theme->color(SouveraTheme::Color::TextPrimary));
    const auto senderText = senderFm.elidedText(fromName, Qt::ElideRight, dateX - textX - 12);
    painter->drawText(QPoint(textX, rect.top() + Padding - 2 + senderFm.ascent()), senderText);

    // Subject (second line)
    QFont subjectFont = option.font;
    subjectFont.setBold(unread);
    const auto subjectFm = QFontMetrics(subjectFont);
    const auto subjectY = rect.top() + Padding + senderFm.height() + 4 + subjectFm.ascent();
    painter->setFont(subjectFont);
    painter->setPen(theme->color(unread ? SouveraTheme::Color::TextPrimary
                                        : SouveraTheme::Color::TextSecondary));
    const auto availWidth = textRight - textX - (hasAttachment ? 26 : 0);
    const auto subjectText = subjectFm.elidedText(
        subject.isEmpty() ? QStringLiteral("(kein Betreff)") : subject, Qt::ElideRight, availWidth);
    painter->drawText(QPoint(textX, subjectY), subjectText);

    // Attachment indicator (right of subject line)
    if (hasAttachment) {
        painter->setPen(theme->color(SouveraTheme::Color::TextMuted));
        painter->drawText(QPoint(rect.right() - Padding - 18, subjectY),
                          QStringLiteral("\U0001F4CE"));
    }

    // Preview (third line) - only when enabled in the mail view settings
    if (_showPreview) {
        QFont previewFont = option.font;
        previewFont.setPointSizeF(option.font.pointSizeF() * 0.9);
        const auto previewFm = QFontMetrics(previewFont);
        const auto previewY = subjectY + 4 + previewFm.ascent();
        painter->setFont(previewFont);
        painter->setPen(theme->color(SouveraTheme::Color::TextMuted));
        const auto previewText = previewFm.elidedText(preview, Qt::ElideRight,
                                                      textRight - textX);
        painter->drawText(QPoint(textX, previewY), previewText);
    }

    // Flagged star (next to date, second line)
    if (isFlagged) {
        painter->setPen(theme->color(SouveraTheme::Color::Warning));
        painter->drawText(QPoint(dateX, subjectY), QStringLiteral("\u2605"));
    }

    painter->restore();
}

QSize EmailListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return QSize(option.rect.width(), _showPreview ? RowHeight : RowHeightCompact);
}

} // namespace OCC
