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
class QMimeData;
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

    void addCardWidget(DeckCardWidget *card);
    void removeCardWidget(DeckCardWidget *card);
    void insertCardWidget(DeckCardWidget *card, int index);
    void clearCards();
    void updateCardCount();
    void applyColumnTheme();

signals:
    void cardDropped(int cardId, int fromStackId, int targetStackId, int insertIndex);
    void addCardRequested(int targetStackId);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void handleDrop(const QMimeData *mime, const QPointF &pos);
    void showDropIndicator(const QPointF &pos);
    void hideDropIndicator();

    int _stackId = -1;
    QLabel *_headerLabel = nullptr;
    QLabel *_countLabel = nullptr;
    QVBoxLayout *_cardsLayout = nullptr;
    QWidget *_scrollContainer = nullptr;
    QWidget *_dropIndicator = nullptr;
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
    void onEditCard(int cardId, int stackId);
    void onDeleteCard(int cardId, int stackId);
    void onAddStack();
    void onCardDropped(int cardId, int fromStackId, int targetStackId, int insertIndex);
    void setStatus(const QString &message, bool isError = false);
    [[nodiscard]] DeckCardWidget *findCard(int cardId) const;
    void connectCard(DeckCardWidget *card);

    QScrollArea *_scrollArea = nullptr;
    QWidget *_columnsContainer = nullptr;
    QHBoxLayout *_columnsLayout = nullptr;
    QComboBox *_boardComboBox = nullptr;
    QLabel *_statusLabel = nullptr;
    int _currentBoardId = -1;
    int _newCardTargetStackId = -1;
    QPushButton *_addStackButton = nullptr;
    QPushButton *_newCardButton = nullptr;
    DeckOcsApi *_ocsApi = nullptr;
    QJsonArray _boards;
    QVector<DeckColumnWidget *> _columns;
};

} // namespace OCC

#endif // DECKPANEL_H
