/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckOcsApi.h"

#include "account.h"
#include "accountstate.h"
#include "net/OcsDavClient.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QNetworkRequest>
#include <QPointer>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcDeckOcsApi, "souvera.deck.ocsapi")

namespace OCC {

DeckOcsApi::DeckOcsApi(QObject *parent)
    : QObject(parent)
{
}

void DeckOcsApi::setAccountState(AccountState *state)
{
    _accountState = state;
}

QString DeckOcsApi::apiUrl(const QString &path) const
{
    if (!_accountState || !_accountState->account()) return {};
    const auto base = _accountState->account()->url().toString();
    return QStringLiteral("%1/index.php/apps/deck/api/v1.0%2").arg(base, path);
}

void DeckOcsApi::fetchBoards()
{
    const auto url = apiUrl(QStringLiteral("/boards"));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "GET", QUrl(url), {},
        [this](const QJsonDocument &doc, int) {
            if (!doc.isArray() || doc.array().isEmpty()) {
                qCWarning(lcDeckOcsApi) << "Boards response is not a non-empty array; head:"
                                        << QString::fromUtf8(doc.toJson(QJsonDocument::Compact).left(200));
            }
            emit boardsReceived(doc.array());
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "fetchBoards failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::fetchStacks(int boardId)
{
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks").arg(boardId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "GET", QUrl(url), {},
        [this](const QJsonDocument &doc, int) {
            emit stacksReceived(doc.array());
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "fetchStacks failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::createStack(int boardId, const QString &title)
{
    // POST /boards/{boardId}/stacks (Android DeckAPI.createStack)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks").arg(boardId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("title")] = title;

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "POST", QUrl(url), payload,
        [this, boardId](const QJsonDocument &doc, int) {
            emit stackCreated(doc.object(), boardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "createStack failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::updateCard(int boardId, int stackId, int cardId, const QJsonObject &cardJson)
{
    // PUT /boards/{boardId}/stacks/{stackId}/cards/{cardId}
    // The Deck server validates the card PUT strictly: only the COMPLETE
    // object is accepted — partial payloads answer with HTTP 400.
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3")
                                .arg(boardId).arg(stackId).arg(cardId));
    if (url.isEmpty()) return;

    const auto payload = QJsonDocument(cardJson).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "PUT", QUrl(url), payload,
        [this, boardId, stackId](const QJsonDocument &doc, int) {
            emit cardUpdated(doc.object(), boardId, stackId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "updateCard failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::updateCardFields(int boardId, int stackId, const QJsonObject &cardJson,
                                  const QString &newTitle, const QString &newDescription)
{
    auto body = cardJson;
    body[QStringLiteral("title")] = newTitle;
    body[QStringLiteral("description")] = newDescription;
    updateCard(boardId, stackId, static_cast<int>(body.value(QStringLiteral("id")).toDouble()),
               body);
}

void DeckOcsApi::updateStack(int boardId, int stackId, const QString &title)
{
    // PUT /boards/{boardId}/stacks/{stackId} (Android DeckAPI.updateStack)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2").arg(boardId).arg(stackId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("title")] = title;

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "PUT", QUrl(url), payload,
        [this, boardId, stackId](const QJsonDocument &, int) {
            emit stackUpdated(boardId, stackId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "updateStack failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::deleteStack(int boardId, int stackId)
{
    // DELETE /boards/{boardId}/stacks/{stackId} (Android DeckAPI.deleteStack)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2").arg(boardId).arg(stackId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "DELETE", QUrl(url), {},
        [this, boardId, stackId](const QJsonDocument &, int) {
            emit stackDeleted(boardId, stackId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "deleteStack failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::fetchComments(int cardId)
{
    // GET /cards/{cardId}/comments (NextcloudServerAPI.getComments)
    const auto url = apiUrl(QStringLiteral("/cards/%1/comments").arg(cardId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "GET", QUrl(url), {},
        [this, cardId](const QJsonDocument &doc, int) {
            // The comments endpoint answers in the OCS envelope — unwrap it.
            const auto ocs = doc.object().value(QStringLiteral("ocs")).toObject();
            emit commentsReceived(cardId, ocs.value(QStringLiteral("data")).toArray());
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "fetchComments failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::createComment(int cardId, const QString &message)
{
    const auto url = apiUrl(QStringLiteral("/cards/%1/comments").arg(cardId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("message")] = message;

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "POST", QUrl(url), payload,
        [this, cardId](const QJsonDocument &, int) {
            emit commentCreated(cardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "createComment failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::updateComment(int cardId, int commentId, const QString &message)
{
    const auto url = apiUrl(QStringLiteral("/cards/%1/comments/%2").arg(cardId).arg(commentId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("message")] = message;

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "PUT", QUrl(url), payload,
        [this, cardId](const QJsonDocument &, int) {
            emit commentUpdated(cardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "updateComment failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::deleteComment(int cardId, int commentId)
{
    const auto url = apiUrl(QStringLiteral("/cards/%1/comments/%2").arg(cardId).arg(commentId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "DELETE", QUrl(url), {},
        [this, cardId](const QJsonDocument &, int) {
            emit commentDeleted(cardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "deleteComment failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::fetchAttachments(int boardId, int stackId, int cardId)
{
    // GET /boards/{b}/stacks/{s}/cards/{c}/attachments (DeckAPI.getAttachments)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3/attachments")
                                .arg(boardId).arg(stackId).arg(cardId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "GET", QUrl(url), {},
        [this, cardId](const QJsonDocument &doc, int) {
            emit attachmentsReceived(cardId, doc.array());
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "fetchAttachments failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::uploadAttachment(int boardId, int stackId, int cardId,
                                  const QString &filePath)
{
    // POST multipart /boards/{b}/stacks/{s}/cards/{c}/attachments
    // (DeckAPI.uploadAttachment). Since Deck 1.17.0 an EMPTY form part named
    // "data" is REQUIRED (nextcloud/deck#7681) and the type part switches
    // from "deck_file" to "file".
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return;

    auto *fileDevice = new QFile(filePath);
    if (!fileDevice->open(QIODevice::ReadOnly)) {
        delete fileDevice;
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    // The multiPart does NOT take ownership of the device implicitly — parent
    // it explicitly or the QFile (and its file descriptor) leaks per upload.
    fileDevice->setParent(multiPart);

    QHttpPart typePart;
    typePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"type\"")));
    typePart.setBody(QByteArray("file"));
    multiPart->append(typePart);

    QHttpPart dataPart;
    dataPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"data\"")));
    dataPart.setBody(QString().toUtf8());
    multiPart->append(dataPart);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"%1\"")
                                    .arg(info.fileName())));
    const QMimeDatabase mimeDb;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant(mimeDb.mimeTypeForFile(filePath).name()));
    filePart.setBodyDevice(fileDevice);
    multiPart->append(filePart);

    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3/attachments")
                                .arg(boardId).arg(stackId).arg(cardId));
    if (url.isEmpty()) {
        delete multiPart;
        return;
    }

    OcsDavClient::multipartRequest(_accountState, QUrl(url), multiPart,
        [this, cardId](const QJsonDocument &, int) {
            emit attachmentUploaded(cardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "uploadAttachment failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::deleteAttachment(int boardId, int stackId, int cardId, int attachmentId)
{
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3/attachments/%4?type=file")
                                .arg(boardId).arg(stackId).arg(cardId).arg(attachmentId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "DELETE", QUrl(url), {},
        [this, cardId](const QJsonDocument &, int) {
            emit attachmentDeleted(cardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "deleteAttachment failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::downloadAttachment(int boardId, int stackId, int cardId, int attachmentId,
                                    const QString &fileName)
{
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3/attachments/%4")
                                .arg(boardId).arg(stackId).arg(cardId).arg(attachmentId));
    if (url.isEmpty()) return;

    // Raw GET — the response body is the binary file itself, not JSON.
    OcsDavClient::binaryRequest(_accountState, "GET", QUrl(url), {},
        [this, fileName](const QByteArray &data, int) {
            const auto dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            const auto localPath = dir + QStringLiteral("/deck-attachment-%1").arg(fileName);
            QFile out(localPath);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                emit apiError(QStringLiteral("Anhang konnte nicht gespeichert werden."));
                return;
            }
            out.write(data);
            out.close();
            emit attachmentDownloaded(fileName, localPath);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "downloadAttachment failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::createCard(int boardId, int stackId, const QString &title, const QString &description)
{
    // POST /boards/{boardId}/stacks/{stackId}/cards (Android DeckAPI.createCard)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards").arg(boardId).arg(stackId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("title")] = title;
    body[QStringLiteral("description")] = description;
    body[QStringLiteral("type")] = QStringLiteral("plain");

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "POST", QUrl(url), payload,
        [this, boardId, stackId](const QJsonDocument &doc, int) {
            emit cardCreated(doc.object(), boardId, stackId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "createCard failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::moveCard(int boardId, int sourceStackId, int targetStackId, int cardId, int order)
{
    // PUT /boards/{boardId}/stacks/{sourceStackId}/cards/{cardId}/reorder
    // (Android DeckAPI.moveCard + Reorder.java: the URL carries the SOURCE
    // column, the JSON body carries the TARGET column in "stackId".)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3/reorder")
                                .arg(boardId).arg(sourceStackId).arg(cardId));
    if (url.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("stackId")] = targetStackId;
    body[QStringLiteral("order")] = order;

    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    OcsDavClient::jsonRequest(_accountState, "PUT", QUrl(url), payload,
        [this](const QJsonDocument &, int) {
            emit cardMoved();
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "moveCard failed:" << message;
            emit apiError(message);
        });
}

void DeckOcsApi::deleteCard(int boardId, int stackId, int cardId)
{
    // DELETE /boards/{boardId}/stacks/{stackId}/cards/{cardId} (Android DeckAPI.deleteCard)
    const auto url = apiUrl(QStringLiteral("/boards/%1/stacks/%2/cards/%3")
                                .arg(boardId).arg(stackId).arg(cardId));
    if (url.isEmpty()) return;

    OcsDavClient::jsonRequest(_accountState, "DELETE", QUrl(url), {},
        [this, boardId, stackId, cardId](const QJsonDocument &, int) {
            emit cardDeleted(boardId, stackId, cardId);
        },
        [this](int, const QString &message) {
            qCWarning(lcDeckOcsApi) << "deleteCard failed:" << message;
            emit apiError(message);
        });
}

} // namespace OCC
