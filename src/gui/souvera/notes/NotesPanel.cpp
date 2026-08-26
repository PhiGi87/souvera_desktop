/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "NotesPanel.h"
#include "accountstate.h"
#include "account.h"
#include "creds/abstractcredentials.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace OCC {

NotesPanel::NotesPanel(AccountState *accountState, QWidget *parent)
    : QWidget(parent)
    , _accountState(accountState)
    , _nam(new QNetworkAccessManager(this))
    , _noteList(new QListWidget(this))
    , _editor(new QPlainTextEdit(this))
    , _preview(new QTextBrowser(this))
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QHBoxLayout;
    auto *newBtn = new QPushButton(QStringLiteral("+ New Note"));
    auto *saveBtn = new QPushButton(QStringLiteral("Save"));
    auto *delBtn = new QPushButton(QStringLiteral("Delete"));
    toolbar->addWidget(newBtn);
    toolbar->addWidget(saveBtn);
    toolbar->addWidget(delBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    _noteList->setMaximumWidth(280);
    _editor->setPlaceholderText(QStringLiteral("Write your note in Markdown…"));
    _preview->setOpenExternalLinks(true);
    splitter->addWidget(_noteList);
    splitter->addWidget(_editor);
    splitter->addWidget(_preview);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 2);
    mainLayout->addWidget(splitter);

    connect(newBtn, &QPushButton::clicked, this, &NotesPanel::createNote);
    connect(saveBtn, &QPushButton::clicked, this, &NotesPanel::saveCurrentNote);
    connect(delBtn, &QPushButton::clicked, this, &NotesPanel::deleteCurrentNote);
    connect(_noteList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < _notes.size()) fetchNoteContent(_notes.at(row).id);
    });

    // Simple Markdown preview on text change
    connect(_editor, &QPlainTextEdit::textChanged, this, [this]() {
        auto html = _editor->toPlainText();
        html.replace(QLatin1String("&"), QLatin1String("&amp;"));
        html.replace(QLatin1String("<"), QLatin1String("&lt;"));
        html.replace(QLatin1String(">"), QLatin1String("&gt;"));
        // Basic markdown: headers, bold, italic
        html.replace(QRegularExpression("^### (.+)$", QRegularExpression::MultilineOption),
                     QStringLiteral("<h3>\\1</h3>"));
        html.replace(QRegularExpression("^## (.+)$", QRegularExpression::MultilineOption),
                     QStringLiteral("<h2>\\1</h2>"));
        html.replace(QRegularExpression("^# (.+)$", QRegularExpression::MultilineOption),
                     QStringLiteral("<h1>\\1</h1>"));
        html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), QStringLiteral("<b>\\1</b>"));
        html.replace(QRegularExpression("\\*(.+?)\\*"), QStringLiteral("<i>\\1</i>"));
        html.replace(QLatin1String("\n\n"), QLatin1String("<br><br>"));
        _preview->setHtml(html);
    });
}

void NotesPanel::loadNotes()
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return;
    _user = acc->credentials()->user();
    _password = acc->credentials()->password();
    fetchNotes();
}

void NotesPanel::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) return;
    _accountState = accountState;
    loadNotes();
}

void NotesPanel::fetchNotes()
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return;
    QUrl url(acc->url());
    url.setPath(QLatin1String("/index.php/apps/notes/api/v1/notes"));
    QNetworkRequest req(url);
    const auto cred = QStringLiteral("%1:%2").arg(_user, _password).toUtf8().toBase64();
    req.setRawHeader("Authorization", QByteArray("Basic ") + cred);
    req.setRawHeader("Accept", "application/json");

    auto *reply = _nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const auto arr = QJsonDocument::fromJson(reply->readAll()).array();
        _notes.clear();
        _noteList->clear();
        for (const auto &v : arr) {
            const auto o = v.toObject();
            NoteItem n;
            n.id = o.value(QLatin1String("id")).toInt();
            n.title = o.value(QLatin1String("title")).toString();
            n.content = o.value(QLatin1String("content")).toString();
            n.modified = QDateTime::fromString(o.value(QLatin1String("modified")).toString(), Qt::ISODate);
            _notes.append(n);
            _noteList->addItem(n.title.isEmpty() ? QLatin1String("Untitled") : n.title);
        }
    });
}

void NotesPanel::fetchNoteContent(int noteId)
{
    _currentNoteId = noteId;
    for (const auto &n : _notes) {
        if (n.id == noteId) {
            _editor->setPlainText(n.content);
            return;
        }
    }
}

void NotesPanel::createNote()
{
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return;
    QUrl url(acc->url());
    url.setPath(QLatin1String("/index.php/apps/notes/api/v1/notes"));
    QNetworkRequest req(url);
    const auto cred = QStringLiteral("%1:%2").arg(_user, _password).toUtf8().toBase64();
    req.setRawHeader("Authorization", QByteArray("Basic ") + cred);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    QJsonObject body;
    body[QLatin1String("title")] = QLatin1String("New Note");
    body[QLatin1String("content")] = QString();
    auto *reply = _nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        fetchNotes();
    });
}

void NotesPanel::saveCurrentNote()
{
    if (_currentNoteId < 0) return;
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return;
    QUrl url(acc->url());
    url.setPath(QStringLiteral("/index.php/apps/notes/api/v1/notes/%1").arg(_currentNoteId));
    QNetworkRequest req(url);
    const auto cred = QStringLiteral("%1:%2").arg(_user, _password).toUtf8().toBase64();
    req.setRawHeader("Authorization", QByteArray("Basic ") + cred);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    QJsonObject body;
    body[QLatin1String("content")] = _editor->toPlainText();
    auto *reply = _nam->put(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
    });
}

void NotesPanel::deleteCurrentNote()
{
    if (_currentNoteId < 0) return;
    const auto acc = _accountState ? _accountState->account() : nullptr;
    if (!acc) return;
    QUrl url(acc->url());
    url.setPath(QStringLiteral("/index.php/apps/notes/api/v1/notes/%1").arg(_currentNoteId));
    QNetworkRequest req(url);
    const auto cred = QStringLiteral("%1:%2").arg(_user, _password).toUtf8().toBase64();
    req.setRawHeader("Authorization", QByteArray("Basic ") + cred);
    auto *reply = _nam->deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        _editor->clear();
        _currentNoteId = -1;
        fetchNotes();
    });
}

} // namespace OCC
