/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkPanel.h"
#include "CallWindow.h"
#include "theme/SouveraTheme.h"
#include "TalkConversationModel.h"
#include "TalkOcsApi.h"
#include "TalkMessageWidget.h"

#include "account.h"
#include "accountstate.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcTalkPanel, "souvera.talk.panel")

namespace OCC {

namespace {

// Conversation list delegate (WhatsApp-style rows) ----------------------------

class ConversationDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (!index.isValid()) return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const auto &rect = option.rect;

        const auto *theme = SouveraTheme::instance();
        const auto isDark = theme->theme() == SouveraTheme::Theme::Dark;

        if (option.state & QStyle::State_Selected) {
            auto accent = theme->color(SouveraTheme::Color::Accent);
            accent.setAlpha(isDark ? 40 : 30);
            painter->fillRect(rect, accent);
        } else if (option.state & QStyle::State_MouseOver) {
            auto overlay = isDark ? QColor(255, 255, 255) : QColor(0, 0, 0);
            overlay.setAlpha(isDark ? 10 : 8);
            painter->fillRect(rect, overlay);
        }

        const auto name = index.data(TalkConversationModel::DisplayNameRole).toString();
        const auto lastMessage = index.data(TalkConversationModel::LastMessageRole).toString();
        const auto ts = index.data(TalkConversationModel::LastTimestampRole).toLongLong();
        const auto unread = index.data(TalkConversationModel::UnreadCountRole).toInt(0);
        const auto isGroup = index.data(TalkConversationModel::IsGroupRole).toBool();
        const auto initial = index.data(TalkConversationModel::AvatarInitialRole).toString();

        // Avatar circle
        const auto avatarCx = rect.left() + 10 + 20;
        const auto avatarCy = rect.top() + rect.height() / 2;
        const auto hue = qHash(name) % 360;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor::fromHsl(hue, 120, 110));
        painter->drawEllipse(QPoint(avatarCx, avatarCy), 20, 20);
        painter->setPen(Qt::white);
        QFont avatarFont = option.font;
        avatarFont.setPixelSize(13);
        avatarFont.setBold(true);
        painter->setFont(avatarFont);
        painter->drawText(QRect(avatarCx - 20, avatarCy - 20, 40, 40), Qt::AlignCenter, initial);

        // Name
        const auto textX = avatarCx + 30;
        QFont nameFont = option.font;
        nameFont.setBold(unread > 0);
        const auto nameFm = QFontMetrics(nameFont);
        auto timeStr = QString();
        if (ts > 0) {
            const auto dt = QDateTime::fromSecsSinceEpoch(ts);
            timeStr = dt.date() == QDate::currentDate()
                ? dt.toString(QStringLiteral("HH:mm"))
                : dt.toString(QStringLiteral("dd.MM."));
        }
        QFont timeFont = option.font;
        timeFont.setPointSizeF(option.font.pointSizeF() * 0.8);
        const auto timeFm = QFontMetrics(timeFont);
        const auto timeX = rect.right() - 10 - (timeStr.isEmpty() ? 0 : timeFm.horizontalAdvance(timeStr));

        painter->setFont(nameFont);
        painter->setPen(theme->color(SouveraTheme::Color::TextPrimary));
        painter->drawText(QPoint(textX, rect.top() + 12 + nameFm.ascent()),
                          nameFm.elidedText(name, Qt::ElideRight, timeX - textX - 8));

        // Time
        if (!timeStr.isEmpty()) {
            painter->setFont(timeFont);
            painter->setPen(unread > 0 ? theme->color(SouveraTheme::Color::Accent)
                                       : theme->color(SouveraTheme::Color::TextMuted));
            painter->drawText(QPoint(timeX, rect.top() + 12 + timeFm.ascent()), timeStr);
        }

        // Last message
        QFont msgFont = option.font;
        msgFont.setPointSizeF(option.font.pointSizeF() * 0.9);
        const auto msgFm = QFontMetrics(msgFont);
        painter->setFont(msgFont);
        painter->setPen(theme->color(SouveraTheme::Color::TextSecondary));
        const auto badgeSpace = unread > 0 ? 40 : 0;
        painter->drawText(QPoint(textX, rect.top() + 14 + nameFm.height() + msgFm.ascent()),
                          msgFm.elidedText(lastMessage, Qt::ElideRight,
                                           timeX - textX - 12 - badgeSpace));

        // Unread badge
        if (unread > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(theme->color(SouveraTheme::Color::Accent));
            const auto badgeText = unread > 99 ? QStringLiteral("99+") : QString::number(unread);
            QFont badgeFont = option.font;
            badgeFont.setPixelSize(10);
            badgeFont.setBold(true);
            const auto bfm = QFontMetrics(badgeFont);
            const auto badgeWidth = qMax(bfm.horizontalAdvance(badgeText) + 12, 22);
            painter->drawRoundedRect(QRect(timeX - badgeWidth - 6, rect.bottom() - 26,
                                           badgeWidth, 18), 9, 9);
            painter->setPen(Qt::white);
            painter->setFont(badgeFont);
            painter->drawText(QRect(timeX - badgeWidth - 6, rect.bottom() - 26,
                                    badgeWidth, 18), Qt::AlignCenter, badgeText);
        }

        // Group icon hint next to the name
        if (isGroup) {
            painter->setPen(theme->color(SouveraTheme::Color::TextMuted));
            painter->setFont(timeFont);
            painter->drawText(QPoint(rect.left() + 2, rect.top() + 8),
                              QStringLiteral(""));
        }

        painter->restore();
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        return QSize(option.rect.width(), 64);
    }
};

} // namespace

TalkPanel::TalkPanel(QWidget *parent)
    : QWidget(parent)
{
    _ocsApi = new TalkOcsApi(this);
    _conversationModel = new TalkConversationModel(this);

    setupUi();

    _pollTimer = new QTimer(this);
    _pollTimer->setInterval(5000);
    connect(_pollTimer, &QTimer::timeout, this, &TalkPanel::pollMessages);

    connect(_ocsApi, &TalkOcsApi::conversationsReceived,
            this, &TalkPanel::onConversationsReceived);
    connect(_ocsApi, &TalkOcsApi::messagesReceived,
            this, &TalkPanel::onMessagesReceived);
    connect(_ocsApi, &TalkOcsApi::messageSent,
            this, [this](const QString &token) {
        if (token == _currentToken) {
            _lastKnownId = 0;
            _ocsApi->fetchMessages(_currentToken);
        }
    });
    connect(_ocsApi, &TalkOcsApi::apiError, this, [this](const QString &message) {
        setApiStatus(message, true);
    });
}

void TalkPanel::setAccountState(AccountState *state)
{
    _accountState = state;
    _ocsApi->setAccountState(state);
    if (state && state->account()) {
        _currentUserId = state->account()->davUser();
        setApiStatus(QStringLiteral("Lade Chats\u2026"));
        _ocsApi->fetchConversations();
    } else {
        setApiStatus(QStringLiteral("Kein Konto verbunden."), true);
    }
}

void TalkPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 8, 16, 8);

    auto *title = new QLabel(QStringLiteral("Link"), toolbar);
    title->setObjectName(QStringLiteral("PanelTitle"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();

    auto *refreshBtn = new QPushButton(QStringLiteral("Aktualisieren"), toolbar);
    refreshBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        _ocsApi->fetchConversations();
        if (!_currentToken.isEmpty()) {
            _lastKnownId = 0;
            _ocsApi->fetchMessages(_currentToken);
        }
    });
    toolbarLayout->addWidget(refreshBtn);

    layout->addWidget(toolbar);

    _splitter = new QSplitter(Qt::Horizontal, this);

    // ---- Conversation list (WhatsApp style) ----
    auto *conversationPanel = new QWidget(_splitter);
    conversationPanel->setObjectName(QStringLiteral("MailFolderPanel"));
    auto *conversationLayout = new QVBoxLayout(conversationPanel);
    conversationLayout->setContentsMargins(0, 0, 0, 0);
    conversationLayout->setSpacing(0);

    _conversationList = new QListView(conversationPanel);
    _conversationList->setObjectName(QStringLiteral("RemoteFilesView"));
    _conversationList->setModel(_conversationModel);
    _conversationList->setFixedWidth(280);
    _conversationList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _conversationList->setSelectionMode(QAbstractItemView::SingleSelection);
    _conversationList->setMouseTracking(true);
    _conversationList->setItemDelegate(new ConversationDelegate(_conversationList));
    conversationLayout->addWidget(_conversationList);
    _splitter->addWidget(conversationPanel);

    // ---- Chat area (header + bubbles + composer) ----
    auto *chatWidget = new QWidget(_splitter);
    auto *chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

    auto *chatHeader = new QWidget(chatWidget);
    chatHeader->setObjectName(QStringLiteral("MailToolbar"));
    auto *chatHeaderLayout = new QHBoxLayout(chatHeader);
    chatHeaderLayout->setContentsMargins(16, 8, 16, 8);
    _chatHeaderLabel = new QLabel(QStringLiteral("W\u00E4hle einen Chat"), chatHeader);
    _chatHeaderLabel->setObjectName(QStringLiteral("PanelTitle"));
    chatHeaderLayout->addWidget(_chatHeaderLabel);
    chatHeaderLayout->addStretch();
    chatLayout->addWidget(chatHeader);

    _chatScroll = new QScrollArea(chatWidget);
    _chatScroll->setObjectName(QStringLiteral("TalkChatArea"));
    _chatScroll->setWidgetResizable(true);
    _chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    _chatContainer = new QWidget(_chatScroll);
    auto *containerLayout = new QVBoxLayout(_chatContainer);
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(4);
    containerLayout->addStretch();

    auto *placeholder = new QLabel(QStringLiteral("W\u00E4hle links einen Chat aus"), _chatContainer);
    placeholder->setObjectName(QStringLiteral("PanelPlaceholder"));
    placeholder->setAlignment(Qt::AlignCenter);
    containerLayout->addWidget(placeholder);

    _chatScroll->setWidget(_chatContainer);
    chatLayout->addWidget(_chatScroll, 1);

    auto *inputBar = new QWidget(chatWidget);
    inputBar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(12, 8, 12, 8);

    _messageInput = new QTextEdit(inputBar);
    _messageInput->setObjectName(QStringLiteral("TalkMessageInput"));
    _messageInput->setPlaceholderText(QStringLiteral("Nachricht schreiben\u2026"));
    _messageInput->setFixedHeight(44);
    _messageInput->setAcceptRichText(false);
    inputLayout->addWidget(_messageInput, 1);

    auto *buttonColumn = new QVBoxLayout;
    buttonColumn->setContentsMargins(0, 0, 0, 0);
    buttonColumn->setSpacing(4);
    _sendBtn = new QPushButton(QStringLiteral("Senden"), inputBar);
    _sendBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    buttonColumn->addWidget(_sendBtn);
    _callBtn = new QPushButton(QStringLiteral("\U0001F4DE Anrufen"), inputBar);
    _callBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    buttonColumn->addWidget(_callBtn);
    inputLayout->addLayout(buttonColumn);

    chatLayout->addWidget(inputBar);

    _splitter->addWidget(chatWidget);
    _splitter->setStretchFactor(0, 0);
    _splitter->setStretchFactor(1, 1);
    _splitter->setSizes({280, 1000});

    layout->addWidget(_splitter, 1);

    _apiStatusLabel = new QLabel(this);
    _apiStatusLabel->setObjectName(QStringLiteral("MailStatusLabel"));
    _apiStatusLabel->setFixedHeight(0);
    layout->addWidget(_apiStatusLabel);

    connect(_conversationList, &QListView::clicked, this, [this]() { onConversationSelected(); });

    // Enter = send, Shift+Enter = newline
    _messageInput->installEventFilter(this);

    connect(_sendBtn, &QPushButton::clicked, this, &TalkPanel::sendMessage);
    connect(_callBtn, &QPushButton::clicked, this, &TalkPanel::startCall);
}

bool TalkPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _messageInput && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                sendMessage();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TalkPanel::setApiStatus(const QString &message, bool isError)
{
    if (!_apiStatusLabel) return;
    if (message.isEmpty()) {
        _apiStatusLabel->setFixedHeight(0);
        _apiStatusLabel->clear();
        return;
    }
    _apiStatusLabel->setFixedHeight(22);
    _apiStatusLabel->setText(message);
    _apiStatusLabel->setStyleSheet(QStringLiteral(
        "color: %1; padding: 2px 14px; font-size: 11px; background: transparent;")
        .arg(isError ? QStringLiteral("#ef4444") : QStringLiteral("#64748b")));
}

void TalkPanel::onConversationSelected()
{
    const auto idx = _conversationList->currentIndex();
    if (!idx.isValid()) return;

    const auto token = idx.data(TalkConversationModel::TokenRole).toString();
    const auto displayName = idx.data(TalkConversationModel::DisplayNameRole).toString();
    if (token.isEmpty()) return;

    qCInfo(lcTalkPanel) << "Conversation selected:" << displayName << token;

    _currentToken = token;
    _lastKnownId = 0;
    _chatHeaderLabel->setText(displayName);

    setApiStatus(QStringLiteral("Lade Nachrichten\u2026"));
    _ocsApi->fetchMessages(_currentToken);
    _pollTimer->start();
}

void TalkPanel::startCall()
{
    if (_currentToken.isEmpty() || !_accountState) return;

    const auto acc = _accountState->account();
    if (!acc) return;

    const auto displayName = _conversationList->currentIndex()
        .data(TalkConversationModel::DisplayNameRole).toString();
    auto url = acc->url();
    url.setPath(QStringLiteral("/index.php/call/") + _currentToken);

    auto *callWindow = new CallWindow(url, displayName.isEmpty() ? _currentToken : displayName, this);
    callWindow->show();
}

void TalkPanel::sendMessage()
{
    const auto text = _messageInput->toPlainText().trimmed();
    if (text.isEmpty() || _currentToken.isEmpty()) return;

    qCInfo(lcTalkPanel) << "Sending message to" << _currentToken;
    _messageInput->clear();
    _ocsApi->sendMessage(_currentToken, text);
}

void TalkPanel::pollMessages()
{
    if (_currentToken.isEmpty()) return;
    qCInfo(lcTalkPanel) << "Polling for messages in" << _currentToken;
    _ocsApi->fetchMessages(_currentToken, _lastKnownId);
}

void TalkPanel::onConversationsReceived(const QJsonArray &conversations)
{
    _conversationModel->setConversations(conversations);
    if (conversations.isEmpty()) {
        setApiStatus(QStringLiteral("Keine Chats gefunden. Ist Nextcloud Talk (Souvera Link) auf dem Server aktiv?"));
    } else {
        setApiStatus(QString());
    }
    qCInfo(lcTalkPanel) << "Conversations updated:" << conversations.size();
}

void TalkPanel::onMessagesReceived(const QJsonArray &messages, const QString &token)
{
    if (token != _currentToken) return;

    qCInfo(lcTalkPanel) << "Messages received for" << token << ":" << messages.size();

    if (messages.isEmpty()) {
        setApiStatus(QString());
        return;
    }

    rebuildChatArea(messages);

    for (const auto &msgVal : messages) {
        const auto msgObj = msgVal.toObject();
        const auto id = static_cast<qint64>(msgObj.value(QStringLiteral("id")).toDouble());
        if (id > _lastKnownId) {
            _lastKnownId = id;
        }
    }

    setApiStatus(QString());

    auto *scrollBar = _chatScroll->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void TalkPanel::rebuildChatArea(const QJsonArray &messages)
{
    auto *old = _chatScroll->takeWidget();
    if (old) {
        old->deleteLater();
    }

    _chatContainer = new QWidget(_chatScroll);
    auto *layout = new QVBoxLayout(_chatContainer);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    layout->addStretch();

    QDate lastDate;
    for (const auto &msgVal : messages) {
        const auto msgObj = msgVal.toObject();
        const auto actorId = msgObj.value(QStringLiteral("actorId")).toString();
        const auto isOwn = (actorId == _currentUserId);
        const auto timestamp = static_cast<qint64>(msgObj.value(QStringLiteral("timestamp")).toDouble());
        const auto msgDate = timestamp > 0 ? QDateTime::fromSecsSinceEpoch(timestamp).date() : QDate();

        // Date separator
        if (msgDate.isValid() && msgDate != lastDate) {
            lastDate = msgDate;
            auto *dateLabel = new QLabel(msgDate.toString(QStringLiteral("dddd, dd. MMMM yyyy")), _chatContainer);
            dateLabel->setObjectName(QStringLiteral("PanelPlaceholder"));
            dateLabel->setAlignment(Qt::AlignCenter);
            layout->addWidget(dateLabel);
        }

        auto *msgWidget = new TalkMessageWidget(isOwn, _chatContainer);
        msgWidget->setMessage(msgObj);
        layout->addWidget(msgWidget);
    }

    _chatScroll->setWidget(_chatContainer);
}

} // namespace OCC
