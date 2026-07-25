/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckPanel.h"
#include "DeckOcsApi.h"

#include <QLabel>
#include <QLoggingCategory>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcDeckPanel, "souvera.deck.panel")

namespace OCC {

DeckColumnWidget::DeckColumnWidget(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setFixedWidth(280);
    setStyleSheet(QStringLiteral(
        "DeckColumnWidget { background-color: #f0f2f5; border-radius: 8px; padding: 8px; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *header = new QLabel(title, this);
    header->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 14px; padding: 4px 0; color: #333;"));
    layout->addWidget(header);
    layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void DeckColumnWidget::addCard(const QString &title, const QString &description)
{
    auto *card = new QFrame(this);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background-color: white; border-radius: 6px; border: 1px solid #ddd;"
        "  padding: 8px; }"
        "QFrame:hover { border-color: #4a90d9; }"));

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(4);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #333;"));
    titleLabel->setWordWrap(true);

    auto *descLabel = new QLabel(description, card);
    descLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #666;"));
    descLabel->setWordWrap(true);

    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(descLabel);

    auto *layout = qobject_cast<QHBoxLayout *>(this->layout());
    if (layout) {
        layout->insertWidget(layout->count() - 1, card);
    }
}

DeckPanel::DeckPanel(QWidget *parent)
    : QWidget(parent)
{
    _ocsApi = new DeckOcsApi(this);
    setupUi();
    loadDemoData();
}

void DeckPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);

    auto *title = new QLabel(QStringLiteral("Deck – Kanban"), toolbar);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();

    layout->addWidget(toolbar);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setStyleSheet(QStringLiteral("QScrollArea { border: none; background: transparent; }"));

    _columnsContainer = new QWidget(_scrollArea);
    _columnsLayout = new QHBoxLayout(_columnsContainer);
    _columnsLayout->setContentsMargins(12, 8, 12, 8);
    _columnsLayout->setSpacing(12);

    _scrollArea->setWidget(_columnsContainer);
    layout->addWidget(_scrollArea, 1);
}

void DeckPanel::loadDemoData()
{
    auto *todo = new DeckColumnWidget(QStringLiteral("Offen"), _columnsContainer);
    todo->addCard(QStringLiteral("Design finalisieren"), QStringLiteral("UI/UX Review abschließen"));
    todo->addCard(QStringLiteral("Dokumentation"), QStringLiteral("API-Doku für v2 schreiben"));
    _columnsLayout->addWidget(todo);

    auto *inProgress = new DeckColumnWidget(QStringLiteral("In Bearbeitung"), _columnsContainer);
    inProgress->addCard(QStringLiteral("Login-Fix"), QStringLiteral("OAuth2 Token Refresh reparieren"));
    inProgress->addCard(QStringLiteral("Performance"), QStringLiteral("Sync-Engine optimieren"));
    _columnsLayout->addWidget(inProgress);

    auto *done = new DeckColumnWidget(QStringLiteral("Fertig"), _columnsContainer);
    done->addCard(QStringLiteral("Build-Pipeline"), QStringLiteral("CI/CD für ARM64"));
    done->addCard(QStringLiteral("Logging"), QStringLiteral("Structured Logging eingeführt"));
    _columnsLayout->addWidget(done);

    _columnsLayout->addStretch();
}

} // namespace OCC
