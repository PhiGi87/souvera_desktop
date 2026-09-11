/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKCOMMENTSWIDGET_H
#define DECKCOMMENTSWIDGET_H

#include <QWidget>
#include <QVector>

#include "DeckModels.h"

class QLabel;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;

namespace OCC {

class DeckOcsApi;

/**
 * @brief Comment list + composer for a Deck card, with @-mention support.
 */
class DeckCommentsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeckCommentsWidget(DeckOcsApi *api, QWidget *parent = nullptr);

    void setCard(int cardId, const QVector<DeckUser> &boardMembers);
    void loadComments();

private slots:
    void onCommentsReceived(int cardId, const QVector<DeckComment> &comments);
    void onCommentCreated(int cardId);
    void sendComment();
    void updateMentionPopup(const QString &text);

private:
    void rebuildList(const QVector<DeckComment> &comments);

    DeckOcsApi *_api = nullptr;
    int _cardId = -1;
    QVector<DeckUser> _boardMembers;

    QScrollArea *_scrollArea = nullptr;
    QWidget *_listContainer = nullptr;
    QVBoxLayout *_listLayout = nullptr;
    QLineEdit *_composer = nullptr;
    QWidget *_mentionPopup = nullptr;
    QVBoxLayout *_mentionLayout = nullptr;
};

} // namespace OCC

#endif // DECKCOMMENTSWIDGET_H
