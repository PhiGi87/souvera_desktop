/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OfficeWindow.h"
#include "OfficeDocumentView.h"
#include "LokOffice.h"

#include "config.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

#ifdef BUILD_WITH_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#endif

namespace OCC {

OfficeWindow::OfficeWindow(const QString &localPath, const QString &displayName,
                           const QUrl &collaboraUrl, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , _localPath(localPath)
{
    setWindowTitle(QStringLiteral("Souvera Office \u2014 %1").arg(displayName));
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_QuitOnClose, false);
    resize(1000, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolbar(displayName);
    setupView(localPath, collaboraUrl);
}

void OfficeWindow::setupToolbar(const QString &displayName)
{
    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 8, 16, 8);
    toolbarLayout->setSpacing(6);

    auto *titleLabel = new QLabel(QStringLiteral("\U0001F4C4 %1").arg(displayName), toolbar);
    titleLabel->setObjectName(QStringLiteral("PanelTitle"));
    toolbarLayout->addWidget(titleLabel);

    toolbarLayout->addSpacing(10);

    _saveBtn = new QPushButton(QStringLiteral("Speichern"), toolbar);
    _saveBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(_saveBtn, &QPushButton::clicked, this, [this]() {
        save();
    });
    toolbarLayout->addWidget(_saveBtn);

    auto *undoBtn = new QPushButton(QStringLiteral("R\u00FCckg\u00E4ngig"), toolbar);
    undoBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(undoBtn, &QPushButton::clicked, this, [this]() {
        if (_view) _view->unoCommand(".uno:Undo");
    });
    toolbarLayout->addWidget(undoBtn);

    auto *redoBtn = new QPushButton(QStringLiteral("Wiederholen"), toolbar);
    redoBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(redoBtn, &QPushButton::clicked, this, [this]() {
        if (_view) _view->unoCommand(".uno:Redo");
    });
    toolbarLayout->addWidget(redoBtn);

    toolbarLayout->addSpacing(6);

    auto *boldBtn = new QPushButton(QStringLiteral("B"), toolbar);
    boldBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    QFont boldFont = boldBtn->font();
    boldFont.setBold(true);
    boldBtn->setFont(boldFont);
    connect(boldBtn, &QPushButton::clicked, this, [this]() {
        if (_view) _view->unoCommand(".uno:Bold");
    });
    toolbarLayout->addWidget(boldBtn);

    auto *italicBtn = new QPushButton(QStringLiteral("I"), toolbar);
    italicBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    QFont italicFont = italicBtn->font();
    italicFont.setItalic(true);
    italicBtn->setFont(italicFont);
    connect(italicBtn, &QPushButton::clicked, this, [this]() {
        if (_view) _view->unoCommand(".uno:Italic");
    });
    toolbarLayout->addWidget(italicBtn);

    auto *underlineBtn = new QPushButton(QStringLiteral("U"), toolbar);
    underlineBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    QFont underlineFont = underlineBtn->font();
    underlineFont.setUnderline(true);
    underlineBtn->setFont(underlineFont);
    connect(underlineBtn, &QPushButton::clicked, this, [this]() {
        if (_view) _view->unoCommand(".uno:Underline");
    });
    toolbarLayout->addWidget(underlineBtn);

    toolbarLayout->addStretch();

    auto *zoomOut = new QPushButton(QStringLiteral("\u2212"), toolbar);
    zoomOut->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(zoomOut, &QPushButton::clicked, this, [this]() {
        if (_view) _view->setZoom(_view->zoom() - 0.1);
    });
    toolbarLayout->addWidget(zoomOut);

    _zoomLabel = new QLabel(QStringLiteral("100 %"), toolbar);
    _zoomLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    toolbarLayout->addWidget(_zoomLabel);

    auto *zoomIn = new QPushButton(QStringLiteral("+"), toolbar);
    zoomIn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(zoomIn, &QPushButton::clicked, this, [this]() {
        if (_view) _view->setZoom(_view->zoom() + 0.1);
    });
    toolbarLayout->addWidget(zoomIn);

    auto *layout2 = qobject_cast<QVBoxLayout *>(this->layout());
    layout2->addWidget(toolbar);
}

void OfficeWindow::setupView(const QString &localPath, const QUrl &collaboraUrl)
{
    auto *layout = qobject_cast<QVBoxLayout *>(this->layout());

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("PanelScroll"));
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignCenter);

    _view = new OfficeDocumentView(scroll);
    _loaded = _view->load(localPath);

    scroll->setWidget(_view);
    layout->addWidget(scroll, 1);

    connect(_view, &OfficeDocumentView::zoomChanged, this, [this](double zoom) {
        _zoomLabel->setText(QStringLiteral("%1 %").arg(qRound(zoom * 100)));
    });
    connect(_view, &OfficeDocumentView::modifiedChanged, this, [this](bool modified) {
        auto title = windowTitle();
        if (modified && !title.endsWith(QStringLiteral(" *"))) {
            setWindowTitle(title + QStringLiteral(" *"));
        } else if (!modified && title.endsWith(QStringLiteral(" *"))) {
            title.chop(2);
            setWindowTitle(title);
        }
    });
#else
    Q_UNUSED(localPath)
    // macOS: embed the Collabora editor from the workspace instead.
#ifdef BUILD_WITH_WEBENGINE
    if (collaboraUrl.isValid() && !collaboraUrl.isEmpty()) {
        auto *view = new QWebEngineView(this);
        auto *page = new QWebEnginePage(QWebEngineProfile::defaultProfile(), view);
        connect(page, &QWebEnginePage::featurePermissionRequested, this,
                [page](const QUrl &origin, QWebEnginePage::Feature feature) {
            page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionGrantedByUser);
        });
        view->setPage(page);
        view->load(collaboraUrl);
        _fallbackView = view;
        layout->addWidget(view, 1);
        _loaded = true;
        return;
    }
#endif
    auto *hint = new QLabel(QStringLiteral(
        "Eingebettetes Office ist auf diesem System nicht verf\u00FCgbar.\n"
        "Das Dokument wird im Browser ge\u00F6ffnet."), this);
    hint->setObjectName(QStringLiteral("PanelPlaceholder"));
    hint->setAlignment(Qt::AlignCenter);
    _fallbackView = hint;
    layout->addWidget(hint, 1);
    if (collaboraUrl.isValid() && !collaboraUrl.isEmpty()) {
        QDesktopServices::openUrl(collaboraUrl);
        _loaded = true;
    }
#endif
}

bool OfficeWindow::isModified() const
{
    return _view ? _view->isModified() : false;
}

bool OfficeWindow::save()
{
    if (!_view) {
        return false;
    }
    if (_view->save()) {
        emit documentSaved(_localPath);
        return true;
    }
    QMessageBox::warning(this, QStringLiteral("Souvera Office"),
                         QStringLiteral("Das Dokument konnte nicht gespeichert werden."));
    return false;
}

void OfficeWindow::forceClose()
{
    _forceClosing = true;
    close();
}

void OfficeWindow::closeEvent(QCloseEvent *event)
{
    if (!_forceClosing && isModified()) {
        const auto ret = QMessageBox::question(this, QStringLiteral("Speichern"),
            QStringLiteral("Das Dokument wurde ge\u00E4ndert. Vor dem Schlie\u00DFen speichern?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (ret == QMessageBox::Yes) {
            save();
        }
    }
    emit windowClosed(this);
    event->accept();
}

} // namespace OCC
