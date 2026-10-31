/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKTODAYVIEW_H
#define DECKTODAYVIEW_H

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace OCC {

class DeckCard;

/**
 * @brief "Heute" focus view aggregating due and assigned cards across all
 *        boards from the DeckManager stacks cache (no network access).
 */
class DeckTodayView : public QWidget
{
    Q_OBJECT

public:
    explicit DeckTodayView(QWidget *parent = nullptr);
    ~DeckTodayView() override;

    /** Feeds the board list JSON as received by the panel's fetchBoards. */
    void setBoards(const QJsonArray &boards);
    /** Sets the current account uid (used for the "Mir zugewiesen" section). */
    void setCurrentUser(const QString &uid);
    /** Live filter: case-insensitive substring match on card title and description; empty shows all. */
    void setSearchText(const QString &text);
    /** Rebuilds all sections from DeckManager::instance()->cachedStacks(boardId) for every board given to setBoards(). Performs NO network calls. */
    void rebuildFromCache();

signals:
    /** A row was activated (click or double click). cardJson is the cached card object. */
    void cardActivated(int boardId, int stackId, int cardId, const QJsonObject &cardJson);
    /** The done circle of a row was clicked; cardJson is the complete cached card object with "done" already flipped. */
    void doneToggleRequested(int boardId, int stackId, int cardId, const QJsonObject &cardJson);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /**
     * @brief Styling bucket of a row's due chip, fixed by the section the
     *        card was assigned to during the last rebuild.
     */
    enum class DueChip { Overdue, Today, Soon, Calm };

    void applyTheme();
    void clearSections();

    [[nodiscard]] bool isAssignedToCurrentUser(const DeckCard &card) const;
    [[nodiscard]] QJsonObject makeEntry(const DeckCard &card, int boardId,
        const QString &boardTitle, int stackId, const QString &stackTitle) const;
    [[nodiscard]] QWidget *buildSection(const QString &title, const QColor &headerColor,
        const QVector<QJsonObject> &entries, DueChip chipKind);
    [[nodiscard]] QWidget *buildRow(const QJsonObject &entry, DueChip chipKind);
    [[nodiscard]] QLabel *makeElidedLabel(const QString &text, const QString &styleSheet,
        QWidget *parent);
    [[nodiscard]] QLabel *makeChip(const QString &text, const QColor &background,
        const QColor &foreground, QWidget *parent) const;
    void emitCardActivated(QWidget *row);

    QJsonArray _boards; //!< board list JSON as received from fetchBoards
    QString _currentUserUid;
    QString _searchText;

    QScrollArea *_scrollArea = nullptr;
    QWidget *_content = nullptr;
    QVBoxLayout *_contentLayout = nullptr;
    QLabel *_summaryLabel = nullptr;
    QLabel *_placeholderLabel = nullptr;
    QList<QWidget *> _sectionWidgets; //!< section widgets of the last rebuild, owned by _contentLayout
    bool _suppressNextClickRelease = false; //!< set on double click so its trailing release does not activate the card twice
};

} // namespace OCC

#endif // DECKTODAYVIEW_H
