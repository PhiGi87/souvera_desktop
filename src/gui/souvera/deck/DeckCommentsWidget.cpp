/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckCommentsWidget.h"
#include "DeckOcsApi.h"
#include "theme/SouveraTheme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPoint>
#include <QLoggingCategory>
#include <QPushButton>
#include <QTimer>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

namespace OCC {

Q_LOGGING_CATEGORY(lcDeckComments, "souvera.deck.comments")

namespace {
QString avatarInitial(const QString &name)
{
    return name.isEmpty() ? QStringLiteral("?") : name.left(1).toUpper();
}
}

DeckCommentsWidget::DeckCommentsWidget(DeckOcsApi *api, QWidget *parent)
    : QWidget(parent)
    , _api(api)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setFrameShape(QFrame::NoFrame);
    _listContainer = new QWidget(_scrollArea);
    _listLayout = new QVBoxLayout(_listContainer);
    _listLayout->setContentsMargins(0, 0, 0, 0);
    _listLayout->setSpacing(8);
    _listLayout->addStretch();
    _scrollArea->setWidget(_listContainer);
    layout->addWidget(_scrollArea, 1);

    auto *composerRow = new QWidget(this);
    auto *composerLayout = new QHBoxLayout(composerRow);
    composerLayout->setContentsMargins(0, 0, 0, 0);
    _composer = new QLineEdit(composerRow);
    _composer->setPlaceholderText(QStringLiteral("Kommentar schreiben… (@ für Erwähnungen)"));
    connect(_composer, &QLineEdit::textChanged, this, &DeckCommentsWidget::updateMentionPopup);
    connect(_composer, &QLineEdit::returnPressed, this, &DeckCommentsWidget::sendComment);
    composerLayout->addWidget(_composer, 1);

    auto *sendBtn = new QPushButton(QStringLiteral("Senden"), composerRow);
    sendBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(sendBtn, &QPushButton::clicked, this, &DeckCommentsWidget::sendComment);
    composerLayout->addWidget(sendBtn);
    layout->addWidget(composerRow);

    _mentionPopup = new QWidget(this, Qt::Popup);
    _mentionLayout = new QVBoxLayout(_mentionPopup);
    _mentionLayout->setContentsMargins(4, 4, 4, 4);
    _mentionLayout->setSpacing(2);
    _mentionPopup->hide();

    if (_api) {
        connect(_api, &DeckOcsApi::commentsReceived, this,
                &DeckCommentsWidget::onCommentsReceived);
        connect(_api, &DeckOcsApi::commentCreated, this,
                &DeckCommentsWidget::onCommentCreated);
        connect(_api, &DeckOcsApi::commentDeleted, this, [this](int cardId) {
            if (cardId == _cardId) loadComments();
        });
        connect(_api, &DeckOcsApi::commentUpdated, this, [this](int cardId) {
            if (cardId == _cardId) loadComments();
        });
    }
}

void DeckCommentsWidget::setCard(int cardId, const QVector<DeckUser> &boardMembers)
{
    _cardId = cardId;
    _boardMembers = boardMembers;
    // Clear list while loading.
    rebuildList({});
    loadComments();
}

void DeckCommentsWidget::loadComments()
{
    if (_cardId > 0 && _api) {
        _api->fetchComments(_cardId);
    }
}

void DeckCommentsWidget::onCommentsReceived(int cardId, const QVector<DeckComment> &comments)
{
    if (cardId != _cardId) return;
    rebuildList(comments);
}

void DeckCommentsWidget::onCommentCreated(int cardId)
{
    if (cardId != _cardId) return;
    _composer->clear();
    loadComments();
}

void DeckCommentsWidget::rebuildList(const QVector<DeckComment> &comments)
{
    while (auto *item = _listLayout->takeAt(0)) {
        if (auto *w = item->widget()) w->deleteLater();
        delete item;
    }

    if (comments.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("Noch keine Kommentare."), _listContainer);
        empty->setStyleSheet(QStringLiteral("color: rgba(128,128,128,160);"));
        _listLayout->addWidget(empty);
    }

    const auto *theme = SouveraTheme::instance();
    for (const auto &comment : comments) {
        auto *row = new QWidget(_listContainer);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto *avatar = new QLabel(avatarInitial(comment.actorDisplayName), row);
        avatar->setFixedSize(26, 26);
        avatar->setAlignment(Qt::AlignCenter);
        const auto hue = qHash(comment.actorId) % 360;
        avatar->setStyleSheet(QStringLiteral(
            "background-color: %1; color: #ffffff; border-radius: 13px;"
            "font-size: 11px; font-weight: bold;")
            .arg(QColor::fromHsl(hue, 120, 110).name()));
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);

        auto *bubble = new QWidget(row);
        auto *bubbleLayout = new QVBoxLayout(bubble);
        bubbleLayout->setContentsMargins(8, 6, 8, 6);
        bubbleLayout->setSpacing(2);
        auto *meta = new QLabel(QStringLiteral("%1 · %2").arg(
            comment.actorDisplayName.isEmpty() ? comment.actorId : comment.actorDisplayName,
            comment.creationDateTime.isValid()
                ? comment.creationDateTime.toLocalTime().toString(QStringLiteral("dd.MM. HH:mm"))
                : QString()), bubble);
        meta->setStyleSheet(QStringLiteral("font-size: 10px; color: %1; background: transparent;")
            .arg(theme->color(SouveraTheme::Color::TextMuted).name()));
        auto *text = new QLabel(comment.message, bubble);
        text->setWordWrap(true);
        text->setTextFormat(Qt::PlainText);
        text->setStyleSheet(QStringLiteral("font-size: 12px; color: %1; background: transparent;")
            .arg(theme->color(SouveraTheme::Color::TextPrimary).name()));
        bubbleLayout->addWidget(meta);
        bubbleLayout->addWidget(text);
        bubble->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 8px;")
            .arg(theme->color(SouveraTheme::Color::SurfaceHover).name()));
        rowLayout->addWidget(bubble, 1);

        _listLayout->addWidget(row);
    }
    _listLayout->addStretch();

    // Keep the newest comment visible after (re)loads.
    QTimer::singleShot(50, this, [this]() {
        _scrollArea->verticalScrollBar()->setValue(_scrollArea->verticalScrollBar()->maximum());
    });
}

void DeckCommentsWidget::sendComment()
{
    const auto text = _composer->text().trimmed();
    if (text.isEmpty() || _cardId <= 0 || !_api) return;
    _api->createComment(_cardId, text);
}

void DeckCommentsWidget::updateMentionPopup(const QString &text)
{
    const auto cursorPos = _composer->cursorPosition();
    const auto before = text.left(cursorPos);
    const auto atPos = before.lastIndexOf(QLatin1Char('@'));
    if (atPos < 0 || before.size() - atPos > 12) {
        _mentionPopup->hide();
        return;
    }
    const auto query = before.mid(atPos + 1).toLower();

    while (auto *item = _mentionLayout->takeAt(0)) {
        if (auto *w = item->widget()) w->deleteLater();
        delete item;
    }

    auto matches = 0;
    for (const auto &member : _boardMembers) {
        const auto name = member.displayName.isEmpty() ? member.uid : member.displayName;
        if (!name.toLower().contains(query)) continue;
        auto *btn = new QPushButton(QStringLiteral("@%1 — %2").arg(member.uid, name),
                                    _mentionPopup);
        btn->setFlat(true);
        connect(btn, &QPushButton::clicked, this, [this, member, atPos]() {
            const auto t = _composer->text();
            _composer->setText(t.left(atPos) + QLatin1Char('@') + member.uid + QLatin1Char(' ')
                               + t.mid(_composer->cursorPosition()));
            _composer->setFocus();
            _mentionPopup->hide();
        });
        _mentionLayout->addWidget(btn);
        if (++matches >= 6) break;
    }

    if (matches == 0) {
        _mentionPopup->hide();
        return;
    }
    _mentionPopup->adjustSize();
    _mentionPopup->move(_composer->mapToGlobal(
        QPoint(0, -_mentionPopup->height() - 4)));
    _mentionPopup->show();
}

} // namespace OCC
