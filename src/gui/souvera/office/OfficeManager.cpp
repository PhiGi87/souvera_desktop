/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OfficeManager.h"
#include "OfficeWindow.h"

#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"

#include <QApplication>
#include <QDir>
#include <QHash>
#include <QFile>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcOfficeManager, "souvera.office.manager")

namespace OCC {

namespace {
QByteArray authBytes(AccountState *accountState)
{
    const auto acc = accountState->account();
    const auto creds = acc ? acc->credentials() : nullptr;
    if (!creds) return {};
    return QStringLiteral("%1:%2").arg(creds->user(), creds->password()).toUtf8().toBase64();
}
}

OfficeManager *OfficeManager::instance()
{
    static auto *manager = new OfficeManager(qApp);
    return manager;
}

OfficeManager::OfficeManager(QObject *parent)
    : QObject(parent)
{
}

QUrl OfficeManager::davUrlFor(AccountState *accountState, const QString &remotePath) const
{
    const auto acc = accountState->account();
    auto url = acc->url();
    url.setPath(acc->url().path() + QStringLiteral("/remote.php/dav/files/%1/%2")
        .arg(acc->davUser(), remotePath));
    return url;
}

QUrl OfficeManager::collaboraUrlFor(AccountState *accountState, const QString &fileId,
                                    const QString &remotePath) const
{
    const auto acc = accountState->account();
    const auto base = acc->url().toString();
    if (!fileId.isEmpty()) {
        return QUrl(QStringLiteral("%1/index.php/apps/richdocuments/index?fileId=%2").arg(base, fileId));
    }
    return QUrl(QStringLiteral("%1/index.php/apps/richdocuments/index?path=/%2").arg(base, remotePath));
}

void OfficeManager::openDocument(AccountState *accountState, const QString &remotePath,
                                 const QString &fileName, const QString &fileId,
                                 const QString &localPathIfSynced)
{
    if (!accountState) return;

    if (!localPathIfSynced.isEmpty() && QFile::exists(localPathIfSynced)) {
        // Edit the synced file in place; the sync engine uploads changes.
        openWindow(localPathIfSynced, fileName, accountState, remotePath,
                   davUrlFor(accountState, remotePath), false);
        return;
    }

    downloadThenOpen(accountState, davUrlFor(accountState, remotePath),
                     remotePath, fileName, fileId);
}

void OfficeManager::downloadThenOpen(AccountState *accountState, const QUrl &davUrl,
                                     const QString &remotePath, const QString &fileName,
                                     const QString &fileId)
{
    const auto acc = accountState->account();

    QNetworkRequest req(davUrl);
    const auto auth = authBytes(accountState);
    if (!auth.isEmpty()) {
        req.setRawHeader("Authorization", "Basic " + auth);
    }

    auto *reply = acc->networkAccessManager()->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, accountState, davUrl, remotePath, fileName, fileId, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(nullptr, QStringLiteral("Souvera Office"),
                QStringLiteral("Dokument konnte nicht geladen werden:\n%1").arg(reply->errorString()));
            return;
        }

        const auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/office-cache");
        QDir().mkpath(cacheDir);
        const auto localPath = QDir(cacheDir).filePath(fileName);

        QFile out(localPath);
        if (!out.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(nullptr, QStringLiteral("Souvera Office"),
                QStringLiteral("Dokument konnte nicht lokal gespeichert werden."));
            return;
        }
        out.write(reply->readAll());
        out.close();

        openWindow(localPath, fileName, accountState, remotePath, davUrl, true);
        Q_UNUSED(fileId)
    });
}

void OfficeManager::openWindow(const QString &localPath, const QString &fileName,
                               AccountState *accountState, const QString &remotePath,
                               const QUrl &davUrl, bool uploadOnSave)
{
    auto *window = new OfficeWindow(localPath, fileName,
                                    collaboraUrlFor(accountState, QString(), remotePath), nullptr);
    window->show();

    OpenDocument doc;
    doc.window = window;
    doc.accountState = accountState;
    doc.remotePath = remotePath;
    doc.davUrl = davUrl;
    doc.localPath = localPath;
    doc.uploadOnSave = uploadOnSave;
    _open.insert(window, doc);

    connect(window, &OfficeWindow::windowClosed, this, &OfficeManager::onWindowClosed);
    connect(window, &OfficeWindow::documentSaved, this, &OfficeManager::onDocumentSaved);

    lockRemote(accountState, davUrl, window);
}

void OfficeManager::lockRemote(AccountState *accountState, const QUrl &davUrl,
                               OfficeWindow *window)
{
    const auto acc = accountState->account();

    QNetworkRequest req(davUrl);
    const auto auth = authBytes(accountState);
    if (!auth.isEmpty()) {
        req.setRawHeader("Authorization", "Basic " + auth);
    }
    req.setRawHeader("Timeout", "Second-3600");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml");

    const auto body = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
        "<d:lockinfo xmlns:d=\"DAV:\">"
        "<d:lockscope><d:exclusive/></d:lockscope>"
        "<d:locktype><d:write/></d:locktype>"
        "<d:owner>Souvera Workspace</d:owner>"
        "</d:lockinfo>").toUtf8();

    auto *reply = acc->networkAccessManager()->sendCustomRequest(req, "LOCK", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, window]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // Locking is best-effort: read-only or offline servers just skip it.
            qCWarning(lcOfficeManager) << "LOCK failed:" << reply->errorString();
            return;
        }
        // The lock token is returned in the response body XML
        // (<D:locktoken><D:href>opaquelocktoken:...</D:href></D:locktoken>)
        // and optionally in a Lock-Token header. Try both.
        auto body = QString::fromUtf8(reply->readAll());
        auto token = QString::fromLatin1(reply->rawHeader("Lock-Token"));
        if (token.isEmpty()) {
            const auto match = QRegularExpression(
                QStringLiteral("locktoken[^>]*>\s*<[^>]*>\s*<?(opaquelocktoken:[^<\s>]*)")).match(body);
            if (match.hasMatch()) {
                token = match.captured(1);
            }
        }
        if (token.startsWith(QLatin1Char('<'))) token.remove(0, 1);
        if (token.endsWith(QLatin1Char('>'))) token.chop(1);
        if (!token.isEmpty()) {
            auto it = _open.find(window);
            if (it != _open.end()) {
                it.value().lockToken = token;
            }
        }
    });
}

void OfficeManager::unlockRemote(AccountState *accountState, const QUrl &davUrl,
                                 const QString &token)
{
    if (token.isEmpty()) return;

    const auto acc = accountState->account();

    QNetworkRequest req(davUrl);
    const auto auth = authBytes(accountState);
    if (!auth.isEmpty()) {
        req.setRawHeader("Authorization", "Basic " + auth);
    }
    req.setRawHeader("Lock-Token", QStringLiteral("<%1>").arg(token).toLatin1());

    auto *reply = acc->networkAccessManager()->sendCustomRequest(req, "UNLOCK");
    connect(reply, &QNetworkReply::finished, reply, [reply]() {
        reply->deleteLater();
    });
}

void OfficeManager::uploadFile(AccountState *accountState, const QUrl &davUrl,
                               const QString &localPath)
{
    const auto acc = accountState->account();

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QNetworkRequest req(davUrl);
    const auto auth = authBytes(accountState);
    if (!auth.isEmpty()) {
        req.setRawHeader("Authorization", "Basic " + auth);
    }
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    auto *reply = acc->networkAccessManager()->put(req, file.readAll());
    connect(reply, &QNetworkReply::finished, reply, [reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcOfficeManager) << "Upload failed:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void OfficeManager::onDocumentSaved(const QString &localPath)
{
    auto *window = qobject_cast<OfficeWindow *>(sender());
    if (!window) return;

    const auto it = _open.constFind(window);
    if (it == _open.constEnd()) return;

    if (it.value().uploadOnSave) {
        uploadFile(it.value().accountState, it.value().davUrl, localPath);
    }
}

void OfficeManager::onWindowClosed(OfficeWindow *window)
{
    const auto it = _open.constFind(window);
    if (it == _open.constEnd()) return;

    if (!it.value().lockToken.isEmpty()) {
        unlockRemote(it.value().accountState, it.value().davUrl, it.value().lockToken);
    }
    _open.erase(it);
}

} // namespace OCC
