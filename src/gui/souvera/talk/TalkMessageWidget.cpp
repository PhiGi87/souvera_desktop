/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkMessageWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTalkMsgWidget, "souvera.talk.messagewidget")

namespace OCC {

TalkMessageWidget::TalkMessageWidget(bool isOwn, QWidget *parent)
    : QWidget(parent)
    , _isOwn(isOwn)
{
    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(4, 2, 4, 2);

    _bubble = new QWidget(this);
    _bubble->setMinimumWidth(200);
    _bubble->setMaximumWidth(400);

    auto *bubbleLayout = new QVBoxLayout(_bubble);
    bubbleLayout->setContentsMargins(10, 6, 10, 6);
    bubbleLayout->setSpacing(2);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(6);

    _avatarLabel = new QLabel(_bubble);
    _avatarLabel->setFixedSize(24, 24);
    headerLayout->addWidget(_avatarLabel);

    _nameLabel = new QLabel(_bubble);
    _nameLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
    headerLayout->addWidget(_nameLabel);

    headerLayout->addStretch();

    _timestampLabel = new QLabel(_bubble);
    _timestampLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #888;"));
    headerLayout->addWidget(_timestampLabel);

    bubbleLayout->addLayout(headerLayout);

    _textLabel = new QLabel(_bubble);
    _textLabel->setWordWrap(true);
    _textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    _textLabel->setStyleSheet(QStringLiteral("font-size: 13px;"));
    bubbleLayout->addWidget(_textLabel);

    if (_isOwn) {
        outerLayout->addStretch();
    }
    outerLayout->addWidget(_bubble);
    if (!_isOwn) {
        outerLayout->addStretch();
    }

    applyBubbleStyle();
}

void TalkMessageWidget::setMessage(const QJsonObject &msg)
{
    _messageId = static_cast<qint64>(msg.value(QStringLiteral("id")).toDouble());

    const auto actorDisplayName = msg.value(QStringLiteral("actorDisplayName")).toString();
    const auto messageText = msg.value(QStringLiteral("message")).toString();
    const auto timestamp = static_cast<qint64>(msg.value(QStringLiteral("timestamp")).toDouble());

    _nameLabel->setText(actorDisplayName);

    const auto dt = QDateTime::fromSecsSinceEpoch(timestamp);
    _timestampLabel->setText(dt.toString(QStringLiteral("HH:mm")));

    _textLabel->setText(messageText);

    QPixmap avatar(24, 24);
    avatar.fill(Qt::transparent);
    {
        QPainter p(&avatar);
        p.setRenderHint(QPainter::Antialiasing);
        const auto initial = actorDisplayName.isEmpty() ? QStringLiteral("?") : actorDisplayName.at(0).toUpper();
        const auto hue = qHash(actorDisplayName) % 360;
        p.setBrush(QColor::fromHsl(hue, 160, 140));
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 24, 24);
        p.setPen(Qt::white);
        auto font = p.font();
        font.setPixelSize(12);
        font.setBold(true);
        p.setFont(font);
        p.drawText(QRect(0, 0, 24, 24), Qt::AlignCenter, initial);
    }
    _avatarLabel->setPixmap(avatar);

    qCInfo(lcTalkMsgWidget) << "Message widget set:" << actorDisplayName << messageText;
}

void TalkMessageWidget::applyBubbleStyle()
{
    if (_isOwn) {
        _bubble->setStyleSheet(QStringLiteral(
            "background-color: #4a90d9; border-radius: 12px; color: white;"));
        _nameLabel->setStyleSheet(QStringLiteral(
            "font-weight: bold; font-size: 12px; color: rgba(255,255,255,0.85);"));
        _textLabel->setStyleSheet(QStringLiteral(
            "font-size: 13px; color: white;"));
    } else {
        _bubble->setStyleSheet(QStringLiteral(
            "background-color: #e8e8e8; border-radius: 12px; color: #111;"));
        _nameLabel->setStyleSheet(QStringLiteral(
            "font-weight: bold; font-size: 12px; color: #333;"));
        _textLabel->setStyleSheet(QStringLiteral(
            "font-size: 13px; color: #111;"));
    }
}

} // namespace OCC
