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
#include "notes/NotesPanel.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "theme/SouveraTheme.h"

#include <QApplication>
#include <QCloseEvent>
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

    switchToTab(0);

    if (auto *am = AccountManager::instance()) {
        const auto accounts = am->accounts();
        if (!accounts.isEmpty()) {
            const auto accountState = accounts.first().data();
            _mailPanel->setAccountState(accountState);
            _talkPanel->setAccountState(accountState);
            _deckPanel->setAccountState(accountState);
            _calendarPanel->setAccountState(accountState);
            _notesPanel->setAccountState(accountState);
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
    _sidebar->addItem(QStringLiteral("mail"), QStringLiteral("Mail"));
    _sidebar->addItem(QStringLiteral("chat"), QStringLiteral("Talk"));
    _sidebar->addItem(QStringLiteral("folder"), QStringLiteral("Dateien"));
    _sidebar->addItem(QStringLiteral("board"), QStringLiteral("Deck"));
    _sidebar->addItem(QStringLiteral("calendar"), QStringLiteral("Kalender"));
    _sidebar->addItem(QStringLiteral("notes"), QStringLiteral("Notizen"));
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
    _filesPanel = new FilesPanel(contentArea);
    _deckPanel = new DeckPanel(contentArea);
    _calendarPanel = new CalendarPanel(contentArea);
    _notesPanel = new NotesPanel(nullptr, contentArea);

    _contentStack = new QStackedWidget(contentArea);
    _contentStack->setObjectName(QStringLiteral("ContentArea"));
    _contentStack->addWidget(_mailPanel);
    _contentStack->addWidget(_talkPanel);
    _contentStack->addWidget(_filesPanel);
    _contentStack->addWidget(_deckPanel);
    _contentStack->addWidget(_calendarPanel);
    _contentStack->addWidget(_notesPanel);
    contentLayout->addWidget(_contentStack, 1);

    rootLayout->addWidget(contentArea, 1);

    connect(_sidebar, &LeftSidebar::currentChanged, this, &SouveraMainWindow::switchToTab);

    setCentralWidget(root);
}

void SouveraMainWindow::loadStyleSheet()
{
    SouveraTheme::instance()->applyStyleSheet();
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
