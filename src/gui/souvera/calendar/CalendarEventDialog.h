/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALENDAREVENTDIALOG_H
#define CALENDAREVENTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QDateEdit>
#include <QTimeEdit>
#include <QTextEdit>
#include <QDateTime>

namespace OCC {

class CalendarEventDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CalendarEventDialog(QWidget *parent = nullptr);
    ~CalendarEventDialog() override = default;

    QByteArray getICalendar() const;

signals:
    void eventCreated(const QString &title,
                      const QDateTime &startDt,
                      const QDateTime &endDt,
                      const QString &description);

private:
    void onSave();
    static QString escapeICalText(const QString &text);

    QLineEdit *_titleEdit = nullptr;
    QDateEdit *_startDateEdit = nullptr;
    QTimeEdit *_startTimeEdit = nullptr;
    QDateEdit *_endDateEdit = nullptr;
    QTimeEdit *_endTimeEdit = nullptr;
    QTextEdit *_descriptionEdit = nullptr;
};

} // namespace OCC

#endif // CALENDAREVENTDIALOG_H
