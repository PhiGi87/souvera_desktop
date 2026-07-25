/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailAccount.h"
#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"

#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QUrl>

Q_LOGGING_CATEGORY(lcMailAccount, "souvera.mail.account")

namespace OCC {

MailAccount::MailAccount(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
{
}

MailAccount::~MailAccount()
{
    disconnectFromImap();
    if (_smtpSocket) {
        _smtpSocket->disconnectFromHost();
        _smtpSocket->deleteLater();
        _smtpSocket = nullptr;
    }
}

QString MailAccount::emailAddress() const
{
    auto *acc = _accountState->account();
    auto *creds = acc->credentials();
    return QStringLiteral("%1@%2").arg(creds->user(), acc->url().host());
}

QString MailAccount::userName() const
{
    return _accountState->account()->credentials()->user();
}

QString MailAccount::imapHost() const
{
    return deriveImapHost(_accountState->account()->url().host());
}

int MailAccount::imapPort() const
{
    return 993;
}

QString MailAccount::smtpHost() const
{
    return deriveSmtpHost(_accountState->account()->url().host());
}

int MailAccount::smtpPort() const
{
    return 465;
}

QString MailAccount::deriveImapHost(const QString &souveraDomain)
{
    return QStringLiteral("imap.%1").arg(souveraDomain);
}

QString MailAccount::deriveSmtpHost(const QString &souveraDomain)
{
    return QStringLiteral("smtp.%1").arg(souveraDomain);
}

// ── IMAP ─────────────────────────────────────────────────────────────────────

void MailAccount::connectImap()
{
    if (_imapSocket && _imapSocket->state() == QAbstractSocket::ConnectedState) {
        qCWarning(lcMailAccount) << "IMAP already connected";
        return;
    }

    setupImapSocket();

    auto host = imapHost();
    auto port = imapPort();

    qCInfo(lcMailAccount) << "Connecting to IMAP" << host << port;
    _imapSocket->connectToHostEncrypted(host, port);
}

void MailAccount::disconnectFromImap()
{
    if (!_imapSocket) return;
    if (_imapLoggedIn) {
        sendImapLine(QStringLiteral("a000 LOGOUT"));
    }
    _imapSocket->disconnectFromHost();
    _imapSocket->deleteLater();
    _imapSocket = nullptr;
    _imapLoggedIn = false;
}

bool MailAccount::isImapConnected() const
{
    return _imapSocket
        && _imapSocket->state() == QAbstractSocket::ConnectedState
        && _imapLoggedIn;
}

void MailAccount::setupImapSocket()
{
    if (_imapSocket) {
        _imapSocket->deleteLater();
        _imapSocket = nullptr;
    }

    _imapSocket = new QSslSocket(this);
    _imapSocket->setPeerVerifyMode(QSslSocket::QueryPeer);
    _imapBuf.clear();
    _imapLiteralRemaining = 0;
    _imapLiteralData.clear();
    _imapResponseLines.clear();
    _imapTagCounter = 0;
    _imapLoggedIn = false;

    connect(_imapSocket, &QSslSocket::encrypted, this, &MailAccount::onImapConnected);
    connect(_imapSocket, &QSslSocket::readyRead, this, &MailAccount::onImapReadyRead);
    connect(_imapSocket, &QAbstractSocket::errorOccurred, this, &MailAccount::onImapErrorOccurred);
    connect(_imapSocket, &QSslSocket::disconnected, this, &MailAccount::onImapDisconnected);
}

QString MailAccount::nextImapTag()
{
    return QStringLiteral("a%1").arg(++_imapTagCounter, 3, 10, QLatin1Char('0'));
}

void MailAccount::sendImapLine(const QString &line)
{
    if (!_imapSocket || _imapSocket->state() != QAbstractSocket::ConnectedState) return;
    _imapSocket->write(line.toUtf8() + "\r\n");
}

void MailAccount::onImapConnected()
{
    qCInfo(lcMailAccount) << "IMAP SSL connected, sending LOGIN";
    auto *creds = _accountState->account()->credentials();

    auto user = creds->user();
    auto pass = creds->password();

    auto tag = nextImapTag();
    _lastTag = tag;
    sendImapLine(QStringLiteral("%1 LOGIN \"%2\" \"%3\"").arg(tag, user, pass));
}

void MailAccount::onImapReadyRead()
{
    _imapBuf.append(_imapSocket->readAll());

    while (true) {
        if (_imapLiteralRemaining > 0) {
            auto chunk = _imapBuf.left(_imapLiteralRemaining);
            _imapLiteralData.append(chunk);
            _imapBuf.remove(0, chunk.size());
            _imapLiteralRemaining -= chunk.size();

            if (_imapLiteralRemaining == 0) {
                _imapResponseLines.append(_imapLiteralData);
                _imapLiteralData.clear();
            }
            continue;
        }

        auto nlIdx = _imapBuf.indexOf('\n');
        if (nlIdx < 0) break;

        auto raw = _imapBuf.left(nlIdx);
        _imapBuf.remove(0, nlIdx + 1);

        if (!raw.isEmpty() && raw.at(raw.size() - 1) == '\r') {
            raw.chop(1);
        }

        int literalSize = 0;
        auto braceStart = raw.lastIndexOf('{');
        auto braceEnd = raw.lastIndexOf('}');
        if (braceStart >= 0 && braceEnd == raw.size() - 1) {
            auto sizeStr = QString::fromUtf8(raw.mid(braceStart + 1, braceEnd - braceStart - 1));
            literalSize = sizeStr.toInt();
        }

        if (literalSize > 0) {
            _imapLiteralRemaining = literalSize;
            _imapLiteralData.clear();
            _imapResponseLines.append(raw);
            continue;
        }

        _imapResponseLines.append(raw);

        auto lineStr = QString::fromUtf8(raw);
        static QRegularExpression tagRe(R"(^a\d{3}\s+(OK|NO|BAD)\s)");
        auto tagMatch = tagRe.match(lineStr);
        if (tagMatch.hasMatch()) {
            handleImapTaggedResponse(_lastTag, _imapResponseLines);
            _imapResponseLines.clear();
        }
    }
}

void MailAccount::handleImapTaggedResponse(const QString &tag, const QList<QByteArray> &lines)
{
    if (lines.isEmpty()) return;

    auto lastLine = QString::fromUtf8(lines.last());
    auto isOk = lastLine.contains(QStringLiteral("OK"));

    if (tag == _lastTag) {
        if (lastLine.contains(QStringLiteral("LOGIN"))) {
            handleTagLogin(lastLine);
        } else if (lastLine.contains(QStringLiteral("LIST"))) {
            if (isOk) handleTagList(lines);
        } else if (lastLine.contains(QStringLiteral("SELECT"))) {
            if (isOk) handleTagSelect(lastLine, lines);
        } else if (lastLine.contains(QStringLiteral("FETCH"))) {
            if (isOk) {
                auto isBody = lastLine.contains(QStringLiteral("BODY"));
                if (isBody) {
                    handleTagBody(lines);
                } else {
                    handleTagFetch(lines);
                }
            }
        }
    }
}

void MailAccount::handleTagLogin(const QString &response)
{
    if (response.contains(QStringLiteral("OK"))) {
        _imapLoggedIn = true;
        qCInfo(lcMailAccount) << "IMAP login successful";
        emit imapConnected();
    } else {
        qCWarning(lcMailAccount) << "IMAP login failed:" << response;
        emit imapConnectionError(QStringLiteral("IMAP-Login fehlgeschlagen"));
    }
}

void MailAccount::handleTagList(const QList<QByteArray> &lines)
{
    QList<ImapFolderData> folders;

    static QRegularExpression listRe(R"(\* LIST\s*\([^)]*\)\s*"[^"]*"\s*"([^"]+)")");
    static QRegularExpression listNoAttrRe(R"(\* LIST\s*\(\)\s*"[^"]*"\s*"([^"]+)")");

    for (const auto &raw : lines) {
        auto line = QString::fromUtf8(raw);

        auto match = listRe.match(line);
        if (!match.hasMatch()) {
            match = listNoAttrRe.match(line);
        }
        if (match.hasMatch()) {
            ImapFolderData folder;
            folder.name = match.captured(1);
            folders.append(folder);
        }
    }

    qCInfo(lcMailAccount) << "Fetched" << folders.size() << "folders";
    emit foldersFetched(folders);
}

void MailAccount::handleTagSelect(const QString &response, const QList<QByteArray> &lines)
{
    Q_UNUSED(response)

    int exists = 0;
    for (const auto &raw : lines) {
        auto line = QString::fromUtf8(raw);
        static QRegularExpression existsRe(R"(\*\s+(\d+)\s+EXISTS)");
        auto match = existsRe.match(line);
        if (match.hasMatch()) {
            exists = match.captured(1).toInt();
        }
    }

    qCInfo(lcMailAccount) << "Folder selected, messages:" << exists;

    if (exists > 0) {
        auto tag = nextImapTag();
        _lastTag = tag;
        auto endSeq = exists;
        sendImapLine(QStringLiteral("%1 FETCH 1:%2 (FLAGS INTERNALDATE BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])")
                         .arg(tag).arg(endSeq));
    } else {
        emit messagesFetched({});
    }
}

void MailAccount::handleTagFetch(const QList<QByteArray> &lines)
{
    QList<ImapMessageData> messages;

    QByteArray headerBuffer;
    bool inHeader = false;
    bool headerPending = false;
    int currentSeq = 0;
    bool currentSeen = false;

    for (const auto &raw : lines) {
        auto line = QString::fromUtf8(raw);

        static QRegularExpression fetchStartRe(R"(\*\s+(\d+)\s+FETCH\s+\()");
        auto startMatch = fetchStartRe.match(line);
        if (startMatch.hasMatch()) {
            if (currentSeq > 0 && !headerBuffer.isEmpty()) {
                processMessageBuffer(headerBuffer, messages, currentSeq, currentSeen);
            }

            currentSeq = startMatch.captured(1).toInt();
            currentSeen = line.contains(QStringLiteral("\\Seen"));
            headerBuffer.clear();
            inHeader = false;
            headerPending = false;
            continue;
        }

        static QRegularExpression literalRe(R"(\{(\d+)\}$)");
        auto litMatch = literalRe.match(line);
        if (litMatch.hasMatch()) {
            inHeader = true;
            headerPending = true;
            continue;
        }

        if (inHeader && headerPending) {
            headerBuffer.append(raw);
            headerBuffer.append('\n');
            headerPending = false;
            inHeader = false;
            continue;
        }
    }

    if (currentSeq > 0 && !headerBuffer.isEmpty()) {
        processMessageBuffer(headerBuffer, messages, currentSeq, currentSeen);
    }

    qCInfo(lcMailAccount) << "Fetched" << messages.size() << "messages";
    emit messagesFetched(messages);
}

void MailAccount::processMessageBuffer(const QByteArray &buf, QList<ImapMessageData> &messages, int seq, bool seen)
{
    ImapMessageData msg;
    msg.seq = seq;
    msg.seen = seen;

    auto headerText = QString::fromUtf8(buf);
    for (const auto &hdrLine : headerText.split(QStringLiteral("\n"), Qt::SkipEmptyParts)) {
        auto trimmed = hdrLine.trimmed();
        auto upper = trimmed.toUpper();
        if (upper.startsWith(QStringLiteral("FROM:"))) {
            msg.from = trimmed.mid(5).trimmed();
        } else if (upper.startsWith(QStringLiteral("SUBJECT:"))) {
            msg.subject = trimmed.mid(8).trimmed();
        } else if (upper.startsWith(QStringLiteral("DATE:"))) {
            auto dateStr = trimmed.mid(5).trimmed();
            msg.dateTime = QDateTime::fromString(dateStr, Qt::RFC2822Date);
            if (!msg.dateTime.isValid()) {
                msg.dateTime = QDateTime::fromString(dateStr, Qt::ISODate);
            }
        }
    }
    messages.append(msg);
}

void MailAccount::handleTagBody(const QList<QByteArray> &lines)
{
    QString htmlBody;
    QString plainBody;

    bool inBody = false;
    int bodySeq = 0;

    for (const auto &raw : lines) {
        auto line = QString::fromUtf8(raw);

        static QRegularExpression fetchRe(R"(\*\s+(\d+)\s+FETCH)");
        auto match = fetchRe.match(line);
        if (match.hasMatch()) {
            bodySeq = match.captured(1).toInt();
        }

        static QRegularExpression literalRe(R"(\{(\d+)\}$)");
        auto litMatch = literalRe.match(line);
        if (litMatch.hasMatch()) {
            inBody = true;
            continue;
        }

        if (inBody) {
            auto text = QString::fromUtf8(raw);
            if (text.contains(QStringLiteral("text/html")) || text.contains(QStringLiteral("text/plain"))) {
                continue;
            }
            if (raw.size() > 0 && raw.at(0) != '(' && raw.at(0) != ')') {
                plainBody = text;
                break;
            }
            inBody = false;
        }
    }

    if (bodySeq > 0) {
        qCInfo(lcMailAccount) << "Body fetched for seq:" << bodySeq;
        emit bodyFetched(bodySeq, htmlBody, plainBody);
    }
}

void MailAccount::onImapErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    qCWarning(lcMailAccount) << "IMAP socket error:" << _imapSocket->errorString();
    emit imapConnectionError(_imapSocket->errorString());
    _imapLoggedIn = false;
}

void MailAccount::onImapDisconnected()
{
    qCInfo(lcMailAccount) << "IMAP disconnected";
    _imapLoggedIn = false;
    emit imapDisconnected();
}

void MailAccount::fetchFolders()
{
    if (!_imapLoggedIn) {
        qCWarning(lcMailAccount) << "Cannot fetch folders: not logged in";
        return;
    }

    auto tag = nextImapTag();
    _lastTag = tag;
    sendImapLine(QStringLiteral("%1 LIST \"\" \"*\"").arg(tag));
    qCInfo(lcMailAccount) << "Fetching folders...";
}

void MailAccount::fetchMessages(const QString &folderName)
{
    if (!_imapLoggedIn) {
        qCWarning(lcMailAccount) << "Cannot fetch messages: not logged in";
        return;
    }

    _pendingFolderName = folderName;
    auto escaped = QString(folderName);
    escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));

    auto tag = nextImapTag();
    _lastTag = tag;
    sendImapLine(QStringLiteral("%1 SELECT \"%2\"").arg(tag, escaped));
    qCInfo(lcMailAccount) << "Selecting folder:" << folderName;
}

void MailAccount::fetchBody(int seq)
{
    if (!_imapLoggedIn) {
        qCWarning(lcMailAccount) << "Cannot fetch body: not logged in";
        return;
    }

    _pendingBodySeq = seq;

    auto tag = nextImapTag();
    _lastTag = tag;
    sendImapLine(QStringLiteral("%1 FETCH %2 (BODY.PEEK[])").arg(tag).arg(seq));
    qCInfo(lcMailAccount) << "Fetching body for seq:" << seq;
}

// ── SMTP ─────────────────────────────────────────────────────────────────────

void MailAccount::sendMail(const QString &to, const QString &cc, const QString &bcc,
                           const QString &subject, const QString &body)
{
    _smtpFrom = emailAddress();
    _smtpTo = to;
    _smtpCc = cc;
    _smtpBcc = bcc;
    _smtpSubject = subject;
    _smtpBody = body;
    _smtpRecipients.clear();

    static QRegularExpression sepRe(R"([;,]\s*)");
    for (const auto &r : to.split(sepRe)) {
        if (!r.trimmed().isEmpty()) _smtpRecipients << r.trimmed();
    }
    for (const auto &r : cc.split(sepRe)) {
        if (!r.trimmed().isEmpty()) _smtpRecipients << r.trimmed();
    }
    for (const auto &r : bcc.split(sepRe)) {
        if (!r.trimmed().isEmpty()) _smtpRecipients << r.trimmed();
    }

    if (_smtpRecipients.isEmpty()) {
        emit messageSent(false, QStringLiteral("Keine Empfänger angegeben"));
        return;
    }

    _smtpStep = 0;
    _smtpRcptIndex = 0;
    connectSmtp();
}

void MailAccount::setupSmtpSocket()
{
    if (_smtpSocket) {
        _smtpSocket->disconnectFromHost();
        _smtpSocket->deleteLater();
        _smtpSocket = nullptr;
    }

    _smtpSocket = new QSslSocket(this);
    _smtpSocket->setPeerVerifyMode(QSslSocket::QueryPeer);
    _smtpBuf.clear();

    connect(_smtpSocket, &QSslSocket::encrypted, this, &MailAccount::onSmtpConnected);
    connect(_smtpSocket, &QSslSocket::readyRead, this, &MailAccount::onSmtpReadyRead);
    connect(_smtpSocket, &QAbstractSocket::errorOccurred, this, &MailAccount::onSmtpErrorOccurred);
}

void MailAccount::connectSmtp()
{
    setupSmtpSocket();
    _smtpSocket->connectToHostEncrypted(smtpHost(), smtpPort());
}

void MailAccount::onSmtpConnected()
{
    qCInfo(lcMailAccount) << "SMTP connected";
    _smtpStep = 1;
    _smtpSocket->write(QStringLiteral("EHLO %1\r\n").arg(_accountState->account()->url().host()).toUtf8());
}

void MailAccount::onSmtpReadyRead()
{
    _smtpBuf.append(_smtpSocket->readAll());

    while (true) {
        auto nlIdx = _smtpBuf.indexOf('\n');
        if (nlIdx < 0) break;

        auto line = _smtpBuf.left(nlIdx);
        _smtpBuf.remove(0, nlIdx + 1);
        if (line.endsWith('\r')) line.chop(1);

        if (line.size() < 3) continue;
        auto code = line.left(3).toInt();

        if (line.size() > 3 && line.at(3) == '-') {
            continue;
        }

        auto *creds = _accountState->account()->credentials();

        switch (_smtpStep) {
        case 1:
            if (code == 250) {
                _smtpStep = 2;
                _smtpSocket->write(QStringLiteral("AUTH LOGIN\r\n").toUtf8());
            } else {
                emit messageSent(false, QStringLiteral("SMTP EHLO fehlgeschlagen"));
                _smtpSocket->disconnectFromHost();
            }
            break;
        case 2:
            if (code == 334) {
                _smtpStep = 3;
                _smtpSocket->write(creds->user().toUtf8().toBase64() + "\r\n");
            } else {
                emit messageSent(false, QStringLiteral("SMTP AUTH fehlgeschlagen"));
                _smtpSocket->disconnectFromHost();
            }
            break;
        case 3:
            if (code == 334) {
                _smtpStep = 4;
                _smtpSocket->write(creds->password().toUtf8().toBase64() + "\r\n");
            } else {
                emit messageSent(false, QStringLiteral("SMTP Benutzername abgelehnt"));
                _smtpSocket->disconnectFromHost();
            }
            break;
        case 4:
            if (code == 235) {
                _smtpStep = 5;
                _smtpSocket->write(QStringLiteral("MAIL FROM:<%1>\r\n").arg(_smtpFrom).toUtf8());
            } else {
                emit messageSent(false, QStringLiteral("SMTP Passwort abgelehnt"));
                _smtpSocket->disconnectFromHost();
            }
            break;
        case 5:
            if (code == 250) {
                _smtpStep = 6;
                _smtpRcptIndex = 0;
                _smtpSocket->write(QStringLiteral("RCPT TO:<%1>\r\n").arg(_smtpRecipients[0]).toUtf8());
            } else {
                emit messageSent(false, QStringLiteral("SMTP MAIL FROM fehlgeschlagen"));
                _smtpSocket->disconnectFromHost();
            }
            break;
        case 6:
            if (code == 250 || code == 251) {
                _smtpRcptIndex++;
                if (_smtpRcptIndex < _smtpRecipients.size()) {
                    _smtpSocket->write(QStringLiteral("RCPT TO:<%1>\r\n").arg(_smtpRecipients[_smtpRcptIndex]).toUtf8());
                } else {
                    _smtpStep = 7;
                    _smtpSocket->write(QStringLiteral("DATA\r\n").toUtf8());
                }
            } else {
                emit messageSent(false, QStringLiteral("SMTP RCPT TO fehlgeschlagen für %1")
                                    .arg(_smtpRecipients.value(_smtpRcptIndex)));
                _smtpSocket->disconnectFromHost();
            }
            break;
        case 7:
            if (code == 354) {
                _smtpStep = 8;
                auto mailData = QStringLiteral(
                    "From: %1\r\n"
                    "To: %2\r\n"
                    "Subject: %3\r\n"
                    "MIME-Version: 1.0\r\n"
                    "Content-Type: text/plain; charset=UTF-8\r\n"
                    "Content-Transfer-Encoding: 8bit\r\n"
                    "\r\n"
                    "%4\r\n"
                    ".\r\n")
                    .arg(_smtpFrom, _smtpTo, _smtpSubject, _smtpBody);
                _smtpSocket->write(mailData.toUtf8());
            } else {
                emit messageSent(false, QStringLiteral("SMTP DATA fehlgeschlagen"));
                _smtpSocket->disconnectFromHost();
            }
            break;
        case 8:
            if (code == 250) {
                qCInfo(lcMailAccount) << "Mail sent successfully";
                emit messageSent(true, {});
                _smtpStep = 9;
                _smtpSocket->write(QStringLiteral("QUIT\r\n").toUtf8());
                _smtpSocket->disconnectFromHost();
            } else {
                emit messageSent(false, QStringLiteral("SMTP Senden fehlgeschlagen"));
                _smtpSocket->disconnectFromHost();
            }
            break;
        default:
            break;
        }
    }
}

void MailAccount::onSmtpErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    qCWarning(lcMailAccount) << "SMTP error:" << _smtpSocket->errorString();
    emit messageSent(false, _smtpSocket->errorString());
}

} // namespace OCC
