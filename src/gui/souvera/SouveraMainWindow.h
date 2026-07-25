/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SOUVERAMAINWINDOW_H
#define SOUVERAMAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QBoxLayout>
#include <QVector>

namespace OCC {

class MailPanel;
class TalkPanel;
class DeckPanel;
class CalendarPanel;

class SouveraMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit SouveraMainWindow(QWidget *parent = nullptr);
    ~SouveraMainWindow() override = default;

    [[nodiscard]] MailPanel *mailPanel() const { return _mailPanel; }
    [[nodiscard]] TalkPanel *talkPanel() const { return _talkPanel; }
    [[nodiscard]] DeckPanel *deckPanel() const { return _deckPanel; }
    [[nodiscard]] CalendarPanel *calendarPanel() const { return _calendarPanel; }

    void switchToTab(int index);

private:
    void setupSidebar(QBoxLayout *layout);
    void setupContent();
    void updateActiveTab(int index);

    QStackedWidget *_contentStack = nullptr;
    QVector<QPushButton *> _tabButtons;

    MailPanel *_mailPanel = nullptr;
    TalkPanel *_talkPanel = nullptr;
    DeckPanel *_deckPanel = nullptr;
    CalendarPanel *_calendarPanel = nullptr;
};

} // namespace OCC

#endif // SOUVERAMAINWINDOW_H
