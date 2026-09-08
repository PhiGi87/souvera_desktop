/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkMessageWidget.h"
#include "theme/SouveraTheme.h"

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
    headerLayout->addWidget(_nameLabel);

    headerLayout->addStretch();

    _timestampLabel = new QLabel(_bubble);
    headerLayout->addWidget(_timestampLabel);

    bubbleLayout->addLayout(headerLayout);

    _textLabel = new QLabel(_bubble);
    _textLabel->setWordWrap(true);
    _textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubbleLayout->addWidget(_textLabel);

    if (_isOwn) {
        outerLayout->addStretch();
    }
    outerLayout->addWidget(_bubble);
    if (!_isOwn) {
        outerLayout->addStretch();
    }

    applyBubbleStyle();

    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyBubbleStyle();
    });
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
        p.setBrush(QColor::fromHsl(hue, 130, 130));
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
}

void TalkMessageWidget::applyBubbleStyle()
{
    const auto *theme = SouveraTheme::instance();

    if (_isOwn) {
        _bubble->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 12px;")
            .arg(theme->color(SouveraTheme::Color::Accent).name()));
        _nameLabel->setStyleSheet(QStringLiteral(
            "font-weight: bold; font-size: 12px; color: %1;")
            .arg(theme->color(SouveraTheme::Color::OnAccent).name()));
        _textLabel->setStyleSheet(QStringLiteral(
            "font-size: 13px; color: %1;")
            .arg(theme->color(SouveraTheme::Color::OnAccent).name()));
        _timestampLabel->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: %1;")
            .arg(theme->color(SouveraTheme::Color::OnAccent).name()));
    } else {
        _bubble->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 12px;")
            .arg(theme->color(SouveraTheme::Color::Surface).name()));
        _nameLabel->setStyleSheet(QStringLiteral(
            "font-weight: bold; font-size: 12px; color: %1;")
            .arg(theme->color(SouveraTheme::Color::TextPrimary).name()));
        _textLabel->setStyleSheet(QStringLiteral(
            "font-size: 13px; color: %1;")
            .arg(theme->color(SouveraTheme::Color::TextPrimary).name()));
        _timestampLabel->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: %1;")
            .arg(theme->color(SouveraTheme::Color::TextMuted).name()));
    }
}

} // namespace OCC
