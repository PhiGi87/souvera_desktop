/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SouveraMainWindow.h"

#include "mail/MailPanel.h"
#include "talk/TalkPanel.h"
#include "deck/DeckPanel.h"
#include "calendar/CalendarPanel.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcSouveraMainWindow, "souvera.mainwindow")

namespace OCC {

static const auto TabNames = {
    QStringLiteral("Files"),
    QStringLiteral("Mail"),
    QStringLiteral("Talk"),
    QStringLiteral("Deck"),
    QStringLiteral("Calendar")
};

SouveraMainWindow::SouveraMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Souvera Workspace"));
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint);
    resize(1200, 800);

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupSidebar(layout);
    setupContent();
    layout->addWidget(_contentStack, 1);

    setCentralWidget(central);

    if (auto *screen = QApplication::primaryScreen()) {
        const auto geo = screen->availableGeometry();
        resize(static_cast<int>(geo.width() * 0.75), static_cast<int>(geo.height() * 0.8));
        move((geo.width() - width()) / 2, (geo.height() - height()) / 2);
    }

    updateActiveTab(0);
}

void SouveraMainWindow::setupSidebar(QBoxLayout *layout)
{
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("souveraSidebar"));
    sidebar->setFixedWidth(200);
    sidebar->setStyleSheet(QStringLiteral(
        "#souveraSidebar { background-color: #2b2b3d; }"));

    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    auto *logo = new QLabel(QStringLiteral("Souvera"), sidebar);
    logo->setStyleSheet(QStringLiteral(
        "color: white; font-size: 18px; font-weight: bold; padding: 20px 16px;"));
    sidebarLayout->addWidget(logo);

    auto *scrollContent = new QWidget(sidebar);
    auto *btnLayout = new QVBoxLayout(scrollContent);
    btnLayout->setContentsMargins(8, 0, 8, 0);
    btnLayout->setSpacing(2);

    for (auto i = 0u; auto &&name : TabNames) {
        auto *btn = new QPushButton(name, scrollContent);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(44);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { color: #ccc; text-align: left; padding: 10px 16px;"
            "  border: none; border-radius: 6px; font-size: 14px;"
            "  background-color: transparent; }"
            "QPushButton:hover { background-color: #3d3d5c; color: white; }"
            "QPushButton:checked { background-color: #4a90d9; color: white; font-weight: bold; }"));
        connect(btn, &QPushButton::clicked, this, [this, i]() { switchToTab(static_cast<int>(i)); });
        btnLayout->addWidget(btn);
        _tabButtons.append(btn);
        ++i;
    }

    btnLayout->addStretch();
    sidebarLayout->addWidget(scrollContent, 1);

    layout->addWidget(sidebar);
}

void SouveraMainWindow::setupContent()
{
    auto *filesPanel = new QWidget(this);
    auto *filesLayout = new QVBoxLayout(filesPanel);
    auto *filesLabel = new QLabel(QStringLiteral("Files – Synchronisation & Explorer"), filesPanel);
    filesLabel->setStyleSheet(QStringLiteral("font-size: 18px; padding: 20px;"));
    filesLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    filesLayout->addWidget(filesLabel);

    _mailPanel = new MailPanel(this);
    _talkPanel = new TalkPanel(this);
    _deckPanel = new DeckPanel(this);
    _calendarPanel = new CalendarPanel(this);

    _contentStack = new QStackedWidget(this);
    _contentStack->addWidget(filesPanel);
    _contentStack->addWidget(_mailPanel);
    _contentStack->addWidget(_talkPanel);
    _contentStack->addWidget(_deckPanel);
    _contentStack->addWidget(_calendarPanel);
}

void SouveraMainWindow::switchToTab(int index)
{
    if (index < 0 || index >= _contentStack->count()) {
        return;
    }
    _contentStack->setCurrentIndex(index);
    updateActiveTab(index);
}

void SouveraMainWindow::updateActiveTab(int index)
{
    for (auto i = 0; i < _tabButtons.size(); ++i) {
        _tabButtons[i]->setChecked(i == index);
    }
}

} // namespace OCC
