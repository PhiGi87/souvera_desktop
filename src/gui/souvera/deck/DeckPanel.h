/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKPANEL_H
#define DECKPANEL_H

#include <QFrame>
#include <QWidget>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVector>
#include <QJsonArray>

class QLabel;
class QComboBox;
class QPushButton;
class QVBoxLayout;

namespace OCC {

class DeckOcsApi;
class DeckCardWidget;
class AccountState;

class DeckColumnWidget : public QFrame
{
    Q_OBJECT
public:
    explicit DeckColumnWidget(const QString &title, QWidget *parent = nullptr);
    ~DeckColumnWidget() override = default;

    void setStackId(int id) { _stackId = id; }
    int stackId() const { return _stackId; }

    DeckCardWidget *addCard(const QJsonObject &cardData);
    void removeCard(DeckCardWidget *card);
    void clearCards();
    void updateCardCount();

private:
    int _stackId = -1;
    QLabel *_headerLabel = nullptr;
    QLabel *_countLabel = nullptr;
    QVBoxLayout *_cardsLayout = nullptr;
};

class DeckPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DeckPanel(QWidget *parent = nullptr);
    ~DeckPanel() override = default;

    void setAccountState(AccountState *state);
    void loadBoards();

private:
    void setupUi();
    void clearColumns();
    void loadBoard(int boardId);
    void onNewCard();

    QScrollArea *_scrollArea = nullptr;
    QWidget *_columnsContainer = nullptr;
    QHBoxLayout *_columnsLayout = nullptr;
    QComboBox *_boardComboBox = nullptr;
    QPushButton *_newCardButton = nullptr;
    DeckOcsApi *_ocsApi = nullptr;
    QJsonArray _boards;
    QVector<DeckColumnWidget *> _columns;
};

} // namespace OCC

#endif // DECKPANEL_H
