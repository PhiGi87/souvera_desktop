/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CalendarEventDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcCalendarEventDialog, "souvera.calendar.eventdialog")

namespace OCC {

CalendarEventDialog::CalendarEventDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Neuer Termin"));
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);

    auto *formLayout = new QFormLayout;

    _titleEdit = new QLineEdit(this);
    _titleEdit->setPlaceholderText(QStringLiteral("Titel"));
    formLayout->addRow(QStringLiteral("Titel:"), _titleEdit);

    _startDateEdit = new QDateEdit(QDate::currentDate(), this);
    _startDateEdit->setCalendarPopup(true);
    _startDateEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    formLayout->addRow(QStringLiteral("Startdatum:"), _startDateEdit);

    _startTimeEdit = new QTimeEdit(QTime::currentTime().addSecs(3600), this);
    _startTimeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    formLayout->addRow(QStringLiteral("Startzeit:"), _startTimeEdit);

    _endDateEdit = new QDateEdit(QDate::currentDate(), this);
    _endDateEdit->setCalendarPopup(true);
    _endDateEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    formLayout->addRow(QStringLiteral("Enddatum:"), _endDateEdit);

    _endTimeEdit = new QTimeEdit(QTime::currentTime().addSecs(7200), this);
    _endTimeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    formLayout->addRow(QStringLiteral("Endzeit:"), _endTimeEdit);

    layout->addLayout(formLayout);

    auto *descLabel = new QLabel(QStringLiteral("Beschreibung:"), this);
    layout->addWidget(descLabel);

    _descriptionEdit = new QTextEdit(this);
    _descriptionEdit->setPlaceholderText(QStringLiteral("Beschreibung (optional)"));
    _descriptionEdit->setMaximumHeight(120);
    layout->addWidget(_descriptionEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CalendarEventDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

void CalendarEventDialog::onSave()
{
    if (_titleEdit->text().trimmed().isEmpty()) {
        _titleEdit->setFocus();
        return;
    }

    auto title = _titleEdit->text().trimmed();
    auto startDt = QDateTime(_startDateEdit->date(), _startTimeEdit->time());
    auto endDt = QDateTime(_endDateEdit->date(), _endTimeEdit->time());
    auto description = _descriptionEdit->toPlainText().trimmed();

    emit eventCreated(title, startDt, endDt, description);
    accept();
}

QString CalendarEventDialog::escapeICalText(const QString &text)
{
    auto result = text;
    result.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    result.replace(QStringLiteral(";"), QStringLiteral("\\;"));
    result.replace(QStringLiteral(","), QStringLiteral("\\,"));
    result.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    result.replace(QStringLiteral("\r"), QString());
    return result;
}

QByteArray CalendarEventDialog::getICalendar() const
{
    auto uid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto formatDt = [](const QDateTime &dt) -> QString {
        return dt.toUTC().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    };

    auto startDt = QDateTime(_startDateEdit->date(), _startTimeEdit->time());
    auto endDt = QDateTime(_endDateEdit->date(), _endTimeEdit->time());

    auto iCal = QStringLiteral(
        "BEGIN:VCALENDAR\r\n"
        "VERSION:2.0\r\n"
        "PRODID:-//Souvera//Desktop//DE\r\n"
        "BEGIN:VEVENT\r\n"
        "UID:%1\r\n"
        "DTSTART:%2\r\n"
        "DTEND:%3\r\n"
        "SUMMARY:%4\r\n"
        "DESCRIPTION:%5\r\n"
        "END:VEVENT\r\n"
        "END:VCALENDAR\r\n")
        .arg(uid,
             formatDt(startDt),
             formatDt(endDt),
             escapeICalText(_titleEdit->text().trimmed()),
             escapeICalText(_descriptionEdit->toPlainText().trimmed()));

    return iCal.toUtf8();
}

} // namespace OCC
