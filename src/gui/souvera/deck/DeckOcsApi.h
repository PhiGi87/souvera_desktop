/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKOCSAPI_H
#define DECKOCSAPI_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

namespace OCC {

class AccountState;

class DeckOcsApi : public QObject
{
    Q_OBJECT
public:
    explicit DeckOcsApi(QObject *parent = nullptr);
    ~DeckOcsApi() override = default;

    void setAccountState(AccountState *state);

    void fetchBoards();
    void fetchStacks(int boardId);
    void createStack(int boardId, const QString &title);
    void updateStack(int boardId, int stackId, const QString &title);
    void deleteStack(int boardId, int stackId);
    /** PUT with the COMPLETE card object — the Deck API 400s on partials. */
    void updateCard(int boardId, int stackId, int cardId, const QJsonObject &cardJson);
    void updateCardFields(int boardId, int stackId, const QJsonObject &cardJson,
                          const QString &newTitle, const QString &newDescription);

    // Comments (base: /cards/{cardId}/comments)
    void fetchComments(int cardId);
    void createComment(int cardId, const QString &message);
    void updateComment(int cardId, int commentId, const QString &message);
    void deleteComment(int cardId, int commentId);

    // Attachments
    void fetchAttachments(int boardId, int stackId, int cardId);
    void uploadAttachment(int boardId, int stackId, int cardId,
                          const QString &filePath);
    void deleteAttachment(int boardId, int stackId, int cardId, int attachmentId);
    void downloadAttachment(int boardId, int stackId, int cardId, int attachmentId,
                            const QString &fileName);
    void createCard(int boardId, int stackId, const QString &title, const QString &description);
    void moveCard(int boardId, int sourceStackId, int targetStackId, int cardId, int order);
    void deleteCard(int boardId, int stackId, int cardId);

signals:
    void boardsReceived(const QJsonArray &boards);
    void stacksReceived(const QJsonArray &stacks);
    void cardCreated(const QJsonObject &card, int boardId, int stackId);
    void cardMoved();
    void cardUpdated(const QJsonObject &card, int boardId, int stackId);
    void stackCreated(const QJsonObject &stack, int boardId);
    void stackUpdated(int boardId, int stackId);
    void stackDeleted(int boardId, int stackId);
    void cardDeleted(int boardId, int stackId, int cardId);
    void commentsReceived(int cardId, const QJsonArray &comments);
    void commentCreated(int cardId);
    void commentUpdated(int cardId);
    void commentDeleted(int cardId);
    void attachmentsReceived(int cardId, const QJsonArray &attachments);
    void attachmentUploaded(int cardId);
    void attachmentDeleted(int cardId);
    void attachmentDownloaded(const QString &fileName, const QString &localPath);
    void apiError(const QString &message);

private:
    [[nodiscard]] QString apiUrl(const QString &path) const;

    AccountState *_accountState = nullptr;
};

} // namespace OCC

#endif // DECKOCSAPI_H
