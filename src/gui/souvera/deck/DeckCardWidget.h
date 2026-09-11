/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKCARDWIDGET_H
#define DECKCARDWIDGET_H

#include <QFrame>
#include <QJsonObject>

class QLabel;
class QVBoxLayout;

namespace OCC {

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
    void dragStarted(int cardId, int fromStackId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void applyTheme();
    void renderLabels();
    void renderMeta();

    QJsonObject _cardData;
    int _cardId = -1;
    int _stackId = -1;

    QLabel *_titleLabel = nullptr;
    QLabel *_descriptionLabel = nullptr;
    QWidget *_labelsContainer = nullptr;
    QVBoxLayout *_labelsLayout = nullptr;
    QWidget *_metaContainer = nullptr;
    QHBoxLayout *_metaLayout = nullptr;
    QLabel *_dueLabel = nullptr;

    QPoint _pressPos;
    bool _dragArmed = false;
};

} // namespace OCC

#endif // DECKCARDWIDGET_H
