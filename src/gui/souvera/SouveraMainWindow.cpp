/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SouveraMainWindow.h"

#include "StatusHeader.h"
#include "LeftSidebar.h"
#include "FilesPanel.h"
#include "mail/MailPanel.h"
#include "talk/TalkPanel.h"
#include "deck/DeckPanel.h"
#include "calendar/CalendarPanel.h"
#include "accountmanager.h"
#include "accountstate.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QHBoxLayout>
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
    setAttribute(Qt::WA_QuitOnClose, false);

    loadStyleSheet();
    setupUi();

    QSettings settings;
    settings.beginGroup(QStringLiteral("souveraMainWindow"));
    const auto geometry = settings.value(QStringLiteral("geometry"));
    settings.endGroup();

    if (geometry.isValid()) {
        restoreGeometry(geometry.toByteArray());
    } else {
        resize(1280, 860);
        if (auto *screen = QApplication::primaryScreen()) {
            const auto geo = screen->availableGeometry();
            move((geo.width() - width()) / 2, (geo.height() - height()) / 2);
        }
    }

    switchToTab(1);

    if (auto *am = AccountManager::instance()) {
        const auto accounts = am->accounts();
        if (!accounts.isEmpty()) {
            _mailPanel->setAccountState(accounts.first().data());
        }
    }
}

void SouveraMainWindow::setupUi()
{
    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("ContentArea"));

    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    _sidebar = new LeftSidebar(root);
    _sidebar->addItem(QStringLiteral("\u2709\uFE0F"), QStringLiteral("Mail"));
    _sidebar->addItem(QStringLiteral("\U0001F4AC"), QStringLiteral("Talk"));
    _sidebar->addItem(QStringLiteral("\U0001F4CB"), QStringLiteral("Deck"));
    _sidebar->addItem(QStringLiteral("\U0001F4C5"), QStringLiteral("Kalender"));
    _sidebar->addItem(QStringLiteral("\U0001F4C1"), QStringLiteral("Dateien"));
    rootLayout->addWidget(_sidebar);

    auto *contentArea = new QWidget(root);
    contentArea->setObjectName(QStringLiteral("ContentArea"));
    auto *contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    _statusHeader = new StatusHeader(contentArea);
    connect(_statusHeader, &StatusHeader::settingsClicked,
            this, &SouveraMainWindow::settingsRequested);
    contentLayout->addWidget(_statusHeader);

    _mailPanel = new MailPanel(contentArea);
    _talkPanel = new TalkPanel(contentArea);
    _deckPanel = new DeckPanel(contentArea);
    _calendarPanel = new CalendarPanel(contentArea);
    _filesPanel = new FilesPanel(contentArea);

    _contentStack = new QStackedWidget(contentArea);
    _contentStack->setObjectName(QStringLiteral("ContentArea"));
    _contentStack->addWidget(_mailPanel);
    _contentStack->addWidget(_talkPanel);
    _contentStack->addWidget(_deckPanel);
    _contentStack->addWidget(_calendarPanel);
    _contentStack->addWidget(_filesPanel);
    contentLayout->addWidget(_contentStack, 1);

    rootLayout->addWidget(contentArea, 1);

    connect(_sidebar, &LeftSidebar::currentChanged, this, &SouveraMainWindow::switchToTab);

    setCentralWidget(root);
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
    if (index < 0 || index >= _contentStack->count()) return;
    _contentStack->setCurrentIndex(index);
    _sidebar->setCurrentIndex(index);
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
