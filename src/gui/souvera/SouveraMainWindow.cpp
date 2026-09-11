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
#include "SettingsPanel.h"
#include "accountmanager.h"
#include "owncloudsetupwizard.h"
#include "accountstate.h"
#include "theme/SouveraTheme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QShortcut>
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
    const auto maximized = settings.value(QStringLiteral("maximized"), true).toBool();
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
    if (maximized) {
        setWindowState(windowState() | Qt::WindowMaximized);
    }

    // Fullscreen toggle (F11)
    auto *fullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreenShortcut, &QShortcut::activated, this, [this]() {
        setWindowState(isFullScreen() ? (windowState() & ~Qt::WindowFullScreen)
                                      : (windowState() | Qt::WindowFullScreen));
    });

    switchToTab(0);

    setupAccountGate();
}

void SouveraMainWindow::setupAccountGate()
{
    auto *am = AccountManager::instance();
    if (!am) return;

    // As soon as a workspace account exists, unlock the window and wire
    // every panel to it (the wizard adds the account at runtime).
    connect(am, &AccountManager::accountAdded, this, [this](AccountState *state) {
        if (_setupPending) {
            _setupPending = false;
            setEnabled(true);
        }
        connectAccount(state);
    });

    connect(am, &AccountManager::accountRemoved, this, [this]() {
        auto *manager = AccountManager::instance();
        if (manager && manager->accounts().isEmpty()) {
            // Mandatory setup again: close the workspace content, disable the
            // window and bring the login wizard back to the front.
            _setupPending = true;
            setEnabled(false);
            showNormal(); // un-fullscreen so the wizard is visible
            QTimer::singleShot(200, this, &SouveraMainWindow::enforceWorkspaceSetup);
        }
    });

    const auto accounts = am->accounts();
    if (accounts.isEmpty()) {
        // Mandatory workspace setup: the wizard stays in front until the
        // connection to *.souvera.work exists; nothing is clickable before.
        _setupPending = true;
        setEnabled(false);
        QTimer::singleShot(200, this, &SouveraMainWindow::enforceWorkspaceSetup);
    } else {
        connectAccount(accounts.first().data());
    }
}

void SouveraMainWindow::connectAccount(AccountState *accountState)
{
    if (!accountState) return;
    _mailPanel->setAccountState(accountState);
    _talkPanel->setAccountState(accountState);
    _deckPanel->setAccountState(accountState);
    _calendarPanel->setAccountState(accountState);
    _notesPanel->setAccountState(accountState);
    _filesPanel->setAccountState(accountState);
    _settingsPanel->setAccountState(accountState);
}

void SouveraMainWindow::enforceWorkspaceSetup()
{
    auto *am = AccountManager::instance();
    if (!am || !am->accounts().isEmpty()) {
        return; // setup completed in the meantime
    }
    if (OwncloudSetupWizard::bringWizardToFrontIfVisible()) {
        return; // wizard is already open
    }
    OwncloudSetupWizard::runWizard(this, SLOT(slotWorkspaceWizardDone(int)), this);
}

void SouveraMainWindow::slotWorkspaceWizardDone(int result)
{
    Q_UNUSED(result)
    if (!_setupPending) return;

    auto *am = AccountManager::instance();
    if (am && !am->accounts().isEmpty()) {
        return; // accountAdded handler takes over
    }
    // Mandatory setup: re-open the wizard until the workspace is connected.
    QTimer::singleShot(200, this, &SouveraMainWindow::enforceWorkspaceSetup);
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
    _sidebar->addItem(QStringLiteral("settings"), QStringLiteral("Einstellungen"));
    rootLayout->addWidget(_sidebar);

    auto *contentArea = new QWidget(root);
    contentArea->setObjectName(QStringLiteral("ContentArea"));
    auto *contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    _statusHeader = new StatusHeader(contentArea);
    connect(_statusHeader, &StatusHeader::settingsClicked, this, [this]() {
        switchToTab(_contentStack->count() - 1);
        emit settingsRequested();
    });
    contentLayout->addWidget(_statusHeader);

    _mailPanel = new MailPanel(contentArea);
    _talkPanel = new TalkPanel(contentArea);
    _filesPanel = new FilesPanel(contentArea);
    _deckPanel = new DeckPanel(contentArea);
    _calendarPanel = new CalendarPanel(contentArea);
    _notesPanel = new NotesPanel(nullptr, contentArea);
    _settingsPanel = new SettingsPanel(contentArea);

    _contentStack = new QStackedWidget(contentArea);
    _contentStack->setObjectName(QStringLiteral("ContentArea"));
    _contentStack->addWidget(_mailPanel);
    _contentStack->addWidget(_talkPanel);
    _contentStack->addWidget(_filesPanel);
    _contentStack->addWidget(_deckPanel);
    _contentStack->addWidget(_calendarPanel);
    _contentStack->addWidget(_notesPanel);
    _contentStack->addWidget(_settingsPanel);
    contentLayout->addWidget(_contentStack, 1);

    rootLayout->addWidget(contentArea, 1);

    connect(_sidebar, &LeftSidebar::currentChanged, this, &SouveraMainWindow::switchToTab);
    connect(_settingsPanel, &SettingsPanel::viewSettingsChanged,
            _mailPanel, &MailPanel::applyViewSettings);

    setCentralWidget(root);
}

void SouveraMainWindow::loadStyleSheet()
{
    // Apply palette FIRST (always works, even if the QSS has a parse error),
    // then the stylesheet for panel-specific styling on top.
    SouveraTheme::applyTheme();
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
    settings.setValue(QStringLiteral("maximized"), isMaximized());
    settings.endGroup();

    hide();
    event->ignore();
}

} // namespace OCC
