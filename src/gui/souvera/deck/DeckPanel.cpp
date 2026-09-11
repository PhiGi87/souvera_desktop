/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckPanel.h"
#include "DeckOcsApi.h"
#include "DeckCardWidget.h"
#include "theme/SouveraTheme.h"
#include "account.h"
#include "accountstate.h"

#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QScrollArea>
#include <QMimeData>
#include <QInputDialog>
#include <QLoggingCategory>
#include <QJsonObject>
#include <QJsonValue>

Q_LOGGING_CATEGORY(lcDeckPanel, "souvera.deck.panel")

namespace OCC {

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

void DeckColumnWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-souvera-deck-card"))) {
        event->acceptProposedAction();
    }
}

void DeckColumnWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void DeckColumnWidget::dropEvent(QDropEvent *event)
{
    handleDrop(event->mimeData());
    event->acceptProposedAction();
}

void DeckColumnWidget::handleDrop(const QMimeData *mime)
{
    const auto data = mime->data(QStringLiteral("application/x-souvera-deck-card"));
    const auto parts = QString::fromUtf8(data).split(':');
    if (parts.size() != 2) return;
    const auto cardId = parts.at(0).toInt();
    const auto fromStackId = parts.at(1).toInt();
    if (cardId <= 0) return;
    emit cardDropped(cardId, fromStackId, _stackId);
}

bool DeckColumnWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _scrollContainer) {
        switch (event->type()) {
        case QEvent::DragEnter: {
            auto *e = static_cast<QDragEnterEvent *>(event);
            if (e->mimeData()->hasFormat(QStringLiteral("application/x-souvera-deck-card"))) {
                e->acceptProposedAction();
                return true;
            }
            break;
        }
        case QEvent::DragMove:
            static_cast<QDragMoveEvent *>(event)->acceptProposedAction();
            return true;
        case QEvent::Drop:
            handleDrop(static_cast<QDropEvent *>(event)->mimeData());
            static_cast<QDropEvent *>(event)->acceptProposedAction();
            return true;
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void DeckColumnWidget::removeCard(DeckCardWidget *card)
{
    _cardsLayout->removeWidget(card);
    card->deleteLater();
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

DeckPanel::DeckPanel(QWidget *parent)
    : QWidget(parent)
{
    _ocsApi = new DeckOcsApi(this);
    setupUi();

    connect(_ocsApi, &DeckOcsApi::boardsReceived, this, [this](const QJsonArray &boards) {
        qCInfo(lcDeckPanel) << "Boards received:" << boards.size();
        setStatus(QString{});
        _boards = boards;
        _boardComboBox->clear();
        for (const auto &boardVal : boards) {
            const auto boardObj = boardVal.toObject();
            const auto title = boardObj[QStringLiteral("title")].toString();
            const auto boardId = boardObj[QStringLiteral("id")].toInt();
            _boardComboBox->addItem(title, boardId);
        }
        if (_boardComboBox->count() > 0) {
            _boardComboBox->setCurrentIndex(0);
            loadBoard(_boardComboBox->currentData().toInt());
        } else {
            _boardComboBox->setPlaceholderText(QStringLiteral("Keine Boards \u2014 ist Nextcloud Deck aktiv?"));
            setStatus(QStringLiteral(
                "Keine Boards empfangen. Entweder hat dein Konto keine Deck-Boards, "
                "oder der Server blockiert die API. Diagnose: startup.log \u2192 'deck'"), true);
        }
    });

    connect(_ocsApi, &DeckOcsApi::stacksReceived, this, [this](const QJsonArray &stacks) {
        qCInfo(lcDeckPanel) << "Stacks received:" << stacks.size();
        clearColumns();
        for (const auto &stackVal : stacks) {
            const auto stackObj = stackVal.toObject();
            const auto stackId = stackObj[QStringLiteral("id")].toInt();
            const auto title = stackObj[QStringLiteral("title")].toString();

            auto *column = new DeckColumnWidget(title, _columnsContainer);
            column->setStackId(stackId);

            const auto cards = stackObj[QStringLiteral("cards")].toArray();
            for (const auto &cardVal : cards) {
                auto *card = new DeckCardWidget(cardVal.toObject(), stackId, column);
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
        _columnsLayout->addStretch();
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

    _boardComboBox = new QComboBox(toolbar);
    _boardComboBox->setObjectName(QStringLiteral("MailSendAsCombo"));

    _statusLabel = new QLabel(toolbar);
    _statusLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    _statusLabel->setWordWrap(true);
    _boardComboBox->setMinimumWidth(200);
    _boardComboBox->setPlaceholderText(QStringLiteral("Board ausw\u00E4hlen\u2026"));
    connect(_boardComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            loadBoard(_boardComboBox->itemData(index).toInt());
        }
    });
    toolbarLayout->addWidget(_boardComboBox);
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

    auto *addStackBtn = new QPushButton(QStringLiteral("+ Liste hinzuf\u00FCgen"), _columnsContainer);
    addStackBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    addStackBtn->setFixedWidth(160);
    connect(addStackBtn, &QPushButton::clicked, this, &DeckPanel::onAddStack);
    _addStackButton = addStackBtn;
    _columnsLayout->addWidget(addStackBtn);

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

    // The add-stack button lived in the layout and was removed with it.
    if (_addStackButton) {
        _columnsLayout->addWidget(_addStackButton);
        _addStackButton->setVisible(true);
    }
}

void DeckPanel::loadBoard(int boardId)
{
    qCInfo(lcDeckPanel) << "Loading board:" << boardId;
    _currentBoardId = boardId;
    clearColumns();
    _ocsApi->fetchStacks(boardId);
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
    _ocsApi->updateCard(_currentBoardId, stackId, cardId, newTitle, descEdit->toPlainText());
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

void DeckPanel::onCardDropped(int cardId, int fromStackId, int targetStackId)
{
    if (_currentBoardId < 0 || fromStackId == targetStackId) return;
    // Append at the end of the target stack. The URL needs the SOURCE stack,
    // the body carries the TARGET stack (see DeckOcsApi::moveCard).
    _ocsApi->moveCard(_currentBoardId, fromStackId, targetStackId, cardId, 999);
}

} // namespace OCC
