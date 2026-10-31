/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKCARDWIDGET_H
#define DECKCARDWIDGET_H

#include <QDateTime>
#include <QFrame>
#include <QJsonObject>

class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace OCC {

/** A single Kanban card as rendered inside a Deck column. */
class DeckCardWidget : public QFrame
{
    Q_OBJECT
public:
    explicit DeckCardWidget(const QJsonObject &cardData, int stackId, QWidget *parent = nullptr);
    ~DeckCardWidget() override = default;

    [[nodiscard]] int cardId() const { return _cardId; }
    [[nodiscard]] int stackId() const { return _stackId; }
    [[nodiscard]] QJsonObject cardData() const { return _cardData; }

    void setTitle(const QString &title);
    void setDescription(const QString &description);
    void updateFrom(const QJsonObject &cardData);

signals:
    void editRequested(int cardId, int stackId);
    void deleteRequested(int cardId, int stackId);
    void doneToggleRequested(int cardId, int stackId, bool done);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyTheme();
    void styleMeta();
    void updateElides();
    void renderLabels();
    void renderMeta();

    QJsonObject _cardData;
    int _cardId = -1;
    int _stackId = -1;

    QString _titleFull; //!< untruncated title, elided into the label on resize
    QString _descriptionFull; //!< untruncated description, clamped to two lines
    QDateTime _dueDate; //!< parsed card due date, invalid when the card has none

    QLabel *_titleLabel = nullptr;
    QLabel *_descriptionLabel = nullptr;
    QWidget *_labelsContainer = nullptr;
    QHBoxLayout *_labelsLayout = nullptr;
    QWidget *_metaContainer = nullptr;
    QHBoxLayout *_metaLayout = nullptr;
    QLabel *_dueLabel = nullptr;
    QLabel *_attachLabel = nullptr;
    QLabel *_commentLabel = nullptr;
    QWidget *_avatarContainer = nullptr;
    QWidget *_doneCircle = nullptr;
    QWidget *_hoverActions = nullptr;

    QPoint _pressPos;
    bool _dragArmed = false;
};

} // namespace OCC

#endif // DECKCARDWIDGET_H
