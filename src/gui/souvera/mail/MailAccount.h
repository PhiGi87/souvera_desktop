/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILACCOUNT_H
#define MAILACCOUNT_H

#include <QObject>
#include <QString>

namespace OCC {

class MailAccount : public QObject
{
    Q_OBJECT
public:
    explicit MailAccount(QObject *parent = nullptr);

    [[nodiscard]] QString imapHost() const { return _imapHost; }
    void setImapHost(const QString &host) { _imapHost = host; }
    [[nodiscard]] int imapPort() const { return _imapPort; }
    void setImapPort(int port) { _imapPort = port; }
    [[nodiscard]] bool imapUseSsl() const { return _imapUseSsl; }
    void setImapUseSsl(bool ssl) { _imapUseSsl = ssl; }

    [[nodiscard]] QString smtpHost() const { return _smtpHost; }
    void setSmtpHost(const QString &host) { _smtpHost = host; }
    [[nodiscard]] int smtpPort() const { return _smtpPort; }
    void setSmtpPort(int port) { _smtpPort = port; }
    [[nodiscard]] bool smtpUseSsl() const { return _smtpUseSsl; }
    void setSmtpUseSsl(bool ssl) { _smtpUseSsl = ssl; }

    [[nodiscard]] QString username() const { return _username; }
    void setUsername(const QString &name) { _username = name; }
    [[nodiscard]] QString password() const { return _password; }
    void setPassword(const QString &pwd) { _password = pwd; }
    [[nodiscard]] QString emailAddress() const { return _emailAddress; }
    void setEmailAddress(const QString &addr) { _emailAddress = addr; }

    static QString deriveImapHost(const QString &souveraDomain);
    static QString deriveSmtpHost(const QString &souveraDomain);

signals:
    void connectionChanged();

private:
    QString _imapHost;
    int _imapPort = 993;
    bool _imapUseSsl = true;
    QString _smtpHost;
    int _smtpPort = 587;
    bool _smtpUseSsl = true;
    QString _username;
    QString _password;
    QString _emailAddress;
};

} // namespace OCC

#endif // MAILACCOUNT_H
