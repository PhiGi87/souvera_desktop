/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILCOMPOSER_H
#define MAILCOMPOSER_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

namespace OCC {

class MailComposer : public QDialog
{
    Q_OBJECT
public:
    explicit MailComposer(QWidget *parent = nullptr);
    ~MailComposer() override = default;

    [[nodiscard]] QString to() const;
    [[nodiscard]] QString cc() const;
    [[nodiscard]] QString bcc() const;
    [[nodiscard]] QString subject() const;
    [[nodiscard]] QString body() const;

    void setTo(const QString &to);
    void setSubject(const QString &subject);
    void setBody(const QString &body);

    /** The send succeeded: closes the dialog (accept). */
    void sendSucceeded();
    /** The send failed: re-enables the dialog with the draft intact. */
    void sendFailed(const QString &error);

signals:
    void sendRequested(const QString &to, const QString &cc,
                       const QString &bcc, const QString &subject,
                       const QString &body);

private:
    void setupUi();
    void onSendClicked();
    void setSendInProgress(bool inProgress);

    QLineEdit *_toEdit = nullptr;
    QLineEdit *_ccEdit = nullptr;
    QLineEdit *_bccEdit = nullptr;
    QLineEdit *_subjectEdit = nullptr;
    QTextEdit *_bodyEdit = nullptr;
    QPushButton *_sendButton = nullptr;
    bool _sendInProgress = false;
};

} // namespace OCC

#endif // MAILCOMPOSER_H
