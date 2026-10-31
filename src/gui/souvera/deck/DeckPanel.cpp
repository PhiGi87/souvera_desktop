/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckPanel.h"
#include "theme/SouveraMetrics.h"
#include "DeckCardDetailDialog.h"
#include "DeckCardWidget.h"
#include "DeckManager.h"
#include "DeckModels.h"
#include "DeckOcsApi.h"
#include "DeckTodayView.h"
#include "net/SouveraAccountGate.h"
#include "theme/SouveraTheme.h"
#include "account.h"
#include "accountstate.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLineEdit>
#include <QJsonObject>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QMimeData>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QDateTime>
#include <QGridLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace OCC {

namespace Metrics = Sou::Metrics;

Q_LOGGING_CATEGORY(lcDeckPanel, "souvera.deck.panel")

namespace {
constexpr auto DeckCardMimeType = "application/x-souvera-deck-card";
// Concurrent cap for the Heute-view background stacks load; further boards
// fire as earlier responses arrive.
constexpr int MaxBackgroundStacksFetches = 4;
}

// ---------------------------------------------------------------------------
// DeckColumnWidget
// ---------------------------------------------------------------------------

DeckColumnWidget::DeckColumnWidget(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // The column stylesheet addresses this frame via object name — a bare
    // type selector would also style every QLabel (QLabel inherits QFrame)
    // and every card inside the column subtree.
    setObjectName(QStringLiteral("DeckColumnFrame"));
    setFixedWidth(280);
    applyColumnTheme();
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyColumnTheme();
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Metrics::SpacingS, Metrics::SpacingS, Metrics::SpacingS, Metrics::SpacingS);
    layout->setSpacing(Metrics::SpacingS);

    auto *headerWidget = new QWidget(this);
    headerWidget->setObjectName(QStringLiteral("DeckColumnHeader"));
    // Plain QWidgets only honour stylesheet background/border with this attr —
    // without it the header renders as an unstyled white bar.
    headerWidget->setAttribute(Qt::WA_StyledBackground, true);
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(Metrics::SpacingXS, 0, Metrics::SpacingXS, 0);

    _headerLabel = new QLabel(title, headerWidget);
    _headerLabel->setObjectName(QStringLiteral("DeckColumnTitle"));
    headerLayout->addWidget(_headerLabel);

    _countLabel = new QLabel(QStringLiteral("0"), headerWidget);
    _countLabel->setObjectName(QStringLiteral("DeckColumnCount"));
    _countLabel->setFixedHeight(18);
    headerLayout->addWidget(_countLabel);
    headerLayout->addStretch();

    auto *addBtn = new QPushButton(QStringLiteral("+"), headerWidget);
    addBtn->setObjectName(QStringLiteral("DeckColumnAddBtn"));
    addBtn->setFixedSize(22, 22);
    addBtn->setToolTip(QStringLiteral("Karte in dieser Liste anlegen"));
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        emit addCardRequested(_stackId);
    });
    headerLayout->addWidget(addBtn);

    layout->addWidget(headerWidget);

    // Inline add row between header and card scroll — deliberately NOT inside
    // _cardsLayout: the drop-index and count math assumes cards-only content
    // plus the trailing stretch item.
    auto *inlineRow = new QWidget(this);
    auto *inlineLayout = new QVBoxLayout(inlineRow);
    inlineLayout->setContentsMargins(0, 0, 0, 0);
    inlineLayout->setSpacing(0);

    _inlineAddButton = new QPushButton(QStringLiteral("\uFF0B Karte hinzuf\u00FCgen\u2026"), inlineRow);
    _inlineAddButton->setObjectName(QStringLiteral("DeckInlineAddBtn"));
    _inlineAddButton->setCursor(Qt::PointingHandCursor);
    connect(_inlineAddButton, &QPushButton::clicked, this, &DeckColumnWidget::showInlineEditor);
    inlineLayout->addWidget(_inlineAddButton);

    _inlineAddEdit = new QLineEdit(inlineRow);
    _inlineAddEdit->setObjectName(QStringLiteral("DeckInlineAddEdit"));
    _inlineAddEdit->setPlaceholderText(QStringLiteral("Kartentitel\u2026"));
    _inlineAddEdit->setVisible(false);
    _inlineAddEdit->installEventFilter(this);
    connect(_inlineAddEdit, &QLineEdit::returnPressed, this, [this]() {
        const auto title = _inlineAddEdit->text().trimmed();
        if (!title.isEmpty()) {
            emit inlineCardRequested(_stackId, title);
        }
        revertInlineEditor();
    });
    inlineLayout->addWidget(_inlineAddEdit);

    layout->addWidget(inlineRow);

    // Vertical scroll area so long card lists never clip; drag&drop events
    // arrive on the scroll container and are forwarded via the event filter.
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    _scrollContainer = new QWidget(scroll);
    _scrollContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    _cardsLayout = new QVBoxLayout(_scrollContainer);
    _cardsLayout->setContentsMargins(Metrics::SpacingS, 0, Metrics::SpacingS, 0);
    _cardsLayout->setSpacing(Metrics::SpacingS);
    _cardsLayout->addStretch();

    scroll->setWidget(_scrollContainer);
    _scrollContainer->setAcceptDrops(true);
    _scrollContainer->installEventFilter(this);
    layout->addWidget(scroll, 1);
}

void DeckColumnWidget::addCardWidget(DeckCardWidget *card)
{
    _cardsLayout->insertWidget(_cardsLayout->count() - 1, card);
    updateCardCount();
}

void DeckColumnWidget::removeCardWidget(DeckCardWidget *card)
{
    _cardsLayout->removeWidget(card);
    card->setParent(nullptr);
    updateCardCount();
}

void DeckColumnWidget::insertCardWidget(DeckCardWidget *card, int index)
{
    // Layout ends with a stretch item: never insert after it.
    card->setParent(_scrollContainer);
    _cardsLayout->insertWidget(qBound(0, index, _cardsLayout->count() - 1), card);
    updateCardCount();
}

void DeckColumnWidget::clearCards()
{
    while (_cardsLayout->count() > 1) {
        auto *item = _cardsLayout->takeAt(0);
        if (item && item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    updateCardCount();
}

void DeckColumnWidget::updateCardCount()
{
    const auto count = _cardsLayout->count() - 1;
    _countLabel->setText(QString::number(count));
}

void DeckColumnWidget::setBoardColor(const QColor &color)
{
    _boardColor = color;
    applyColumnTheme();
}

void DeckColumnWidget::applyColumnTheme()
{
    const auto *theme = SouveraTheme::instance();
    const auto bg = theme->color(SouveraTheme::Color::Background).name();
    const auto border = theme->color(SouveraTheme::Color::Border).name();
    const auto borderStrong = theme->color(SouveraTheme::Color::BorderStrong).name();
    const auto textMuted = theme->color(SouveraTheme::Color::TextMuted).name();
    const auto textPrimary = theme->color(SouveraTheme::Color::TextPrimary).name();
    const auto surface = theme->color(SouveraTheme::Color::Surface).name();
    const auto surfaceHover = theme->color(SouveraTheme::Color::SurfaceHover).name();
    const auto accent = theme->color(SouveraTheme::Color::Accent).name();
    const auto topBorder = _boardColor.isValid() ? _boardColor.name() : accent;
    // One widget-level stylesheet for the whole column: a widget stylesheet
    // replaces the app stylesheet for this subtree, so every style the column
    // relies on must be restated here.
    setStyleSheet(QStringLiteral(
        "QFrame#DeckColumnFrame { background-color: %1; border-radius: 10px; border: 1px solid %2;"
        "  border-top: 3px solid %8; }"
        "QWidget#DeckColumnHeader { background: transparent; border: none;"
        "  border-bottom: 2px solid %3; }"
        "QLabel#DeckColumnTitle { color: %4; background: transparent; font-weight: 600; }"
        "QLabel#DeckColumnCount { color: %5; background: %6; }"
        "QPushButton#DeckColumnAddBtn { background: transparent;"
        "  border: 1px solid %2; border-radius: 6px; color: %5; font-size: 13px; }"
        "QPushButton#DeckColumnAddBtn:hover { background: %6; border-color: %7; }"
        "QPushButton#DeckInlineAddBtn { background: transparent; border: none;"
        "  border-radius: 6px; color: %5; text-align: left; padding: 4px 8px; }"
        "QPushButton#DeckInlineAddBtn:hover { background: %6; }"
        "QLineEdit#DeckInlineAddEdit { background: %9; border: 1px solid %2; border-radius: 8px;"
        "  color: %4; padding: 4px 8px; selection-background-color: %7; selection-color: %4; }"
        "QLineEdit#DeckInlineAddEdit:focus { border-color: %7; }")
        .arg(bg, border, borderStrong, textPrimary, textMuted, surfaceHover, accent, topBorder, surface));
    setAcceptDrops(true);
}

void DeckColumnWidget::showInlineEditor()
{
    _inlineEditing = true;
    _inlineAddButton->setVisible(false);
    _inlineAddEdit->setVisible(true);
    _inlineAddEdit->setFocus();
}

void DeckColumnWidget::revertInlineEditor()
{
    if (!_inlineEditing) return;
    _inlineEditing = false;
    _inlineAddEdit->clear();
    _inlineAddEdit->setVisible(false);
    _inlineAddButton->setVisible(true);
}

void DeckColumnWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String(DeckCardMimeType))) {
        event->acceptProposedAction();
    }
}

void DeckColumnWidget::dragMoveEvent(QDragMoveEvent *event)
{
    showDropIndicator(event->position(), event->mimeData());
    event->acceptProposedAction();
}

void DeckColumnWidget::dropEvent(QDropEvent *event)
{
    hideDropIndicator();
    handleDrop(event->mimeData(), event->position());
    event->acceptProposedAction();
}

void DeckColumnWidget::handleDrop(const QMimeData *mime, const QPointF &pos)
{
    const auto data = mime->data(QLatin1String(DeckCardMimeType));
    const auto parts = QString::fromUtf8(data).split(QLatin1Char(':'));
    if (parts.size() != 2) return;
    const auto cardId = parts.at(0).toInt();
    const auto fromStackId = parts.at(1).toInt();
    if (cardId <= 0) return;

    // Compute the 0-based insertion index from the cursor position: the drop
    // goes before the first card whose center lies below the cursor.
    auto insertIndex = 0;
    for (int i = 0; i < _cardsLayout->count() - 1; ++i) {
        auto *w = _cardsLayout->itemAt(i)->widget();
        if (!w || w == _dropIndicator) continue;
        if (auto *cardWidget = qobject_cast<DeckCardWidget *>(w)) {
            if (cardWidget->cardId() == cardId) continue;
        }
        if (pos.y() < w->geometry().center().y()) break;
        ++insertIndex;
    }
    emit cardDropped(cardId, fromStackId, _stackId, insertIndex);
}

void DeckColumnWidget::showDropIndicator(const QPointF &pos, const QMimeData *mime)
{
    if (!_dropIndicator) {
        _dropIndicator = new QWidget(_scrollContainer);
        _dropIndicator->setFixedHeight(3);
        _dropIndicator->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 1px;")
            .arg(SouveraTheme::instance()->color(SouveraTheme::Color::Accent).name()));
    }
    // Exclude the dragged card so the indicator matches the final position.
    auto draggedCardId = -1;
    if (mime) {
        const auto parts = QString::fromUtf8(
            mime->data(QStringLiteral("application/x-souvera-deck-card")))
            .split(QLatin1Char(':'));
        if (parts.size() == 2) draggedCardId = parts.at(0).toInt();
    }
    auto insertRow = 0;
    for (int i = 0; i < _cardsLayout->count() - 1; ++i) {
        auto *w = _cardsLayout->itemAt(i)->widget();
        if (!w || w == _dropIndicator) continue;
        if (auto *cardWidget = qobject_cast<DeckCardWidget *>(w)) {
            if (cardWidget->cardId() == draggedCardId) continue;
        }
        if (pos.y() < w->geometry().center().y()) break;
        ++insertRow;
    }
    _cardsLayout->insertWidget(insertRow, _dropIndicator);
    _dropIndicator->setVisible(true);
}

void DeckColumnWidget::hideDropIndicator()
{
    if (_dropIndicator && _dropIndicator->parentWidget() == _scrollContainer) {
        _cardsLayout->removeWidget(_dropIndicator);
    }
    if (_dropIndicator) {
        _dropIndicator->hide();
    }
}

bool DeckColumnWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _inlineAddEdit) {
        switch (event->type()) {
        case QEvent::FocusOut:
            revertInlineEditor();
            break;
        case QEvent::KeyPress:
            if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
                revertInlineEditor();
                return true;
            }
            break;
        default:
            break;
        }
    }
    if (watched == _scrollContainer) {
        switch (event->type()) {
        case QEvent::DragEnter: {
            auto *e = static_cast<QDragEnterEvent *>(event);
            if (e->mimeData()->hasFormat(QLatin1String(DeckCardMimeType))) {
                e->acceptProposedAction();
                return true;
            }
            break;
        }
        case QEvent::DragMove:
            showDropIndicator(static_cast<QDragMoveEvent *>(event)->position(),
                              static_cast<QDragMoveEvent *>(event)->mimeData());
            static_cast<QDragMoveEvent *>(event)->acceptProposedAction();
            return true;
        case QEvent::DragLeave:
            hideDropIndicator();
            return true;
        case QEvent::Drop:
            hideDropIndicator();
            handleDrop(static_cast<QDropEvent *>(event)->mimeData(),
                       static_cast<QDropEvent *>(event)->position());
            static_cast<QDropEvent *>(event)->acceptProposedAction();
            return true;
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// DeckPanel
// ---------------------------------------------------------------------------

DeckPanel::DeckPanel(QWidget *parent)
    : QWidget(parent)
{
    _ocsApi = new DeckOcsApi(this);
    setupUi();

    connect(_ocsApi, &DeckOcsApi::boardsReceived, this, [this](const QJsonArray &boards) {
        qCInfo(lcDeckPanel) << "Boards received:" << boards.size();
        setStatus(QString{});
        _boards = boards;
        _todayView->setBoards(_boards);
        _todayView->rebuildFromCache();
        // Boards arrived while the Heute view is open: load the stacks of all
        // not-yet-cached boards so it can aggregate every board.
        if (_viewStack->currentIndex() == 1) {
            startBackgroundStacksLoad();
        }
        if (_boards.isEmpty()) {
            _boardsButton->setText(QStringLiteral("Keine Boards \u2014 ist Deck aktiv?"));
            setStatus(QStringLiteral(
                "Keine Boards empfangen. Entweder hat dein Konto keine Deck-Boards, "
                "oder der Server blockiert die API. Diagnose: startup.log \u2192 'deck'"), true);
            return;
        }
        // Rebuild the picker menu now that the board list is known.
        _boardsButton->setMenu(buildBoardsMenu());
        // Auto-open the first board; its title goes on the picker button.
        const auto first = _boards.first().toObject();
        _boardsButton->setText(first.value(QStringLiteral("title")).toString());
        loadBoard(first.value(QStringLiteral("id")).toInt());
    });

    connect(_ocsApi, &DeckOcsApi::stacksReceived, this,
            [this](int boardId, const QJsonArray &stacks) {
        qCInfo(lcDeckPanel) << "Stacks received for board" << boardId << ":" << stacks.size();
        DeckManager::instance()->storeStacksCache(boardId, stacks);
        // Background responses for other boards only refresh the cache and the
        // Heute view; rendering stays exclusive to the open board.
        if (boardId == _currentBoardId) {
            renderStacks(stacks);
            setStatus(QString());
        }
        _todayView->rebuildFromCache();
        if (_bgStacksFetching.remove(boardId)) {
            pumpBackgroundStacksQueue();
        }
    });

    // Mutations can originate from the Heute view's card dialog for a board
    // that is not the open one — only refetch when the mutation really
    // happened on the open board.
    connect(_ocsApi, &DeckOcsApi::cardCreated, this,
            [this](const QJsonObject &card, int boardId, int stackId) {
        qCInfo(lcDeckPanel) << "Card created in board" << boardId;
        if (boardId != _currentBoardId) return;
        if (_pendingExtras.active && _pendingExtras.boardId == boardId) {
            // Two-phase quick capture: createCard cannot carry extras, so patch
            // the fresh server card with a complete-body PUT; the resulting
            // cardUpdated performs the refetch.
            auto body = card;
            auto labels = QJsonArray();
            for (const auto labelId : _pendingExtras.labelIds) {
                labels.append(labelId);
            }
            auto assignees = QJsonArray();
            for (const auto &uid : _pendingExtras.assigneeUids) {
                assignees.append(uid);
            }
            body.insert(QStringLiteral("labels"), labels);
            body.insert(QStringLiteral("assignedUsers"), assignees);
            body.insert(QStringLiteral("duedate"), _pendingExtras.due.isValid()
                ? QJsonValue(_pendingExtras.due.toString(Qt::ISODate))
                : QJsonValue(QString()));
            _pendingExtras = {};
            _ocsApi->updateCard(boardId, stackId,
                                card.value(QStringLiteral("id")).toInt(), body);
            return;
        }
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::cardUpdated, this, [this](const QJsonObject &, int boardId, int) {
        if (boardId != _currentBoardId) return;
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::cardMoved, this, [this]() {
        if (_currentBoardId >= 0) _ocsApi->fetchStacks(_currentBoardId);
    });
    connect(_ocsApi, &DeckOcsApi::reorderFailed, this, [this]() {
        // Self-heal: restore the authoritative state after a failed reorder
        // instead of leaving the optimistic view behind.
        if (_currentBoardId >= 0) _ocsApi->fetchStacks(_currentBoardId);
    });
    connect(_ocsApi, &DeckOcsApi::stackCreated, this, [this](const QJsonObject &, int boardId) {
        if (boardId != _currentBoardId) return;
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::stackUpdated, this, [this](int boardId, int) {
        if (boardId != _currentBoardId) return;
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::stackDeleted, this, [this](int boardId, int) {
        if (boardId != _currentBoardId) return;
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::cardDeleted, this, [this](int boardId, int, int) {
        if (boardId != _currentBoardId) return;
        _ocsApi->fetchStacks(boardId);
    });

    connect(_ocsApi, &DeckOcsApi::apiError, this, [this](const QString &message) {
        qCWarning(lcDeckPanel) << "API error:" << message;
        setStatus(message, true);
    });

    connect(_ocsApi, &DeckOcsApi::stacksFetchFailed, this, [this](int boardId) {
        // A failed background fetch must free its slot, or the queue stalls.
        if (_bgStacksFetching.remove(boardId)) {
            pumpBackgroundStacksQueue();
        }
    });

    connect(_todayView, &DeckTodayView::cardActivated, this,
            [this](int boardId, int stackId, int cardId, const QJsonObject &cardJson) {
        if (boardId == _currentBoardId && findCard(cardId)) {
            onEditCard(cardId, stackId);
            return;
        }
        openCardDialog(boardId, cardJson);
    });
    connect(_todayView, &DeckTodayView::doneToggleRequested, this,
            [this](int boardId, int stackId, int cardId, const QJsonObject &cardJson) {
        _ocsApi->updateCard(boardId, stackId, cardId, cardJson);
    });
}

void DeckPanel::setAccountState(AccountState *state)
{
    _ocsApi->setAccountState(state);
    DeckManager::instance()->setAccountState(state);
    if (state && state->account()) {
        _todayView->setCurrentUser(state->account()->davUser());
        setStatus(QStringLiteral("Lade Boards\u2026"));
        // Wait for the keychain fetch: requests fired with empty credentials
        // never complete.
        Sou::whenCredentialsReady(state, this, [this]() {
            loadBoards();
        });
    } else {
        setStatus(QStringLiteral("Kein Konto verbunden."), true);
    }
}

void DeckPanel::setStatus(const QString &message, bool isError)
{
    if (!_statusLabel) return;
    _statusLabel->setText(message);
    _statusLabel->setStyleSheet(QStringLiteral(
        "color: %1; padding: 0 8px; font-size: 11px; background: transparent;")
        .arg(isError ? QStringLiteral("#ef4444") : QStringLiteral("#64748b")));
}

void DeckPanel::loadBoards()
{
    _ocsApi->fetchBoards();
}

void DeckPanel::startBackgroundStacksLoad()
{
    const auto manager = DeckManager::instance();
    for (const auto &boardVal : _boards) {
        const auto boardId = boardVal.toObject().value(QStringLiteral("id")).toInt();
        if (_bgStacksQueue.contains(boardId) || _bgStacksFetching.contains(boardId)) continue;
        if (!manager->cachedStacks(boardId).isEmpty()) continue;
        _bgStacksQueue.enqueue(boardId);
    }
    pumpBackgroundStacksQueue();
}

void DeckPanel::pumpBackgroundStacksQueue()
{
    while (_bgStacksFetching.size() < MaxBackgroundStacksFetches && !_bgStacksQueue.isEmpty()) {
        const auto boardId = _bgStacksQueue.dequeue();
        _bgStacksFetching.insert(boardId);
        _ocsApi->fetchStacks(boardId);
    }
}

void DeckPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(Metrics::CardMargin, Metrics::SpacingS, Metrics::CardMargin, Metrics::SpacingS);

    auto *title = new QLabel(QStringLiteral("Deck"), toolbar);
    title->setObjectName(QStringLiteral("PanelTitle"));
    toolbarLayout->addWidget(title);

    toolbarLayout->addSpacing(12);

    _boardsButton = new QToolButton(toolbar);
    _boardsButton->setObjectName(QStringLiteral("DeckBoardsButton"));
    _boardsButton->setText(QStringLiteral("Board ausw\u00E4hlen\u2026"));
    _boardsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    _boardsButton->setMinimumWidth(180);
    _boardsButton->setPopupMode(QToolButton::InstantPopup);
    _boardsButton->setStyleSheet(QStringLiteral(
        "QToolButton#DeckBoardsButton { background: %1; border: 1px solid %2;"
        "  border-radius: 8px; padding: 6px 14px; font-weight: 600; font-size: 13px; }"
        "QToolButton#DeckBoardsButton:hover { border-color: %3; }")
        .arg(SouveraTheme::instance()->color(SouveraTheme::Color::Surface).name(),
             SouveraTheme::instance()->color(SouveraTheme::Color::Border).name(),
             SouveraTheme::instance()->color(SouveraTheme::Color::Accent).name()));
    _boardsButton->setMenu(buildBoardsMenu());
    toolbarLayout->addWidget(_boardsButton);

    _viewSwitch = new QWidget(toolbar);
    _viewSwitch->setObjectName(QStringLiteral("DeckViewSwitch"));
    auto *switchLayout = new QHBoxLayout(_viewSwitch);
    switchLayout->setContentsMargins(2, 2, 2, 2);
    switchLayout->setSpacing(0);

    _boardViewButton = new QPushButton(QStringLiteral("Board"), _viewSwitch);
    _boardViewButton->setObjectName(QStringLiteral("DeckViewSwitchBtn"));
    _boardViewButton->setCheckable(true);
    _boardViewButton->setChecked(true);
    _todayViewButton = new QPushButton(QStringLiteral("Heute"), _viewSwitch);
    _todayViewButton->setObjectName(QStringLiteral("DeckViewSwitchBtn"));
    _todayViewButton->setCheckable(true);
    auto *viewGroup = new QButtonGroup(_viewSwitch);
    viewGroup->setExclusive(true);
    viewGroup->addButton(_boardViewButton);
    viewGroup->addButton(_todayViewButton);
    switchLayout->addWidget(_boardViewButton);
    switchLayout->addWidget(_todayViewButton);
    toolbarLayout->addWidget(_viewSwitch);

    _searchEdit = new QLineEdit(toolbar);
    _searchEdit->setObjectName(QStringLiteral("DeckSearchEdit"));
    _searchEdit->setPlaceholderText(QStringLiteral("Karten durchsuchen\u2026"));
    _searchEdit->setClearButtonEnabled(true);
    _searchEdit->setFixedWidth(220);
    toolbarLayout->addWidget(_searchEdit);

    _filterButton = new QToolButton(toolbar);
    _filterButton->setObjectName(QStringLiteral("DeckFilterButton"));
    _filterButton->setText(QStringLiteral("\U0001F50D Filter"));
    _filterButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    _filterButton->setPopupMode(QToolButton::InstantPopup);
    // InstantPopup suppresses clicked(); a permanent menu must be assigned
    // and rebuilt on aboutToShow.
    auto *filterMenu = new QMenu(_filterButton);
    _filterButton->setMenu(filterMenu);
    connect(filterMenu, &QMenu::aboutToShow, this, [this, filterMenu]() {
        filterMenu->clear();
        filterMenu->addSection(QStringLiteral("Nach Label"));
        const auto boardLabels = boardLabelsOfCurrent();
        for (const auto &label : boardLabels) {
            auto *action = filterMenu->addAction(label.title.isEmpty()
                ? QStringLiteral("(ohne Titel)") : label.title);
            action->setCheckable(true);
            action->setChecked(_filterLabelIds.contains(label.id));
            if (label.color.isValid()) {
                QPixmap pix(10, 10);
                pix.fill(label.color);
                action->setIcon(pix);
            }
            connect(action, &QAction::triggered, this, [this, label](bool checked) {
                if (checked) {
                    _filterLabelIds.insert(label.id);
                } else {
                    _filterLabelIds.remove(label.id);
                }
                rerenderStacks();
            });
        }
        filterMenu->addSection(QStringLiteral("Nach F\u00E4lligkeit"));
        const QStringList dueNames = {
            QStringLiteral("Alle"),
            QStringLiteral("\u00DCberf\u00E4llig"),
            QStringLiteral("Heute f\u00E4llig"),
            QStringLiteral("Diese Woche f\u00E4llig")
        };
        for (int i = 0; i <= 3; ++i) {
            auto *action = filterMenu->addAction(dueNames.at(i));
            action->setCheckable(true);
            action->setChecked(_dueFilter == i);
            connect(action, &QAction::triggered, this, [this, i]() {
                _dueFilter = i;
                rerenderStacks();
            });
        }
        filterMenu->addSeparator();
        connect(filterMenu->addAction(QStringLiteral("Filter zur\u00FCcksetzen")),
                &QAction::triggered, this, [this]() {
            _filterLabelIds.clear();
            _filterAssignees.clear();
            _dueFilter = 0;
            rerenderStacks();
        });
    });
    toolbarLayout->addWidget(_filterButton);

    _statusLabel = new QLabel(toolbar);
    _statusLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    _statusLabel->setWordWrap(true);
    toolbarLayout->addWidget(_statusLabel, 1);

    toolbarLayout->addStretch();

    _newCardButton = new QPushButton(QStringLiteral("+ Neue Karte"), toolbar);
    _newCardButton->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(_newCardButton, &QPushButton::clicked, this, &DeckPanel::onNewCard);
    toolbarLayout->addWidget(_newCardButton);

    layout->addWidget(toolbar);

    _quickCaptureBar = new QWidget(this);
    _quickCaptureBar->setObjectName(QStringLiteral("DeckQuickCaptureBar"));
    auto *captureLayout = new QHBoxLayout(_quickCaptureBar);
    captureLayout->setContentsMargins(Metrics::CardMargin, Metrics::SpacingXS, Metrics::CardMargin, Metrics::SpacingXS);
    captureLayout->setSpacing(Metrics::SpacingS);

    _quickCaptureEdit = new QLineEdit(_quickCaptureBar);
    _quickCaptureEdit->setObjectName(QStringLiteral("DeckQuickCapture"));
    _quickCaptureEdit->setPlaceholderText(
        QStringLiteral("z. B. Bugfix Login #bug @anna morgen"));
    connect(_quickCaptureEdit, &QLineEdit::returnPressed, this, &DeckPanel::submitQuickCapture);
    captureLayout->addWidget(_quickCaptureEdit, 1);

    _stackCombo = new QComboBox(_quickCaptureBar);
    _stackCombo->setObjectName(QStringLiteral("DeckStackCombo"));
    _stackCombo->setMinimumWidth(180);
    captureLayout->addWidget(_stackCombo);

    _quickCaptureButton = new QPushButton(QStringLiteral("Hinzuf\u00FCgen"), _quickCaptureBar);
    _quickCaptureButton->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(_quickCaptureButton, &QPushButton::clicked, this, &DeckPanel::submitQuickCapture);
    captureLayout->addWidget(_quickCaptureButton);

    layout->addWidget(_quickCaptureBar);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setObjectName(QStringLiteral("PanelScroll"));

    _columnsContainer = new QWidget(_scrollArea);
    _columnsContainer->setObjectName(QStringLiteral("DeckColumnsArea"));
    _columnsContainer->setAttribute(Qt::WA_StyledBackground, true);
    _columnsLayout = new QHBoxLayout(_columnsContainer);
    _columnsLayout->setContentsMargins(Metrics::CardMargin, Metrics::SpacingM, Metrics::CardMargin, Metrics::SpacingM);
    _columnsLayout->setSpacing(Metrics::SpacingM);

    _addStackButton = new QPushButton(QStringLiteral("+ Liste hinzuf\u00FCgen"), _columnsContainer);
    _addStackButton->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    _addStackButton->setFixedWidth(4 * Metrics::SpacingXL);
    connect(_addStackButton, &QPushButton::clicked, this, &DeckPanel::onAddStack);
    _columnsLayout->addWidget(_addStackButton);

    _scrollArea->setWidget(_columnsContainer);

    _viewStack = new QStackedWidget(this);
    _viewStack->addWidget(_scrollArea);
    _todayView = new DeckTodayView(this);
    _viewStack->addWidget(_todayView);
    layout->addWidget(_viewStack, 1);

    connect(_boardViewButton, &QPushButton::clicked, this, [this]() {
        _viewStack->setCurrentIndex(0);
    });
    connect(_todayViewButton, &QPushButton::clicked, this, [this]() {
        _viewStack->setCurrentIndex(1);
        _todayView->rebuildFromCache();
        // Top up the cache for boards never opened so the Heute view
        // aggregates every board, not only the ones seen before.
        startBackgroundStacksLoad();
    });

    connect(_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        _searchText = text;
        _todayView->setSearchText(text);
        rerenderStacks();
    });

    applyHeaderTheme();
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged,
            this, &DeckPanel::applyHeaderTheme);

    updateQuickCaptureEnabled();

    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        _searchEdit->setFocus();
        _searchEdit->selectAll();
    });

    auto *captureShortcut = new QShortcut(QKeySequence(Qt::Key_N), this);
    captureShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(captureShortcut, &QShortcut::activated, this, [this]() {
        auto *focus = QApplication::focusWidget();
        if (qobject_cast<QLineEdit *>(focus) || qobject_cast<QPlainTextEdit *>(focus)
            || qobject_cast<QTextEdit *>(focus) || qobject_cast<QTextBrowser *>(focus)) {
            return;
        }
        _quickCaptureEdit->setFocus();
    });

    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, [this]() {
        if (auto *focus = qobject_cast<QLineEdit *>(QApplication::focusWidget())) {
            if (focus != _searchEdit && focus != _quickCaptureEdit) {
                focus->clearFocus();
                return;
            }
        }
        if (!_searchEdit->text().isEmpty()) {
            _searchEdit->clear();
        } else if (!_quickCaptureEdit->text().isEmpty()) {
            _quickCaptureEdit->clear();
        }
    });
}

void DeckPanel::applyHeaderTheme()
{
    const auto *theme = SouveraTheme::instance();
    const auto surface = theme->color(SouveraTheme::Color::Surface).name();
    const auto border = theme->color(SouveraTheme::Color::Border).name();
    const auto surfaceHover = theme->color(SouveraTheme::Color::SurfaceHover).name();
    const auto textPrimary = theme->color(SouveraTheme::Color::TextPrimary).name();
    const auto textSecondary = theme->color(SouveraTheme::Color::TextSecondary).name();
    const auto accent = theme->color(SouveraTheme::Color::Accent).name();
    const auto onAccent = theme->color(SouveraTheme::Color::OnAccent).name();

    _viewSwitch->setStyleSheet(QStringLiteral(
        "QWidget#DeckViewSwitch { background: %1; border: 1px solid %2; border-radius: 8px; }"
        "QPushButton#DeckViewSwitchBtn { background: transparent; border: none;"
        "  border-radius: 8px; color: %3; padding: 4px 14px; font-weight: 600; font-size: 12px; }"
        "QPushButton#DeckViewSwitchBtn:hover:!checked { background: %4; }"
        "QPushButton#DeckViewSwitchBtn:checked { background: %5; color: %6; }")
        .arg(surface, border, textSecondary, surfaceHover, accent, onAccent));

    const auto editStyle = QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2; border-radius: 8px;"
        "  padding: 6px 10px; color: %3; selection-background-color: %4; selection-color: %5; }"
        "QLineEdit:focus { border-color: %4; }")
        .arg(surface, border, textPrimary, accent, onAccent);
    _searchEdit->setStyleSheet(editStyle);
    _quickCaptureEdit->setStyleSheet(editStyle);
}

void DeckPanel::clearColumns()
{
    for (auto *column : _columns) {
        _columnsLayout->removeWidget(column);
        column->deleteLater();
    }
    _columns.clear();

    while (_columnsLayout->count() > 0) {
        auto *item = _columnsLayout->takeAt(0);
        delete item;
    }
}

void DeckPanel::renderStacks(const QJsonArray &stacks)
{
    _lastStacks = stacks;
    clearColumns();

    auto boardColor = QColor();
    for (const auto &boardVal : _boards) {
        const auto boardObj = boardVal.toObject();
        if (boardObj.value(QStringLiteral("id")).toInt() == _currentBoardId) {
            boardColor = QColor(boardObj.value(QStringLiteral("color")).toString());
            break;
        }
    }

    for (const auto &stackVal : stacks) {
        const auto stackObj = stackVal.toObject();
        const auto stackId = stackObj[QStringLiteral("id")].toInt();
        const auto title = stackObj[QStringLiteral("title")].toString();

        auto *column = new DeckColumnWidget(title, _columnsContainer);
        column->setStackId(stackId);
        column->setBoardColor(boardColor);

        const auto cards = stackObj[QStringLiteral("cards")].toArray();
        for (const auto &cardVal : cards) {
            const auto cardObj = cardVal.toObject();
            if (!cardMatchesFilter(cardObj)) continue;
            auto *card = new DeckCardWidget(cardObj, stackId, column);
            connectCard(card);
            column->addCardWidget(card);
        }
        column->updateCardCount();

        connect(column, &DeckColumnWidget::cardDropped,
                this, &DeckPanel::onCardDropped);
        connect(column, &DeckColumnWidget::addCardRequested, this, [this](int stackId) {
            if (_currentBoardId < 0) return;
            _newCardTargetStackId = stackId;
            onNewCard();
        });
        connect(column, &DeckColumnWidget::inlineCardRequested, this,
                [this](int stackId, const QString &title) {
            if (_currentBoardId <= 0 || title.trimmed().isEmpty()) return;
            _ocsApi->createCard(_currentBoardId, stackId, title, QString{});
        });

        _columnsLayout->addWidget(column);
        _columns.append(column);
    }
    // Button right of the last column, stretch closes the row.
    if (_addStackButton) {
        _columnsLayout->addWidget(_addStackButton);
        _addStackButton->setVisible(true);
    }
    _columnsLayout->addStretch();

    if (_currentBoardId < 0) {
        updateQuickCaptureEnabled();
    } else {
        refreshStackCombo(stacks);
    }
}

void DeckPanel::refreshStackCombo(const QJsonArray &stacks)
{
    if (!_stackCombo) return;
    const auto previous = _stackCombo->currentData().toInt();
    _stackCombo->clear();
    auto selectIndex = 0;
    for (const auto &stackVal : stacks) {
        const auto stackObj = stackVal.toObject();
        const auto stackId = stackObj.value(QStringLiteral("id")).toInt();
        _stackCombo->addItem(stackObj.value(QStringLiteral("title")).toString(), stackId);
        if (stackId == previous) selectIndex = _stackCombo->count() - 1;
    }
    _stackCombo->setCurrentIndex(selectIndex);
    _stackCombo->setEnabled(true);
    _quickCaptureEdit->setEnabled(true);
    _quickCaptureButton->setEnabled(true);
}

void DeckPanel::rerenderStacks()
{
    renderStacks(_lastStacks);
}

void DeckPanel::submitQuickCapture()
{
    if (_currentBoardId < 0 || !_quickCaptureEdit->isEnabled()) return;
    const auto targetStackId = _stackCombo->currentData().toInt();
    if (targetStackId <= 0) return;

    const auto input = _quickCaptureEdit->text();
    if (input.trimmed().isEmpty()) return;

    const auto boardLabels = boardLabelsOfCurrent();
    const auto boardMembers = boardMembersOfCurrent();

    auto labelIds = QVector<int>();
    auto assigneeUids = QStringList();
    auto keptTokens = QStringList();
    for (const auto &token : input.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        auto consumed = false;
        if (token.startsWith(QLatin1Char('#'))) {
            const auto name = token.mid(1);
            for (const auto &label : boardLabels) {
                if (QString::compare(label.title, name, Qt::CaseInsensitive) == 0) {
                    if (!labelIds.contains(label.id)) labelIds.append(label.id);
                    consumed = true;
                    break;
                }
            }
        } else if (token.startsWith(QLatin1Char('@'))) {
            const auto name = token.mid(1);
            for (const auto &member : boardMembers) {
                if (member.uid.startsWith(name, Qt::CaseInsensitive)
                    || member.displayName.startsWith(name, Qt::CaseInsensitive)) {
                    if (!assigneeUids.contains(member.uid)) assigneeUids.append(member.uid);
                    consumed = true;
                    break;
                }
            }
        }
        if (!consumed) keptTokens.append(token);
    }

    auto title = keptTokens.join(QLatin1Char(' '));
    auto due = QDateTime();
    // Whole-word due keywords anywhere in the text; "übermorgen" must come
    // first so its "morgen" part cannot win the alternation. Unicode word
    // properties are required or the \b before "ü" never matches.
    static const QRegularExpression dueRx(
        QStringLiteral("\\b(\u00FCbermorgen|morgen|heute)\\b"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    auto keywordDays = -1;
    auto matchIt = dueRx.globalMatch(title);
    while (matchIt.hasNext()) {
        const auto match = matchIt.next();
        if (keywordDays >= 0) continue;
        const auto keyword = match.captured(1).toLower();
        if (keyword == QLatin1String("heute")) {
            keywordDays = 0;
        } else if (keyword == QLatin1String("morgen")) {
            keywordDays = 1;
        } else if (keyword == QLatin1String("\u00FCbermorgen")) {
            keywordDays = 2;
        }
    }
    if (keywordDays >= 0) {
        title.remove(dueRx);
        due = QDateTime(QDate::currentDate().addDays(keywordDays), QTime(17, 0));
    }
    title = title.simplified();
    if (title.isEmpty()) return;

    _pendingExtras.active = !labelIds.isEmpty() || !assigneeUids.isEmpty() || due.isValid();
    _pendingExtras.boardId = _currentBoardId;
    _pendingExtras.labelIds = labelIds;
    _pendingExtras.assigneeUids = assigneeUids;
    _pendingExtras.due = due;

    _ocsApi->createCard(_currentBoardId, targetStackId, title, QString{});
    _quickCaptureEdit->clear();
    _quickCaptureEdit->setFocus();
}

void DeckPanel::updateQuickCaptureEnabled()
{
    const auto enabled = _currentBoardId >= 0;
    _quickCaptureEdit->setEnabled(enabled);
    _stackCombo->setEnabled(enabled);
    _quickCaptureButton->setEnabled(enabled);
    if (!enabled) {
        _stackCombo->clear();
        _stackCombo->addItem(QStringLiteral("Board ausw\u00E4hlen"));
    }
}

void DeckPanel::loadBoard(int boardId)
{
    qCInfo(lcDeckPanel) << "Loading board:" << boardId;
    // Label ids are board-specific — stale filters would hide cards randomly.
    _filterLabelIds.clear();
    _filterAssignees.clear();
    _dueFilter = 0;
    _currentBoardId = boardId;
    // Instant display from cache, then refresh from the server.
    const auto cached = DeckManager::instance()->cachedStacks(boardId);
    if (!cached.isEmpty()) {
        renderStacks(cached);
    } else {
        clearColumns();
        setStatus(QStringLiteral("Lade Board\u2026"));
    }
    _ocsApi->fetchStacks(boardId);
}

QMenu *DeckPanel::buildBoardsMenu()
{
    auto *menu = new QMenu(this);
    for (const auto &boardVal : _boards) {
        const auto boardObj = boardVal.toObject();
        const auto boardId = boardObj.value(QStringLiteral("id")).toInt();
        const auto boardTitle = boardObj.value(QStringLiteral("title")).toString();
        const auto boardColor = QColor(boardObj.value(QStringLiteral("color")).toString());

        auto *action = menu->addAction(boardTitle);
        QPixmap pix(12, 12);
        pix.fill(boardColor.isValid() ? boardColor : QColor(0x4b, 0xbf, 0xea));
        action->setIcon(pix);
        connect(action, &QAction::triggered, this, [this, boardId, boardTitle]() {
            _boardsButton->setText(boardTitle);
            loadBoard(boardId);
        });
    }
    return menu;
}

QMenu *DeckPanel::buildFilterMenu()
{
    auto *menu = new QMenu(this);

    auto *labelsMenu = menu->addMenu(QStringLiteral("Nach Label"));
    const auto boardLabels = boardLabelsOfCurrent();
    for (const auto &label : boardLabels) {
        auto *action = labelsMenu->addAction(label.title.isEmpty()
            ? QStringLiteral("(ohne Titel)") : label.title);
        action->setCheckable(true);
        action->setChecked(_filterLabelIds.contains(label.id));
        if (label.color.isValid()) {
            QPixmap pix(10, 10);
            pix.fill(label.color);
            action->setIcon(pix);
        }
        connect(action, &QAction::toggled, this, [this, label](bool checked) {
            if (checked) {
                _filterLabelIds.insert(label.id);
            } else {
                _filterLabelIds.remove(label.id);
            }
            rerenderStacks();
        });
    }
    labelsMenu->setEnabled(!boardLabels.isEmpty());

    auto *dueMenu = menu->addMenu(QStringLiteral("Nach F\u00E4lligkeit"));
    const QStringList dueNames = {
        QStringLiteral("Alle"),
        QStringLiteral("\u00DCberf\u00E4llig"),
        QStringLiteral("Heute f\u00E4llig"),
        QStringLiteral("Diese Woche f\u00E4llig")
    };
    for (int i = 0; i <= 3; ++i) {
        auto *action = dueMenu->addAction(dueNames.at(i));
        action->setCheckable(true);
        action->setChecked(_dueFilter == i);
        connect(action, &QAction::triggered, this, [this, i]() {
            _dueFilter = i;
            rerenderStacks();
        });
    }

    menu->addSeparator();
    auto *reset = menu->addAction(QStringLiteral("Filter zur\u00FCcksetzen"));
    connect(reset, &QAction::triggered, this, [this]() {
        _filterLabelIds.clear();
        _filterAssignees.clear();
        _dueFilter = 0;
        rerenderStacks();
    });
    return menu;
}

QVector<DeckUser> DeckPanel::boardMembersOf(int boardId) const
{
    for (const auto &boardVal : _boards) {
        const auto boardObj = boardVal.toObject();
        if (boardObj.value(QStringLiteral("id")).toInt() == boardId) {
            QVector<DeckUser> members;
            for (const auto &v : boardObj.value(QStringLiteral("users")).toArray()) {
                members.append(DeckUser::fromJson(v.toObject()
                    .value(QStringLiteral("participant")).toObject()));
            }
            return members;
        }
    }
    return {};
}

QVector<DeckUser> DeckPanel::boardMembersOfCurrent() const
{
    return boardMembersOf(_currentBoardId);
}

QVector<DeckLabel> DeckPanel::boardLabelsOf(int boardId) const
{
    for (const auto &boardVal : _boards) {
        const auto boardObj = boardVal.toObject();
        if (boardObj.value(QStringLiteral("id")).toInt() == boardId) {
            QVector<DeckLabel> labels;
            for (const auto &v : boardObj.value(QStringLiteral("labels")).toArray()) {
                labels.append(DeckLabel::fromJson(v.toObject()));
            }
            return labels;
        }
    }
    return {};
}

QVector<DeckLabel> DeckPanel::boardLabelsOfCurrent() const
{
    return boardLabelsOf(_currentBoardId);
}

void DeckPanel::refreshFromServer()
{
    if (_currentBoardId >= 0) {
        _ocsApi->fetchStacks(_currentBoardId);
    }
}

bool DeckPanel::boardFilterActive() const
{
    return !_filterLabelIds.isEmpty() || !_filterAssignees.isEmpty() || _dueFilter != 0;
}

bool DeckPanel::cardMatchesFilter(const QJsonObject &card) const
{
    const auto cardObj = DeckCard::fromJson(card);

    const auto needle = _searchText.trimmed();
    if (!needle.isEmpty()) {
        if (!cardObj.title.contains(needle, Qt::CaseInsensitive)
            && !cardObj.description.contains(needle, Qt::CaseInsensitive)) {
            return false;
        }
    }

    if (!_filterLabelIds.isEmpty()) {
        auto match = false;
        for (const auto &label : cardObj.labels) {
            if (_filterLabelIds.contains(label.id)) { match = true; break; }
        }
        if (!match) return false;
    }
    if (_dueFilter != 0) {
        if (!cardObj.dueDate.isValid()) return false;
        const auto now = QDateTime::currentDateTime();
        switch (_dueFilter) {
        case 1: if (cardObj.dueDate >= now) return false; break;
        case 2: if (cardObj.dueDate.date() != now.date()) return false; break;
        case 3: if (cardObj.dueDate > now.addDays(7)) return false; break;
        default: break;
        }
    }
    return true;
}

void DeckPanel::onNewCard()
{
    if (_columns.isEmpty() || _currentBoardId < 0) return;

    auto ok = false;
    const auto title = QInputDialog::getText(this,
        QStringLiteral("Neue Karte"),
        QStringLiteral("Titel der neuen Karte:"),
        QLineEdit::Normal, {}, &ok);

    if (!ok || title.isEmpty()) return;

    const auto targetStackId = _newCardTargetStackId > 0
        ? _newCardTargetStackId
        : _columns.first()->stackId();
    _newCardTargetStackId = -1;
    _ocsApi->createCard(_currentBoardId, targetStackId, title, QString{});
}

DeckCardWidget *DeckPanel::findCard(int cardId) const
{
    for (auto *column : _columns) {
        for (auto *child : column->findChildren<DeckCardWidget *>()) {
            if (child->cardId() == cardId) return child;
        }
    }
    return nullptr;
}

void DeckPanel::connectCard(DeckCardWidget *card)
{
    connect(card, &DeckCardWidget::editRequested, this, &DeckPanel::onEditCard);
    connect(card, &DeckCardWidget::deleteRequested, this, &DeckPanel::onDeleteCard);
    connect(card, &DeckCardWidget::doneToggleRequested, this,
            &DeckPanel::onDoneToggleRequested);
}

void DeckPanel::onDoneToggleRequested(int cardId, int stackId, bool done)
{
    auto *card = findCard(cardId);
    if (!card || _currentBoardId < 0) return;

    // Optimistic UI: update the widget locally, then push the complete card.
    auto body = card->cardData();
    body[QStringLiteral("done")] = done;
    card->updateFrom(body);
    _ocsApi->updateCard(_currentBoardId, stackId, cardId, body);
}

void DeckPanel::openCardDialog(int boardId, const QJsonObject &cardJson)
{
    // Premium detail dialog: markdown description, comments, attachments,
    // labels, dates and color. Saving performs the complete-card PUT.
    const auto manager = DeckManager::instance();
    DeckCardDetailDialog dialog(_ocsApi, boardId, cardJson,
                                boardLabelsOf(boardId), boardMembersOf(boardId),
                                manager->supportsStartDate(), manager->supportsCardColor(),
                                this);
    connect(&dialog, &DeckCardDetailDialog::cardSaved, this, &DeckPanel::onCardSaved);
    dialog.exec();
}

void DeckPanel::onCardSaved(int boardId, int stackId, int cardId, const QJsonObject &updatedJson)
{
    _ocsApi->updateCard(boardId, stackId, cardId, updatedJson);
}

void DeckPanel::onEditCard(int cardId, int stackId)
{
    Q_UNUSED(stackId);
    auto *card = findCard(cardId);
    if (!card || _currentBoardId < 0) return;

    openCardDialog(_currentBoardId, card->cardData());
}

void DeckPanel::onDeleteCard(int cardId, int stackId)
{
    if (_currentBoardId < 0) return;
    const auto ret = QMessageBox::question(this,
        QStringLiteral("Karte l\u00F6schen"),
        QStringLiteral("Soll diese Karte wirklich gel\u00F6scht werden?"));
    if (ret != QMessageBox::Yes) return;
    _ocsApi->deleteCard(_currentBoardId, stackId, cardId);
}

void DeckPanel::onAddStack()
{
    if (_currentBoardId < 0) return;
    auto ok = false;
    const auto title = QInputDialog::getText(this,
        QStringLiteral("Neue Liste"),
        QStringLiteral("Name der neuen Liste:"),
        QLineEdit::Normal, {}, &ok);
    if (!ok || title.isEmpty()) return;
    _ocsApi->createStack(_currentBoardId, title);
}

void DeckPanel::onCardDropped(int cardId, int fromStackId, int targetStackId, int insertIndex)
{
    if (_currentBoardId < 0) return;

    // Optimistic UI: move the card widget right away; the authoritative
    // refetch (cardMoved signal) replaces the state afterwards, and an API
    // failure also refetches so the view self-heals.
    auto *card = findCard(cardId);
    DeckColumnWidget *targetColumn = nullptr;
    for (auto *column : _columns) {
        if (column->stackId() == targetStackId) targetColumn = column;
    }
    if (card && targetColumn) {
        DeckColumnWidget *sourceColumn = nullptr;
        for (auto *column : _columns) {
            if (column->findChildren<DeckCardWidget *>().contains(card)) {
                sourceColumn = column;
                break;
            }
        }
        // Optimistic move works cross-column AND within the same column.
        if (sourceColumn) {
            sourceColumn->removeCardWidget(card);
        }
        if (targetColumn) {
            targetColumn->insertCardWidget(card, insertIndex);
        }
    }

    // Same-column sorting also has to be persisted — the reorder call is
    // required in BOTH cases. The URL needs the SOURCE stack, the body the
    // TARGET stack (see DeckOcsApi::moveCard); order = insertion position.
    _ocsApi->moveCard(_currentBoardId, fromStackId, targetStackId, cardId, insertIndex);
}

} // namespace OCC
