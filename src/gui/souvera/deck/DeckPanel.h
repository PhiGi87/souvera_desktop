/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKPANEL_H
#define DECKPANEL_H

#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QJsonObject>
#include <QQueue>
#include <QSet>
#include <QStringList>
#include <QWidget>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVector>
#include <QJsonArray>

class QLabel;
class QMimeData;
class QMenu;
class QToolButton;
class QComboBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace OCC {

class DeckLabel;
class DeckTodayView;
class DeckUser;
class DeckOcsApi;
class DeckCardWidget;
class AccountState;

/**
 * @brief A single Kanban column of the Deck board view.
 *
 * Renders the cards of one stack, supports drag&drop reordering and offers an
 * inline add row beneath the header for quick card creation.
 */
class DeckColumnWidget : public QFrame
{
    Q_OBJECT
public:
    explicit DeckColumnWidget(const QString &title, QWidget *parent = nullptr);
    ~DeckColumnWidget() override = default;

    void setStackId(int id) { _stackId = id; }
    int stackId() const { return _stackId; }

    /** Colors the 3px top border; an invalid color falls back to the accent. */
    void setBoardColor(const QColor &color);

    void addCardWidget(DeckCardWidget *card);
    void removeCardWidget(DeckCardWidget *card);
    void insertCardWidget(DeckCardWidget *card, int index);
    void clearCards();
    void updateCardCount();
    void applyColumnTheme();

signals:
    void cardDropped(int cardId, int fromStackId, int targetStackId, int insertIndex);
    void addCardRequested(int targetStackId);
    /** The inline add row submitted a non-empty title for this stack. */
    void inlineCardRequested(int stackId, const QString &title);
    void renameRequested(int stackId, const QString &newTitle);
    void deleteRequested(int stackId);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void handleDrop(const QMimeData *mime, const QPointF &pos);
    void showDropIndicator(const QPointF &pos, const QMimeData *mime);
    void hideDropIndicator();
    void showInlineEditor();
    void revertInlineEditor();

    int _stackId = -1;
    QLabel *_headerLabel = nullptr;
    QLabel *_countLabel = nullptr;
    QVBoxLayout *_cardsLayout = nullptr;
    QWidget *_scrollContainer = nullptr;
    QWidget *_dropIndicator = nullptr;
    QPushButton *_inlineAddButton = nullptr; //!< flat row button, swaps to the edit when clicked
    QLineEdit *_inlineAddEdit = nullptr; //!< inline add input, reverts on Escape or focus loss
    QColor _boardColor; //!< board color of the top border, invalid until setBoardColor()
    bool _inlineEditing = false; //!< true while the inline add edit is visible
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
    void applyHeaderTheme();
    void clearColumns();
    void renderStacks(const QJsonArray &stacks);
    void refreshStackCombo(const QJsonArray &stacks);
    void updateQuickCaptureEnabled();
    void submitQuickCapture();
    void rerenderStacks();
    void loadBoard(int boardId);
    void onNewCard();
    void onEditCard(int cardId, int stackId);
    void onDeleteCard(int cardId, int stackId);
    void onAddStack();
    void onCardDropped(int cardId, int fromStackId, int targetStackId, int insertIndex);
    void onDoneToggleRequested(int cardId, int stackId, bool done);
    void onCardSaved(int boardId, int stackId, int cardId, const QJsonObject &updatedJson);
    void openCardDialog(int boardId, const QJsonObject &cardJson);
    void setStatus(const QString &message, bool isError = false);
    [[nodiscard]] DeckCardWidget *findCard(int cardId) const;
    void connectCard(DeckCardWidget *card);
    [[nodiscard]] QMenu *buildBoardsMenu();
    [[nodiscard]] QMenu *buildFilterMenu();
    [[nodiscard]] QVector<DeckLabel> boardLabelsOf(int boardId) const;
    [[nodiscard]] QVector<DeckUser> boardMembersOf(int boardId) const;
    [[nodiscard]] QVector<DeckLabel> boardLabelsOfCurrent() const;
    [[nodiscard]] QVector<DeckUser> boardMembersOfCurrent() const;
    [[nodiscard]] bool cardMatchesFilter(const QJsonObject &card) const;
    void refreshFromServer();
    /** Queues a stacks fetch for every board missing from the DeckManager cache. */
    void startBackgroundStacksLoad();
    /** Fires queued fetches while fewer than the concurrency cap are in flight. */
    void pumpBackgroundStacksQueue();

    QScrollArea *_scrollArea = nullptr;
    QWidget *_columnsContainer = nullptr;
    QHBoxLayout *_columnsLayout = nullptr;
    QToolButton *_boardsButton = nullptr;
    QLabel *_statusLabel = nullptr;
    int _currentBoardId = -1;
    int _newCardTargetStackId = -1;
    QPushButton *_addStackButton = nullptr;
    QPushButton *_newCardButton = nullptr;
    QToolButton *_filterButton = nullptr;
    QSet<int> _filterLabelIds;
    QSet<QString> _filterAssignees;
    int _dueFilter = 0; // 0=none 1=overdue 2=today 3=this week
    bool boardFilterActive() const;
    DeckOcsApi *_ocsApi = nullptr;
    QJsonArray _boards;
    QVector<DeckColumnWidget *> _columns;

    QWidget *_viewSwitch = nullptr; //!< segmented "Board" | "Heute" container
    QPushButton *_boardViewButton = nullptr;
    QPushButton *_todayViewButton = nullptr;
    QLineEdit *_searchEdit = nullptr;
    QStackedWidget *_viewStack = nullptr; //!< page 0: board view, page 1: today view
    DeckTodayView *_todayView = nullptr;
    QWidget *_quickCaptureBar = nullptr;
    QLineEdit *_quickCaptureEdit = nullptr;
    QComboBox *_stackCombo = nullptr;
    QPushButton *_quickCaptureButton = nullptr;
    QJsonArray _lastStacks; //!< last stacks rendered; source for local re-renders
    QString _searchText;
    QQueue<int> _bgStacksQueue; //!< boards waiting for a background stacks fetch (Heute view)
    QSet<int> _bgStacksFetching; //!< boards with an in-flight background stacks fetch

    /** Extras parsed from one quick capture submit, applied once cardCreated arrives. */
    struct PendingExtras {
        bool active = false;
        int boardId = -1;
        QVector<int> labelIds;
        QStringList assigneeUids;
        QDateTime due;
    };
    PendingExtras _pendingExtras;
};

} // namespace OCC

#endif // DECKPANEL_H
