/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailComposer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QLoggingCategory>
#include <QApplication>

Q_LOGGING_CATEGORY(lcMailComposer, "souvera.mail.composer")

namespace OCC {

MailComposer::MailComposer(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void MailComposer::setupUi()
{
    setWindowTitle(QStringLiteral("Neue Nachricht"));
    setMinimumSize(600, 500);
    resize(700, 550);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    auto *formLayout = new QFormLayout;
    formLayout->setSpacing(6);

    _toEdit = new QLineEdit(this);
    _toEdit->setPlaceholderText(QStringLiteral("Empfänger"));
    formLayout->addRow(QStringLiteral("An:"), _toEdit);

    _ccEdit = new QLineEdit(this);
    _ccEdit->setPlaceholderText(QStringLiteral("CC"));
    formLayout->addRow(QStringLiteral("CC:"), _ccEdit);

    _bccEdit = new QLineEdit(this);
    _bccEdit->setPlaceholderText(QStringLiteral("BCC"));
    formLayout->addRow(QStringLiteral("BCC:"), _bccEdit);

    _subjectEdit = new QLineEdit(this);
    _subjectEdit->setPlaceholderText(QStringLiteral("Betreff"));
    formLayout->addRow(QStringLiteral("Betreff:"), _subjectEdit);

    mainLayout->addLayout(formLayout);

    auto *bodyLabel = new QLabel(QStringLiteral("Nachricht:"), this);
    mainLayout->addWidget(bodyLabel);

    _bodyEdit = new QTextEdit(this);
    _bodyEdit->setPlaceholderText(QStringLiteral("Schreiben Sie Ihre Nachricht…"));
    mainLayout->addWidget(_bodyEdit, 1);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto *cancelButton = new QPushButton(QStringLiteral("Abbrechen"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    _sendButton = new QPushButton(QStringLiteral("Senden"), this);
    _sendButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4a90d9; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 20px; font-weight: bold; }"
        "QPushButton:hover { background-color: #357abd; }"));
    connect(_sendButton, &QPushButton::clicked, this, &MailComposer::onSendClicked);
    buttonLayout->addWidget(_sendButton);

    mainLayout->addLayout(buttonLayout);
}

QString MailComposer::to() const
{
    return _toEdit->text().trimmed();
}

QString MailComposer::cc() const
{
    return _ccEdit->text().trimmed();
}

QString MailComposer::bcc() const
{
    return _bccEdit->text().trimmed();
}

QString MailComposer::subject() const
{
    return _subjectEdit->text().trimmed();
}

QString MailComposer::body() const
{
    return _bodyEdit->toPlainText().trimmed();
}

void MailComposer::setTo(const QString &to)
{
    _toEdit->setText(to);
}

void MailComposer::setSubject(const QString &subject)
{
    _subjectEdit->setText(subject);
}

void MailComposer::setBody(const QString &body)
{
    _bodyEdit->setPlainText(body);
}

void MailComposer::onSendClicked()
{
    if (_toEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Hinweis"),
                             QStringLiteral("Bitte geben Sie mindestens einen Empfänger an."));
        _toEdit->setFocus();
        return;
    }

    if (_subjectEdit->text().trimmed().isEmpty() && _bodyEdit->toPlainText().trimmed().isEmpty()) {
        auto ret = QMessageBox::question(this, QStringLiteral("Nachricht leer"),
                                          QStringLiteral("Die Nachricht hat weder Betreff noch Inhalt. Trotzdem senden?"),
                                          QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    qCInfo(lcMailComposer) << "Send requested to:" << _toEdit->text();
    emit sendRequested(to(), cc(), bcc(), subject(), body());
    accept();
}

} // namespace OCC
