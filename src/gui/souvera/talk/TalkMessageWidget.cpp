/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkMessageWidget.h"
#include <QPushButton>
#include <QDesktopServices>
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

    _nameLabel->setText(actorDisplayName.isEmpty() ? QStringLiteral("\U0001F464") : actorDisplayName);

    const auto dt = QDateTime::fromSecsSinceEpoch(timestamp);
    _timestampLabel->setText(dt.toString(QStringLiteral("HH:mm")));

    // System messages carry "{actor}"-style templates; the real names live in
    // messageParameters. Substitute them so users never see raw server code.
    const auto messageParameters = msg.value(QStringLiteral("messageParameters")).toObject();
    auto renderText = messageText;
    const auto messageType = msg.value(QStringLiteral("messageType")).toString();
    if (messageType == QLatin1String("system")) {
        const auto keys = messageParameters.keys();
        for (const auto &key : keys) {
            const auto displayName = messageParameters.value(key).toObject()
                                         .value(QStringLiteral("name")).toString();
            if (!displayName.isEmpty()) {
                renderText.replace(QLatin1Char('{') + key + QLatin1Char('}'), displayName);
            }
        }
    }

    const auto fileParam = messageParameters.value(QStringLiteral("file")).toObject();
    if (!fileParam.isEmpty()) {
        // Show any accompanying text (e.g. "X shared a file") WITH the card.
        setMessageText(renderText);
        renderAttachment(fileParam);
        return;
    }
    if (_attachmentCard) {
        _attachmentCard->deleteLater();
        _attachmentCard = nullptr;
    }
    setMessageText(renderText);

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

void TalkMessageWidget::setMessageText(const QString &text)
{
    _textLabel->setVisible(true);
    // QLabel renders HTML when the text looks like markup — enforce plain text
    // so malicious or accidental tags can never distort the chat layout.
    _textLabel->setTextFormat(Qt::PlainText);
    _textLabel->setText(text);
}

void TalkMessageWidget::renderAttachment(const QJsonObject &fileParam)
{
    if (_attachmentCard) {
        _attachmentCard->deleteLater();
        _attachmentCard = nullptr;
    }
    _textLabel->setVisible(false);

    auto *bubbleLayout = qobject_cast<QVBoxLayout *>(_bubble->layout());
    if (!bubbleLayout) return;

    const auto name = fileParam.value(QStringLiteral("name")).toString(
        QStringLiteral("Anhang"));
    const auto link = fileParam.value(QStringLiteral("link")).toString();
    const auto sizeBytes = static_cast<qint64>(fileParam.value(QStringLiteral("size")).toDouble());

    auto sizeText = QString();
    if (sizeBytes > 0) {
        if (sizeBytes >= 1024 * 1024) {
            sizeText = QStringLiteral(" \u00B7 %1 MB").arg(sizeBytes / (1024.0 * 1024.0), 0, 'f', 1);
        } else if (sizeBytes >= 1024) {
            sizeText = QStringLiteral(" \u00B7 %1 KB").arg(sizeBytes / 1024.0, 0, 'f', 0);
        } else {
            sizeText = QStringLiteral(" \u00B7 %1 B").arg(sizeBytes);
        }
    }

    _attachmentCard = new QWidget(_bubble);
    auto *cardLayout = new QHBoxLayout(_attachmentCard);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(10);

    auto *iconLabel = new QLabel(QStringLiteral("\U0001F4CE"), _attachmentCard);
    iconLabel->setStyleSheet(QStringLiteral("font-size: 22px;"));
    cardLayout->addWidget(iconLabel);

    auto *nameLabel = new QLabel(name + sizeText, _attachmentCard);
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: %1;")
        .arg(SouveraTheme::instance()->color(SouveraTheme::Color::TextPrimary).name()));
    cardLayout->addWidget(nameLabel, 1);

    auto *openBtn = new QPushButton(QStringLiteral("\u00D6ffnen"), _attachmentCard);
    openBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    openBtn->setEnabled(!link.isEmpty());
    if (!link.isEmpty()) {
        connect(openBtn, &QPushButton::clicked, this, [link]() {
            QDesktopServices::openUrl(QUrl(link));
        });
    }
    cardLayout->addWidget(openBtn);

    _attachmentCard->setStyleSheet(QStringLiteral(
        "background-color: rgba(128,128,128,40); border-radius: 8px;"));
    bubbleLayout->addWidget(_attachmentCard);
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
