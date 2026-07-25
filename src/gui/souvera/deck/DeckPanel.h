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
#include <QVector>

class QLabel;

namespace OCC {

class DeckOcsApi;

class DeckColumnWidget : public QFrame
{
    Q_OBJECT
public:
    explicit DeckColumnWidget(const QString &title, QWidget *parent = nullptr);
    void addCard(const QString &title, const QString &description);
};

class DeckPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DeckPanel(QWidget *parent = nullptr);
    ~DeckPanel() override = default;

private:
    void setupUi();
    void loadDemoData();

    QScrollArea *_scrollArea = nullptr;
    QWidget *_columnsContainer = nullptr;
    QHBoxLayout *_columnsLayout = nullptr;
    DeckOcsApi *_ocsApi = nullptr;
};

} // namespace OCC

#endif // DECKPANEL_H
