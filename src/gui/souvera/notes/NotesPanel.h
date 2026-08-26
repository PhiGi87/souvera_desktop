/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NOTESPANEL_H
#define NOTESPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QNetworkAccessManager>
#include <QString>
#include <QDateTime>
#include <QList>

namespace OCC {

class AccountState;

struct NoteItem {
    int id = 0;
    QString title;
    QString content;
    QDateTime modified;
};

class NotesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit NotesPanel(AccountState *accountState, QWidget *parent = nullptr);
    void setAccountState(AccountState *accountState);
    void loadNotes();

signals:
    void errorOccurred(const QString &error);

private:
    void fetchNotes();
    void fetchNoteContent(int noteId);
    void createNote();
    void saveCurrentNote();
    void deleteCurrentNote();

    AccountState *_accountState;
    QNetworkAccessManager *_nam;
    QListWidget *_noteList;
    QPlainTextEdit *_editor;
    QTextBrowser *_preview;
    QList<NoteItem> _notes;
    int _currentNoteId = -1;
    QString _user;
    QString _password;
};

} // namespace OCC

#endif // NOTESPANEL_H
