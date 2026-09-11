/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKCARDDETAILDIALOG_H
#define DECKCARDDETAILDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QVector>

#include "DeckModels.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTextBrowser;
class QVBoxLayout;
class QListWidget;
class QDateEdit;

namespace OCC {

class DeckOcsApi;
class DeckCommentsWidget;

/**
 * @brief Two-column card detail dialog: content (markdown, comments) on the
 *        left, metadata (dates, color, labels, assignees, attachments) on
 *        the right. Saves via the complete-card PUT.
 */
class DeckCardDetailDialog : public QDialog
{
    Q_OBJECT
public:
    DeckCardDetailDialog(DeckOcsApi *api, int boardId, const QJsonObject &cardJson,
                         const QVector<DeckLabel> &boardLabels,
                         const QVector<DeckUser> &boardMembers,
                         bool supportsStartDate, bool supportsCardColor,
                         QWidget *parent = nullptr);

    [[nodiscard]] QJsonObject cardJson() const { return _cardJson; }

signals:
    void cardSaved(int boardId, int stackId, int cardId, const QJsonObject &updatedJson);

private slots:
    void editDescription();
    void saveAll();
    void onAttachmentsReceived(int cardId, const QVector<DeckAttachment> &attachments);
    void onAttachmentUploaded(int cardId);
    void uploadAttachment();
    void openAttachment(const DeckAttachment &attachment);
    void deleteAttachment(int attachmentId);

private:
    void buildUi();
    void renderLabels();
    void toggleLabel(int labelId, bool checked);
    void applyColor(const QString &colorName);
    void markDirty();

    DeckOcsApi *_api = nullptr;
    int _boardId = -1;
    QJsonObject _cardJson;
    DeckCard _card;
    QVector<DeckLabel> _boardLabels;
    QVector<DeckUser> _boardMembers;
    bool _supportsStartDate = true;
    bool _supportsCardColor = true;

    QLineEdit *_titleEdit = nullptr;
    QTextBrowser *_descriptionView = nullptr;
    QPlainTextEdit *_descriptionEdit = nullptr;
    QLabel *_colorIndicator = nullptr;
    QWidget *_labelChips = nullptr;
    QVBoxLayout *_labelChipsLayout = nullptr;
    QDateEdit *_dueEdit = nullptr;
    QDateEdit *_startEdit = nullptr;
    QListWidget *_attachmentList = nullptr;
    DeckCommentsWidget *_comments = nullptr;
    bool _descriptionEditing = false;
};

} // namespace OCC

#endif // DECKCARDDETAILDIALOG_H
