/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailPanel.h"
#include "MailFolderModel.h"
#include "MailMessageModel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QHeaderView>

Q_LOGGING_CATEGORY(lcMailPanel, "souvera.mail.panel")

namespace OCC {

MailPanel::MailPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void MailPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);

    auto *title = new QLabel(QStringLiteral("E-Mail"), toolbar);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();

    _newMsgBtn = new QPushButton(QStringLiteral("+ Neue Nachricht"), toolbar);
    _newMsgBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4a90d9; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #357abd; }"));
    connect(_newMsgBtn, &QPushButton::clicked, this, &MailPanel::onNewMessage);
    toolbarLayout->addWidget(_newMsgBtn);

    layout->addWidget(toolbar);

    _splitter = new QSplitter(Qt::Horizontal, this);

    _folderView = new QTreeView(_splitter);
    _folderView->setHeaderHidden(true);
    _folderView->setFixedWidth(220);

    _messageView = new QListView(_splitter);

    _preview = new QTextEdit(_splitter);
    _preview->setReadOnly(true);
    _preview->setPlaceholderText(QStringLiteral("Wählen Sie eine Nachricht aus…"));

    _splitter->addWidget(_folderView);
    _splitter->addWidget(_messageView);
    _splitter->addWidget(_preview);
    _splitter->setStretchFactor(0, 1);
    _splitter->setStretchFactor(1, 2);
    _splitter->setStretchFactor(2, 3);

    _folderModel = new MailFolderModel(this);
    _folderView->setModel(_folderModel);
    connect(_folderView, &QTreeView::clicked, this, &MailPanel::onFolderSelected);

    _messageModel = new MailMessageModel(this);
    _messageView->setModel(_messageModel);
    connect(_messageView, &QListView::clicked, this, &MailPanel::onMessageSelected);

    layout->addWidget(_splitter, 1);
}

void MailPanel::onFolderSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;
    qCInfo(lcMailPanel) << "Folder selected:" << index.data().toString();
    _messageModel->loadFolder(index.data().toString());
}

void MailPanel::onMessageSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;
    qCInfo(lcMailPanel) << "Message selected:" << index.data().toString();
    _preview->setHtml(index.data(Qt::UserRole).toString());
}

void MailPanel::onNewMessage()
{
    qCInfo(lcMailPanel) << "New message requested";
}

} // namespace OCC
