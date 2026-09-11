/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DeckCardDetailDialog.h"
#include "DeckCommentsWidget.h"
#include "DeckManager.h"
#include "DeckOcsApi.h"
#include "theme/SouveraTheme.h"

#include <QDateEdit>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace OCC {

namespace {
constexpr const char *kDeckPalette[] = {
    "00a2e2", "3179c2", "bd1e1e", "d35400", "efb000",
    "7bd14a", "4a8b44", "2d7623", "9d56d2", "c04f90",
    "5bc8c4", "775c1d",
};
}

DeckCardDetailDialog::DeckCardDetailDialog(DeckOcsApi *api, int boardId,
                                           const QJsonObject &cardJson,
                                           const QVector<DeckLabel> &boardLabels,
                                           const QVector<DeckUser> &boardMembers,
                                           bool supportsStartDate, bool supportsCardColor,
                                           QWidget *parent)
    : QDialog(parent)
    , _api(api)
    , _boardId(boardId)
    , _cardJson(cardJson)
    , _card(DeckCard::fromJson(cardJson))
    , _boardLabels(boardLabels)
    , _boardMembers(boardMembers)
    , _supportsStartDate(supportsStartDate)
    , _supportsCardColor(supportsCardColor)
{
    setWindowTitle(_card.title.isEmpty() ? QStringLiteral("Karte") : _card.title);
    resize(980, 640);
    buildUi();

    if (_api) {
        connect(_api, &DeckOcsApi::attachmentsReceived, this,
                &DeckCardDetailDialog::onAttachmentsReceived);
        connect(_api, &DeckOcsApi::attachmentUploaded, this, [this](int cardId) {
            if (cardId == _card.id) {
                _api->fetchAttachments(_boardId, _card.stackId, _card.id);
            }
        });
        connect(_api, &DeckOcsApi::attachmentDeleted, this, [this](int cardId) {
            if (cardId == _card.id) {
                _api->fetchAttachments(_boardId, _card.stackId, _card.id);
            }
        });
        connect(_api, &DeckOcsApi::attachmentDownloaded, this,
                [](const QString &, const QString &localPath) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
        });
        _api->fetchAttachments(_boardId, _card.stackId, _card.id);
    }
}

void DeckCardDetailDialog::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(10);

    // Title row
    _titleEdit = new QLineEdit(_card.title, this);
    _titleEdit->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 600;"));
    connect(_titleEdit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
    rootLayout->addWidget(_titleEdit);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(16);

    // ---- Left: description + comments (60%) ----
    auto *leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(8);

    leftLayout->addWidget(new QLabel(QStringLiteral("Beschreibung"), this));

    _descriptionView = new QTextBrowser(this);
    _descriptionView->setOpenExternalLinks(true);
    _descriptionView->setMarkdown(_card.description);
    _descriptionView->setStyleSheet(QStringLiteral(
        "QTextBrowser { background: %1; border: 1px solid %2; border-radius: 8px; padding: 8px; }")
        .arg(SouveraTheme::instance()->color(SouveraTheme::Color::Surface).name(),
             SouveraTheme::instance()->color(SouveraTheme::Color::Border).name()));
    _descriptionView->setMinimumHeight(120);
    leftLayout->addWidget(_descriptionView, 1);

    auto *descBtnRow = new QWidget(this);
    auto *descBtnLayout = new QHBoxLayout(descBtnRow);
    descBtnLayout->setContentsMargins(0, 0, 0, 0);
    auto *editDescBtn = new QPushButton(QStringLiteral("Beschreibung bearbeiten"), descBtnRow);
    editDescBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(editDescBtn, &QPushButton::clicked, this, &DeckCardDetailDialog::editDescription);
    descBtnLayout->addWidget(editDescBtn);
    descBtnLayout->addStretch();
    leftLayout->addWidget(descBtnRow);

    _comments = new DeckCommentsWidget(_api, this);
    _comments->setCard(_card.id, _boardMembers);
    leftLayout->addWidget(new QLabel(QStringLiteral("Kommentare"), this));
    leftLayout->addWidget(_comments, 2);

    auto *leftWidget = new QWidget(this);
    leftWidget->setLayout(leftLayout);
    columns->addWidget(leftWidget, 6);

    // ---- Right: metadata (40%) ----
    auto *rightScroll = new QScrollArea(this);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    auto *rightWidget = new QWidget(rightScroll);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    // Farbe
    if (_supportsCardColor) {
        rightLayout->addWidget(new QLabel(QStringLiteral("Farbe"), this));
        auto *colorRow = new QWidget(rightWidget);
        auto *colorLayout = new QHBoxLayout(colorRow);
        colorLayout->setContentsMargins(0, 0, 0, 0);
        colorLayout->setSpacing(4);
        for (const auto *hex : kDeckPalette) {
            auto *btn = new QPushButton(colorRow);
            btn->setFixedSize(22, 22);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #%1; border: 2px solid transparent; border-radius: 11px; }"
                "QPushButton:hover { border-color: #ffffff; }").arg(hex));
            connect(btn, &QPushButton::clicked, this, [this, hex]() {
                applyColor(QString::fromLatin1(hex));
            });
            colorLayout->addWidget(btn);
        }
        colorLayout->addStretch();
        rightLayout->addWidget(colorRow);
    }

    // Termine
    rightLayout->addWidget(new QLabel(QStringLiteral("Fällig am"), this));
    _dueEdit = new QDateEdit(rightWidget);
    _dueEdit->setCalendarPopup(true);
    _dueEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    _dueEdit->setSpecialValueText(QStringLiteral("—"));
    if (_card.dueDate.isValid()) _dueEdit->setDate(_card.dueDate.date());
    connect(_dueEdit, &QDateEdit::dateChanged, this, [this]() { markDirty(); });
    rightLayout->addWidget(_dueEdit);

    if (_supportsStartDate) {
        rightLayout->addWidget(new QLabel(QStringLiteral("Start am"), this));
        _startEdit = new QDateEdit(rightWidget);
        _startEdit->setCalendarPopup(true);
        _startEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
        _startEdit->setSpecialValueText(QStringLiteral("—"));
        if (_card.startDate.isValid()) _startEdit->setDate(_card.startDate.date());
        connect(_startEdit, &QDateEdit::dateChanged, this, [this]() { markDirty(); });
        rightLayout->addWidget(_startEdit);
    }

    // Labels
    rightLayout->addWidget(new QLabel(QStringLiteral("Labels"), this));
    _labelChips = new QWidget(rightWidget);
    _labelChipsLayout = new QVBoxLayout(_labelChips);
    _labelChipsLayout->setContentsMargins(0, 0, 0, 0);
    _labelChipsLayout->setSpacing(4);
    renderLabels();
    rightLayout->addWidget(_labelChips);

    // Anhänge
    rightLayout->addWidget(new QLabel(QStringLiteral("Anhänge"), this));
    _attachmentList = new QListWidget(rightWidget);
    _attachmentList->setMinimumHeight(120);
    _attachmentList->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 8px; }")
        .arg(SouveraTheme::instance()->color(SouveraTheme::Color::Surface).name(),
             SouveraTheme::instance()->color(SouveraTheme::Color::Border).name()));
    connect(_attachmentList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) {
        const auto attId = item->data(Qt::UserRole).toInt();
        const auto name = item->text();
        for (const auto &attachment : _card.attachments) {
            if (attachment.id == attId) {
                openAttachment(attachment);
                return;
            }
        }
        Q_UNUSED(name);
    });
    rightLayout->addWidget(_attachmentList, 1);

    auto *attachBtnRow = new QWidget(rightWidget);
    auto *attachBtnLayout = new QHBoxLayout(attachBtnRow);
    attachBtnLayout->setContentsMargins(0, 0, 0, 0);
    auto *uploadBtn = new QPushButton(QStringLiteral("Datei hinzufügen…"), attachBtnRow);
    uploadBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(uploadBtn, &QPushButton::clicked, this, &DeckCardDetailDialog::uploadAttachment);
    auto *deleteAttachBtn = new QPushButton(QStringLiteral("Löschen"), attachBtnRow);
    deleteAttachBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(deleteAttachBtn, &QPushButton::clicked, this, [this]() {
        const auto item = _attachmentList->currentItem();
        if (item) deleteAttachment(item->data(Qt::UserRole).toInt());
    });
    attachBtnLayout->addWidget(uploadBtn);
    attachBtnLayout->addWidget(deleteAttachBtn);
    attachBtnLayout->addStretch();
    rightLayout->addWidget(attachBtnRow);

    rightLayout->addStretch();
    rightScroll->setWidget(rightWidget);
    columns->addWidget(rightScroll, 4);

    rootLayout->addLayout(columns, 1);

    // Save / cancel
    auto *buttonRow = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("Abbrechen"), buttonRow);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto *saveBtn = new QPushButton(QStringLiteral("Speichern"), buttonRow);
    saveBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(saveBtn, &QPushButton::clicked, this, &DeckCardDetailDialog::saveAll);
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(saveBtn);
    rootLayout->addWidget(buttonRow);
}

void DeckCardDetailDialog::renderLabels()
{
    while (auto *item = _labelChipsLayout->takeAt(0)) {
        if (auto *w = item->widget()) w->deleteLater();
        delete item;
    }
    auto assignedIds = QSet<int>();
    for (const auto &label : _card.labels) assignedIds.insert(label.id);

    for (const auto &label : _boardLabels) {
        auto *btn = new QPushButton(label.title.isEmpty()
            ? QStringLiteral("(ohne Titel)") : label.title, _labelChips);
        btn->setCheckable(true);
        btn->setChecked(assignedIds.contains(label.id));
        const auto chipColor = label.color.isValid() ? label.color : QColor(0x94, 0xa3, 0xb8);
        const auto textColor = chipColor.lightness() > 140 ? QStringLiteral("#1c2430")
                                                           : QStringLiteral("#ffffff");
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: %2; border: none; border-radius: 4px;"
            "  font-size: 11px; padding: 3px 8px; text-align: left; }"
            "QPushButton:checked { border: 2px solid %3; }")
            .arg(chipColor.name(), textColor,
                 SouveraTheme::instance()->color(SouveraTheme::Color::Accent).name()));
        connect(btn, &QPushButton::toggled, this, [this, label](bool checked) {
            toggleLabel(label.id, checked);
        });
        _labelChipsLayout->addWidget(btn);
    }
    _labelChips->setVisible(!_boardLabels.isEmpty());
}

void DeckCardDetailDialog::toggleLabel(int labelId, bool checked)
{
    if (checked) {
        for (const auto &label : _boardLabels) {
            if (label.id == labelId) {
                _card.labels.append(label);
                _cardJson[QStringLiteral("labels")] =
                    _cardJson.value(QStringLiteral("labels")).toArray();
                // Keep raw JSON consistent for the save path.
                QJsonArray rawLabels;
                for (const auto &l : _card.labels) {
                    QJsonObject lo;
                    lo.insert(QStringLiteral("id"), l.id);
                    lo.insert(QStringLiteral("title"), l.title);
                    lo.insert(QStringLiteral("color"), l.color.name().mid(1));
                    rawLabels.append(lo);
                }
                _cardJson[QStringLiteral("labels")] = rawLabels;
                break;
            }
        }
    } else {
        for (int i = _card.labels.size() - 1; i >= 0; --i) {
            if (_card.labels.at(i).id == labelId) _card.labels.removeAt(i);
        }
        QJsonArray rawLabels;
        for (const auto &l : _card.labels) {
            QJsonObject lo;
            lo.insert(QStringLiteral("id"), l.id);
            lo.insert(QStringLiteral("title"), l.title);
            lo.insert(QStringLiteral("color"), l.color.name().mid(1));
            rawLabels.append(lo);
        }
        _cardJson[QStringLiteral("labels")] = rawLabels;
    }
}

void DeckCardDetailDialog::applyColor(const QString &colorName)
{
    _card.color = QColor(QLatin1Char('#') + colorName);
    _cardJson[QStringLiteral("color")] = colorName;
}

void DeckCardDetailDialog::editDescription()
{
    if (_descriptionEditing) return;
    _descriptionEditing = true;

    // Pre-created hidden editor sits right below the read-only view.
    _descriptionEdit = new QPlainTextEdit(_card.description, this);
    _descriptionEdit->setPlaceholderText(QStringLiteral("Beschreibung (Markdown)"));
    _descriptionEdit->setStyleSheet(_descriptionView->styleSheet());
    _descriptionEdit->setMinimumHeight(120);
    auto *leftLayout = qobject_cast<QVBoxLayout *>(_descriptionView->parentWidget()->layout());
    if (leftLayout) {
        leftLayout->insertWidget(leftLayout->indexOf(_descriptionView) + 1, _descriptionEdit);
    }
    _descriptionView->hide();
    _descriptionEdit->setFocus();
}

void DeckCardDetailDialog::saveAll()
{
    _card.title = _titleEdit->text().trimmed();
    if (_card.title.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Karte"),
                             QStringLiteral("Der Titel darf nicht leer sein."));
        return;
    }

    // Description: read from the editor when it was opened.
    const auto description = _descriptionEdit
        ? _descriptionEdit->toPlainText()
        : _card.description;

    _card.description = description;
    _card.dueDate = _dueEdit->date() != _dueEdit->minimumDate()
        ? QDateTime(_dueEdit->date(), QTime(12, 0)) : QDateTime();
    if (_startEdit && _supportsStartDate) {
        _card.startDate = _startEdit->date() != _startEdit->minimumDate()
            ? QDateTime(_startEdit->date(), QTime(8, 0)) : QDateTime();
    }

    // Rebuild the complete card JSON from the original raw payload.
    _card.raw = _cardJson;
    const auto updated = _card.toFullJson(
        DeckManager::instance()->supportsOwnerAsString());

    emit cardSaved(_boardId, _card.stackId, _card.id, updated);
    accept();
}

void DeckCardDetailDialog::onAttachmentsReceived(int cardId,
                                                 const QVector<DeckAttachment> &attachments)
{
    if (cardId != _card.id) return;
    _card.attachments = attachments;
    _attachmentList->clear();
    for (const auto &attachment : attachments) {
        auto *item = new QListWidgetItem(attachment.name, _attachmentList);
        item->setData(Qt::UserRole, attachment.id);
        item->setToolTip(QStringLiteral("%1 · %2 KB").arg(
            attachment.mimeType.isEmpty() ? QStringLiteral("Datei") : attachment.mimeType)
            .arg(attachment.size / 1024.0, 0, 'f', 0));
        if (attachment.mimeType.startsWith(QStringLiteral("image/"))) {
            item->setText(QStringLiteral("\U0001F5BC %1").arg(attachment.name));
        } else {
            item->setText(QStringLiteral("\U0001F4CE %1").arg(attachment.name));
        }
    }
}

void DeckCardDetailDialog::onAttachmentUploaded(int cardId)
{
    Q_UNUSED(cardId);
}

void DeckCardDetailDialog::uploadAttachment()
{
    const auto filePath = QFileDialog::getOpenFileName(this,
        QStringLiteral("Anhang hinzufügen"));
    if (filePath.isEmpty() || !_api) return;
    _api->uploadAttachment(_boardId, _card.stackId, _card.id, filePath);
}

void DeckCardDetailDialog::openAttachment(const DeckAttachment &attachment)
{
    if (!_api) return;
    _api->downloadAttachment(_boardId, _card.stackId, _card.id, attachment.id,
                             attachment.name);
}

void DeckCardDetailDialog::deleteAttachment(int attachmentId)
{
    const auto ret = QMessageBox::question(this,
        QStringLiteral("Anhang löschen"),
        QStringLiteral("Soll dieser Anhang gelöscht werden?"));
    if (ret != QMessageBox::Yes) return;
    _api->deleteAttachment(_boardId, _card.stackId, _card.id, attachmentId);
}

void DeckCardDetailDialog::markDirty()
{
    // Reserved for a dirty indicator.
}

} // namespace OCC
