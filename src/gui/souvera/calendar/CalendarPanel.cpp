/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CalendarPanel.h"
#include "CalDavSync.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QTextEdit>

Q_LOGGING_CATEGORY(lcCalendarPanel, "souvera.calendar.panel")

namespace OCC {

CalendarPanel::CalendarPanel(QWidget *parent)
    : QWidget(parent)
{
    _calDavSync = new CalDavSync(this);
    setupUi();
}

void CalendarPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);

    auto *title = new QLabel(QStringLiteral("Kalender"), toolbar);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();

    layout->addWidget(toolbar);

    _splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftWidget = new QWidget(_splitter);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    _calendar = new QCalendarWidget(leftWidget);
    _calendar->setGridVisible(true);
    _calendar->setStyleSheet(QStringLiteral(
        "QCalendarWidget { background-color: white; }"
        "QCalendarWidget QAbstractItemView:enabled { font-size: 13px; }"));
    connect(_calendar, &QCalendarWidget::clicked, this, &CalendarPanel::onDateSelected);
    leftLayout->addWidget(_calendar);

    auto *rightWidget = new QWidget(_splitter);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(8, 8, 8, 8);

    auto *eventHeader = new QLabel(QStringLiteral("Termine"), rightWidget);
    eventHeader->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; margin-bottom: 4px;"));
    rightLayout->addWidget(eventHeader);

    _eventList = new QListWidget(rightWidget);
    _eventList->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid #ddd; border-radius: 4px; }"));
    rightLayout->addWidget(_eventList, 1);

    _eventDetail = new QTextEdit(rightWidget);
    _eventDetail->setReadOnly(true);
    _eventDetail->setMaximumHeight(120);
    _eventDetail->setPlaceholderText(QStringLiteral("Wählen Sie einen Termin aus…"));
    rightLayout->addWidget(_eventDetail);

    _splitter->addWidget(leftWidget);
    _splitter->addWidget(rightWidget);
    _splitter->setStretchFactor(0, 3);
    _splitter->setStretchFactor(1, 2);

    layout->addWidget(_splitter, 1);

    // Demo events
    const auto today = QDate::currentDate();
    _calendar->setSelectedDate(today);
    onDateSelected(today);
}

void CalendarPanel::onDateSelected(const QDate &date)
{
    _eventList->clear();
    _eventDetail->clear();
    _eventList->addItem(QStringLiteral("(Keine Termine für %1)").arg(date.toString(Qt::ISODate)));
    qCInfo(lcCalendarPanel) << "Date selected:" << date;
}

} // namespace OCC
