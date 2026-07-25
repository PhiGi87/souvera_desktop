/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILACCOUNT_H
#define MAILACCOUNT_H

#include <QObject>
#include <QString>
#include <QSslSocket>
#include <QByteArray>
#include <QList>
#include <QDateTime>

namespace OCC {

class AccountState;

struct ImapFolderData {
    QString name;
    int exists = 0;
    int unseen = 0;
};

struct ImapMessageData {
    int seq = 0;
    int uid = 0;
    QString from;
    QString subject;
    QDateTime dateTime;
    bool seen = false;
    bool deleted = false;
};

class MailAccount : public QObject
{
    Q_OBJECT
public:
    explicit MailAccount(AccountState *accountState, QObject *parent = nullptr);
    ~MailAccount() override;

    void connectImap();
    void disconnectFromImap();
    [[nodiscard]] bool isImapConnected() const;

    void fetchFolders();
    void fetchMessages(const QString &folderName);
    void fetchBody(int seq);
    void sendMail(const QString &to, const QString &cc, const QString &bcc,
                  const QString &subject, const QString &body);

    AccountState *accountState() const { return _accountState; }

    [[nodiscard]] QString emailAddress() const;
    [[nodiscard]] QString userName() const;

    [[nodiscard]] QString imapHost() const;
    [[nodiscard]] int imapPort() const;
    [[nodiscard]] QString smtpHost() const;
    [[nodiscard]] int smtpPort() const;

    static QString deriveImapHost(const QString &souveraDomain);
    static QString deriveSmtpHost(const QString &souveraDomain);

signals:
    void imapConnected();
    void imapDisconnected();
    void imapConnectionError(const QString &error);
    void foldersFetched(const QList<ImapFolderData> &folders);
    void messagesFetched(const QList<ImapMessageData> &messages);
    void bodyFetched(int seq, const QString &htmlBody, const QString &plainBody);
    void messageSent(bool success, const QString &errorMsg);

private:
    void setupImapSocket();
    void onImapConnected();
    void onImapReadyRead();
    void onImapErrorOccurred(QAbstractSocket::SocketError error);
    void onImapDisconnected();

    QString nextImapTag();
    void sendImapLine(const QString &line);
    void handleImapTaggedResponse(const QString &tag, const QList<QByteArray> &lines);
    void handleTagLogin(const QString &response);
    void handleTagList(const QList<QByteArray> &lines);
    void handleTagSelect(const QString &response, const QList<QByteArray> &lines);
    void handleTagFetch(const QList<QByteArray> &lines);
    void processMessageBuffer(const QByteArray &buf, QList<ImapMessageData> &messages, int seq, bool seen);
    void handleTagBody(const QList<QByteArray> &lines);

    AccountState *_accountState;

    QSslSocket *_imapSocket = nullptr;
    QByteArray _imapBuf;
    int _imapLiteralRemaining = 0;
    QByteArray _imapLiteralData;
    QList<QByteArray> _imapResponseLines;
    int _imapTagCounter = 0;
    QString _lastTag;
    bool _imapLoggedIn = false;

    QString _pendingFolderName;
    int _pendingBodySeq = 0;

    // SMTP
    QSslSocket *_smtpSocket = nullptr;
    QByteArray _smtpBuf;
    int _smtpStep = 0;
    QStringList _smtpRecipients;
    int _smtpRcptIndex = 0;
    QString _smtpFrom;
    QString _smtpTo;
    QString _smtpCc;
    QString _smtpBcc;
    QString _smtpSubject;
    QString _smtpBody;

    void setupSmtpSocket();
    void connectSmtp();
    void onSmtpConnected();
    void onSmtpReadyRead();
    void onSmtpErrorOccurred(QAbstractSocket::SocketError error);
    void smtpAdvance();
};

} // namespace OCC

#endif // MAILACCOUNT_H
