/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SouveraMainWindow.h"

#include "StatusHeader.h"
#include "BottomBar.h"
#include "FilesPanel.h"
#include "mail/MailPanel.h"
#include "talk/TalkPanel.h"
#include "deck/DeckPanel.h"
#include "calendar/CalendarPanel.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QLabel>
#include <QLoggingCategory>
#include <QScreen>
#include <QSettings>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcSouveraMainWindow, "souvera.mainwindow")

namespace OCC {

SouveraMainWindow::SouveraMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Souvera Workspace"));
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_QuitOnHide, false);

    loadStyleSheet();
    setupUi();

    QSettings settings;
    settings.beginGroup(QStringLiteral("souveraMainWindow"));
    auto geometry = settings.value(QStringLiteral("geometry"));
    settings.endGroup();

    if (geometry.isValid()) {
        restoreGeometry(geometry.toByteArray());
    } else {
        resize(1200, 800);
        if (auto *screen = QApplication::primaryScreen()) {
            auto geo = screen->availableGeometry();
            move((geo.width() - width()) / 2, (geo.height() - height()) / 2);
        }
    }

    switchToTab(0);
}

void SouveraMainWindow::setupUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("ContentArea"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _statusHeader = new StatusHeader(central);
    connect(_statusHeader, &StatusHeader::settingsClicked,
            this, &SouveraMainWindow::settingsRequested);
    layout->addWidget(_statusHeader);

    _filesPanel = new FilesPanel(central);
    _mailPanel = new MailPanel(central);
    _talkPanel = new TalkPanel(central);
    _deckPanel = new DeckPanel(central);
    _calendarPanel = new CalendarPanel(central);

    _contentStack = new QStackedWidget(central);
    _contentStack->setObjectName(QStringLiteral("ContentArea"));
    _contentStack->addWidget(_filesPanel);
    _contentStack->addWidget(_mailPanel);
    _contentStack->addWidget(_talkPanel);
    _contentStack->addWidget(_deckPanel);
    _contentStack->addWidget(_calendarPanel);
    layout->addWidget(_contentStack, 1);

    _bottomBar = new BottomBar(central);
    connect(_bottomBar, &BottomBar::currentChanged, this, &SouveraMainWindow::switchToTab);
    layout->addWidget(_bottomBar);

    setCentralWidget(central);
}

void SouveraMainWindow::loadStyleSheet()
{
    QFile qssFile(QStringLiteral(":/souvera/souvera.qss"));
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(QString::fromUtf8(qssFile.readAll()));
        qssFile.close();
    }
}

void SouveraMainWindow::switchToTab(int index)
{
    if (index < 0 || index >= _contentStack->count()) {
        return;
    }
    _contentStack->setCurrentIndex(index);
    _bottomBar->setCurrentIndex(index);
}

void SouveraMainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("souveraMainWindow"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.endGroup();

    hide();
    event->ignore();
}

} // namespace OCC
