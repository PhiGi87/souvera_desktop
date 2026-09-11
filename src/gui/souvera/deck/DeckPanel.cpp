/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckPanel.h"
#include "DeckCardWidget.h"
#include "DeckManager.h"
#include "DeckModels.h"
#include "DeckOcsApi.h"
#include "theme/SouveraTheme.h"
#include "account.h"
#include "accountstate.h"

#include <QAction>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QJsonObject>
#include <QLabel>
#include <QLoggingCategory>
#include <QMimeData>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QDateTime>
#include <QGridLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace OCC {

Q_LOGGING_CATEGORY(lcDeckPanel, "souvera.deck.panel")

namespace {
constexpr auto DeckCardMimeType = "application/x-souvera-deck-card";
}

// ---------------------------------------------------------------------------
// DeckColumnWidget
// ---------------------------------------------------------------------------

DeckColumnWidget::DeckColumnWidget(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setFixedWidth(280);
    applyColumnTheme();
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, [this]() {
        applyColumnTheme();
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *headerWidget = new QWidget(this);
    headerWidget->setObjectName(QStringLiteral("DeckColumnHeader"));
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(4, 0, 4, 0);

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
    _cardsLayout->setContentsMargins(0, 0, 0, 0);
    _cardsLayout->setSpacing(6);
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

void DeckColumnWidget::applyColumnTheme()
{
    const auto *theme = SouveraTheme::instance();
    setStyleSheet(QStringLiteral(
        "DeckColumnWidget { background-color: %1; border-radius: 10px; border: 1px solid %2; }"
        "DeckColumnWidget QPushButton#DeckColumnAddBtn { background: transparent;"
        "  border: 1px solid %2; border-radius: 6px; color: %3; font-size: 13px; }"
        "DeckColumnWidget QPushButton#DeckColumnAddBtn:hover { background: %4; border-color: %5; }")
        .arg(theme->color(SouveraTheme::Color::Background).name(),
             theme->color(SouveraTheme::Color::Border).name(),
             theme->color(SouveraTheme::Color::TextMuted).name(),
             theme->color(SouveraTheme::Color::SurfaceHover).name(),
             theme->color(SouveraTheme::Color::Accent).name()));
    setAcceptDrops(true);
}

void DeckColumnWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String(DeckCardMimeType))) {
        event->acceptProposedAction();
    }
}

void DeckColumnWidget::dragMoveEvent(QDragMoveEvent *event)
{
    showDropIndicator(event->position());
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
        if (pos.y() < w->geometry().center().y()) break;
        ++insertIndex;
    }
    emit cardDropped(cardId, fromStackId, _stackId, insertIndex);
}

void DeckColumnWidget::showDropIndicator(const QPointF &pos)
{
    if (!_dropIndicator) {
        _dropIndicator = new QWidget(_scrollContainer);
        _dropIndicator->setFixedHeight(3);
        _dropIndicator->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 1px;")
            .arg(SouveraTheme::instance()->color(SouveraTheme::Color::Accent).name()));
        // Insert after the trailing stretch placeholder position handling:
        // addWidget appends before the stretch via insert below.
    }
    auto insertRow = 0;
    for (int i = 0; i < _cardsLayout->count() - 1; ++i) {
        auto *w = _cardsLayout->itemAt(i)->widget();
        if (!w || w == _dropIndicator) continue;
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
            showDropIndicator(static_cast<QDragMoveEvent *>(event)->position());
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
        if (_boards.isEmpty()) {
            _boardsButton->setText(QStringLiteral("Keine Boards \u2014 ist Deck aktiv?"));
            setStatus(QStringLiteral(
                "Keine Boards empfangen. Entweder hat dein Konto keine Deck-Boards, "
                "oder der Server blockiert die API. Diagnose: startup.log \u2192 'deck'"), true);
            return;
        }
        // Auto-open the first board; its title goes on the picker button.
        const auto first = _boards.first().toObject();
        _boardsButton->setText(first.value(QStringLiteral("title")).toString());
        loadBoard(first.value(QStringLiteral("id")).toInt());
    });

    connect(_ocsApi, &DeckOcsApi::stacksReceived, this, [this](const QJsonArray &stacks) {
        qCInfo(lcDeckPanel) << "Stacks received:" << stacks.size();
        DeckManager::instance()->storeStacksCache(_currentBoardId, stacks);
        renderStacks(stacks);
    });

    connect(_ocsApi, &DeckOcsApi::cardCreated, this, [this](const QJsonObject &, int boardId, int) {
        qCInfo(lcDeckPanel) << "Card created in board" << boardId;
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::cardUpdated, this, [this](const QJsonObject &, int boardId, int) {
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::cardMoved, this, [this]() {
        if (_currentBoardId >= 0) _ocsApi->fetchStacks(_currentBoardId);
    });
    connect(_ocsApi, &DeckOcsApi::stackCreated, this, [this](const QJsonObject &, int boardId) {
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::stackUpdated, this, [this](int boardId, int) {
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::stackDeleted, this, [this](int boardId, int) {
        _ocsApi->fetchStacks(boardId);
    });
    connect(_ocsApi, &DeckOcsApi::cardDeleted, this, [this](int boardId, int, int) {
        _ocsApi->fetchStacks(boardId);
    });

    connect(_ocsApi, &DeckOcsApi::apiError, this, [this](const QString &message) {
        qCWarning(lcDeckPanel) << "API error:" << message;
        setStatus(message, true);
    });
}

void DeckPanel::setAccountState(AccountState *state)
{
    _ocsApi->setAccountState(state);
    DeckManager::instance()->setAccountState(state);
    if (state && state->account()) {
        setStatus(QStringLiteral("Lade Boards\u2026"));
        loadBoards();
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

void DeckPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 8, 16, 8);

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

    _filterButton = new QToolButton(toolbar);
    _filterButton->setObjectName(QStringLiteral("DeckFilterButton"));
    _filterButton->setText(QStringLiteral("\U0001F50D Filter"));
    _filterButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    _filterButton->setPopupMode(QToolButton::InstantPopup);
    connect(_filterButton, &QToolButton::clicked, this, [this]() {
        auto *menu = buildFilterMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(_filterButton->mapToGlobal(QPoint(0, _filterButton->height())));
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

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setObjectName(QStringLiteral("PanelScroll"));

    _columnsContainer = new QWidget(_scrollArea);
    _columnsLayout = new QHBoxLayout(_columnsContainer);
    _columnsLayout->setContentsMargins(16, 12, 16, 12);
    _columnsLayout->setSpacing(12);

    _addStackButton = new QPushButton(QStringLiteral("+ Liste hinzuf\u00FCgen"), _columnsContainer);
    _addStackButton->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    _addStackButton->setFixedWidth(160);
    connect(_addStackButton, &QPushButton::clicked, this, &DeckPanel::onAddStack);
    _columnsLayout->addWidget(_addStackButton);

    _scrollArea->setWidget(_columnsContainer);
    layout->addWidget(_scrollArea, 1);
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
    clearColumns();
    for (const auto &stackVal : stacks) {
        const auto stackObj = stackVal.toObject();
        const auto stackId = stackObj[QStringLiteral("id")].toInt();
        const auto title = stackObj[QStringLiteral("title")].toString();

        auto *column = new DeckColumnWidget(title, _columnsContainer);
        column->setStackId(stackId);

        const auto cards = stackObj[QStringLiteral("cards")].toArray();
        for (const auto &cardVal : cards) {
            const auto cardObj = cardVal.toObject();
            if (boardFilterActive() && !cardMatchesFilter(cardObj)) continue;
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

        _columnsLayout->addWidget(column);
        _columns.append(column);
    }
    // Button right of the last column, stretch closes the row.
    if (_addStackButton) {
        _columnsLayout->addWidget(_addStackButton);
        _addStackButton->setVisible(true);
    }
    _columnsLayout->addStretch();
}

void DeckPanel::loadBoard(int boardId)
{
    qCInfo(lcDeckPanel) << "Loading board:" << boardId;
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
            refreshFromServer();
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
            refreshFromServer();
        });
    }

    menu->addSeparator();
    auto *reset = menu->addAction(QStringLiteral("Filter zur\u00FCcksetzen"));
    connect(reset, &QAction::triggered, this, [this]() {
        _filterLabelIds.clear();
        _filterAssignees.clear();
        _dueFilter = 0;
        refreshFromServer();
    });
    return menu;
}

QVector<DeckLabel> DeckPanel::boardLabelsOfCurrent() const
{
    for (const auto &boardVal : _boards) {
        const auto boardObj = boardVal.toObject();
        if (boardObj.value(QStringLiteral("id")).toInt() == _currentBoardId) {
            QVector<DeckLabel> labels;
            for (const auto &v : boardObj.value(QStringLiteral("labels")).toArray()) {
                labels.append(DeckLabel::fromJson(v.toObject()));
            }
            return labels;
        }
    }
    return {};
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
}

void DeckPanel::onEditCard(int cardId, int stackId)
{
    auto *card = findCard(cardId);
    if (!card || _currentBoardId < 0) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Karte bearbeiten"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *titleEdit = new QLineEdit(card->cardData().value(QStringLiteral("title")).toString(), &dialog);
    auto *descEdit = new QPlainTextEdit(
        card->cardData().value(QStringLiteral("description")).toString(), &dialog);
    descEdit->setPlaceholderText(QStringLiteral("Beschreibung (Markdown)"));
    descEdit->setFixedHeight(120);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(new QLabel(QStringLiteral("Titel"), &dialog));
    layout->addWidget(titleEdit);
    layout->addWidget(new QLabel(QStringLiteral("Beschreibung"), &dialog));
    layout->addWidget(descEdit);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) return;
    const auto newTitle = titleEdit->text().trimmed();
    if (newTitle.isEmpty()) return;
    // The Deck API rejects partial card PUTs with HTTP 400 — send back the
    // complete object (only title/description changed).
    _ocsApi->updateCardFields(_currentBoardId, stackId, card->cardData(),
                              newTitle, descEdit->toPlainText());
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
        if (sourceColumn && sourceColumn != targetColumn) {
            sourceColumn->removeCardWidget(card);
        }
        if (sourceColumn != targetColumn) {
            targetColumn->insertCardWidget(card, insertIndex);
        }
    }

    if (fromStackId == targetStackId) return;
    // The URL needs the SOURCE stack, the body carries the TARGET stack (see
    // DeckOcsApi::moveCard); order is the 0-based insertion position.
    _ocsApi->moveCard(_currentBoardId, fromStackId, targetStackId, cardId, insertIndex);
}

} // namespace OCC
