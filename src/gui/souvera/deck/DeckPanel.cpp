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

    layout->addWidget(headerWidget);

    _cardsLayout = new QVBoxLayout;
    _cardsLayout->setContentsMargins(0, 0, 0, 0);
    _cardsLayout->setSpacing(6);
    _cardsLayout->addStretch();
    layout->addLayout(_cardsLayout);
}

DeckCardWidget *DeckColumnWidget::addCard(const QJsonObject &cardData)
{
    const auto title = cardData[QStringLiteral("title")].toString();
    const auto description = cardData[QStringLiteral("description")].toString();

    auto *card = new DeckCardWidget(title, description, this);

    const auto labels = cardData[QStringLiteral("labels")].toArray();
    for (const auto &labelVal : labels) {
        const auto labelObj = labelVal.toObject();
        const auto labelColor = labelObj[QStringLiteral("color")].toString();
        const auto labelTitle = labelObj[QStringLiteral("title")].toString();
        card->addLabel(labelColor, labelTitle);
    }

    _cardsLayout->insertWidget(_cardsLayout->count() - 1, card);
    updateCardCount();
    return card;
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
        "DeckColumnWidget { background-color: %1; border-radius: 10px; }")
        .arg(theme->color(SouveraTheme::Color::Background).name()));
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
                column->addCard(cardVal.toObject());
            }
            column->updateCardCount();

            _columnsLayout->addWidget(column);
            _columns.append(column);
        }
        _columnsLayout->addStretch();
    });

    connect(_ocsApi, &DeckOcsApi::cardCreated, this, [this](const QJsonObject &, int boardId, int) {
        qCInfo(lcDeckPanel) << "Card created in board" << boardId;
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

void DeckPanel::loadBoard(int boardId)
{
    qCInfo(lcDeckPanel) << "Loading board:" << boardId;
    _currentBoardId = boardId;
    clearColumns();
    _ocsApi->fetchStacks(boardId);
}

void DeckPanel::onNewCard()
{
    if (_columns.isEmpty()) return;

    auto ok = false;
    const auto title = QInputDialog::getText(this,
        QStringLiteral("Neue Karte"),
        QStringLiteral("Titel der neuen Karte:"),
        QLineEdit::Normal, {}, &ok);

    if (!ok || title.isEmpty() || _currentBoardId < 0) return;

    const auto targetStackId = _columns.first()->stackId();
    _ocsApi->createCard(_currentBoardId, targetStackId, title, QString{});
}

} // namespace OCC
