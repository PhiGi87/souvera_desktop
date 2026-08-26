/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SOUVERAMAINWINDOW_H
#define SOUVERAMAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>

class QHBoxLayout;

namespace OCC {

class MailPanel;
class TalkPanel;
class DeckPanel;
class CalendarPanel;
class FilesPanel;
class NotesPanel;
class StatusHeader;
class LeftSidebar;

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
    [[nodiscard]] NotesPanel *notesPanel() const { return _notesPanel; }

    void switchToTab(int index);

signals:
    void settingsRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void loadStyleSheet();

    LeftSidebar *_sidebar = nullptr;
    StatusHeader *_statusHeader = nullptr;
    QStackedWidget *_contentStack = nullptr;

    FilesPanel *_filesPanel = nullptr;
    MailPanel *_mailPanel = nullptr;
    TalkPanel *_talkPanel = nullptr;
    DeckPanel *_deckPanel = nullptr;
    CalendarPanel *_calendarPanel = nullptr;
    NotesPanel *_notesPanel = nullptr;
};

} // namespace OCC

#endif // SOUVERAMAINWINDOW_H
