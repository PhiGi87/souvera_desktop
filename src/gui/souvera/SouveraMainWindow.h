/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SOUVERAMAINWINDOW_H
#define SOUVERAMAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>

namespace OCC {

class MailPanel;
class TalkPanel;
class DeckPanel;
class CalendarPanel;
class FilesPanel;
class StatusHeader;
class BottomBar;

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

signals:
    void settingsRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void loadStyleSheet();

    StatusHeader *_statusHeader = nullptr;
    QStackedWidget *_contentStack = nullptr;
    BottomBar *_bottomBar = nullptr;

    FilesPanel *_filesPanel = nullptr;
    MailPanel *_mailPanel = nullptr;
    TalkPanel *_talkPanel = nullptr;
    DeckPanel *_deckPanel = nullptr;
    CalendarPanel *_calendarPanel = nullptr;
};

} // namespace OCC

#endif // SOUVERAMAINWINDOW_H
