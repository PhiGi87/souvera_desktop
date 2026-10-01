/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OfficeWindow.h"
#include "theme/SouveraMetrics.h"
#include "OfficeDocumentView.h"
#include "LokOffice.h"

#include "config.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QShortcut>
#include <QLoggingCategory>
#include <QVBoxLayout>

#ifdef BUILD_WITH_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#endif

namespace OCC {

namespace Metrics = Sou::Metrics;
Q_LOGGING_CATEGORY(lcOfficeWindow, "souvera.office.window")

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
    setupFormatToolbar();
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

void OfficeWindow::setupFormatToolbar()
{
    // Second toolbar row: the full word-processing command set. Everything
    // runs through LibreOfficeKit UNO commands, exactly like the desktop
    // Writer menus do.
    auto *formatBar = new QWidget(this);
    formatBar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *formatLayout = new QHBoxLayout(formatBar);
    formatLayout->setContentsMargins(Metrics::CardMargin, Metrics::SpacingXS, Metrics::CardMargin, Metrics::SpacingXS);
    formatLayout->setSpacing(Metrics::SpacingS);

    // Paragraph styles
    _styleCombo = new QComboBox(formatBar);
    _styleCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    _styleCombo->addItem(QStringLiteral("Standard"));
    _styleCombo->addItem(QStringLiteral("Überschrift 1"));
    _styleCombo->addItem(QStringLiteral("Überschrift 2"));
    _styleCombo->addItem(QStringLiteral("Überschrift 3"));
    _styleCombo->addItem(QStringLiteral("Liste Aufzählung"));
    _styleCombo->addItem(QStringLiteral("Liste Nummerierung"));
    _styleCombo->setToolTip(QStringLiteral("Absatzformat"));
    connect(_styleCombo, &QComboBox::activated, this, [this](int index) {
        if (!_view) return;
        static const char *styles[] = {"Standard", "Heading 1", "Heading 2",
                                       "Heading 3", "List Bullet", "List Number"};
        _view->unoCommandArgs(".uno:StyleApply",
                              {{"Style", styles[index]}, {"Family", "ParagraphStyles"}});
    });
    formatLayout->addWidget(_styleCombo);

    // Font family + size
    _fontCombo = new QComboBox(formatBar);
    _fontCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    _fontCombo->setEditable(true);
    for (const auto *f : {"Liberation Serif", "Liberation Sans", "Liberation Mono",
                          "Arial", "Times New Roman", "Calibri", "Verdana", "Georgia"}) {
        _fontCombo->addItem(QString::fromUtf8(f));
    }
    _fontCombo->setToolTip(QStringLiteral("Schriftart"));
    connect(_fontCombo, &QComboBox::activated, this, [this](int index) {
        if (_view) {
            _view->unoCommandArgs(".uno:CharFontName",
                                  {{"CharFontName", _fontCombo->itemText(index)}});
        }
    });
    formatLayout->addWidget(_fontCombo, 1);

    _sizeCombo = new QComboBox(formatBar);
    _sizeCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    _sizeCombo->setEditable(true);
    for (int size : {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 48, 72}) {
        _sizeCombo->addItem(QString::number(size));
    }
    _sizeCombo->setToolTip(QStringLiteral("Schriftgröße"));
    connect(_sizeCombo, &QComboBox::activated, this, [this](int index) {
        if (_view) {
            _view->unoCommandArgs(".uno:FontHeight",
                                  {{"FontHeight", _sizeCombo->itemText(index).toDouble()}});
        }
    });
    formatLayout->addWidget(_sizeCombo);

    const auto addButton = [this, formatLayout, formatBar](const QString &text, const QString &tooltip,
                                                const char *uno) {
        auto *btn = new QPushButton(text, formatBar);
        btn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
        btn->setToolTip(tooltip);
        if (strlen(tooltip.toUtf8().constData()) < 40) {
            btn->setFixedWidth(qMax(32, btn->sizeHint().width()));
        }
        connect(btn, &QPushButton::clicked, this, [this, uno]() {
            if (_view) _view->unoCommand(uno);
        });
        formatLayout->addWidget(btn);
        return btn;
    };

    // Alignments
    addButton(QStringLiteral("⯇"), QStringLiteral("Linksbündig"), ".uno:AlignLeft");
    addButton(QStringLiteral("≡"), QStringLiteral("Zentriert"), ".uno:AlignHorizontalCenter");
    addButton(QStringLiteral("⯈"), QStringLiteral("Rechtsbündig"), ".uno:AlignRight");
    addButton(QStringLiteral("☰"), QStringLiteral("Blocksatz"), ".uno:AlignJustified");

    // Lists
    addButton(QStringLiteral("•Liste"), QStringLiteral("Aufzählung"), ".uno:DefaultBullet");
    addButton(QStringLiteral("1.Liste"), QStringLiteral("Nummerierung"), ".uno:DefaultNumbering");

    // Indent
    addButton(QStringLiteral("→|"), QStringLiteral("Einzug vergrößern"), ".uno:IncrementIndent");
    addButton(QStringLiteral("|←"), QStringLiteral("Einzug verkleinern"), ".uno:DecrementIndent");

    // Insert
    addButton(QStringLiteral("Tabelle"), QStringLiteral("Tabelle einfügen"), ".uno:InsertTable");
    addButton(QStringLiteral("Bild"), QStringLiteral("Bild einfügen"), ".uno:InsertGraphic");

    // Color buttons open the UNO dialogs directly (simpler + complete)
    addButton(QStringLiteral("A█"), QStringLiteral("Schriftfarbe"), ".uno:FontColor");
    addButton(QStringLiteral("A▓"), QStringLiteral("Hervorhebung"), ".uno:BackColor");

    // Find & replace, print, export
    addButton(QStringLiteral("Suchen"), QStringLiteral("Suchen und ersetzen"), ".uno:SearchDialog");
    addButton(QStringLiteral("Drucken"), QStringLiteral("Drucken"), ".uno:Print");
    addButton(QStringLiteral("PDF"), QStringLiteral("Als PDF exportieren"), ".uno:ExportDirectToPDF");

    formatLayout->addStretch();

    auto *layout2 = qobject_cast<QVBoxLayout *>(this->layout());
    layout2->addWidget(formatBar);
}

void OfficeWindow::connectViewState()
{
    if (!_view) return;
    // Keep the style/font/size combos in sync with the cursor position.
    connect(_view, &OfficeDocumentView::unoStateChanged, this,
            [this](const QString &command, const QString &value) {
        if (_syncingState || value.isEmpty()) return;
        _syncingState = true;
        if (command == QLatin1String(".uno:CharFontName")) {
            const auto idx = _fontCombo->findText(value);
            if (idx < 0) _fontCombo->setCurrentText(value);
            else _fontCombo->setCurrentIndex(idx);
        } else if (command == QLatin1String(".uno:FontHeight")) {
            const auto ok = [&] {
                bool convOk = false; const double d = value.toDouble(&convOk); return convOk ? d : 0.0;
            }();
            if (ok > 0) _sizeCombo->setCurrentText(QString::number(qRound(ok)));
        } else if (command == QLatin1String(".uno:StyleApply")
                   || command == QLatin1String(".uno:TemplateFamily")) {
            static const QHash<QString, int> styleIndex = {
                {QStringLiteral("Standard"), 0}, {QStringLiteral("Heading 1"), 1},
                {QStringLiteral("Heading 2"), 2}, {QStringLiteral("Heading 3"), 3},
                {QStringLiteral("List Bullet"), 4}, {QStringLiteral("List Number"), 5}};
            const auto idx = styleIndex.value(value, -1);
            if (idx >= 0) _styleCombo->setCurrentIndex(idx);
        } else if (command == QLatin1String(".uno:Bold")
                   || command == QLatin1String(".uno:Italic")
                   || command == QLatin1String(".uno:Underline")) {
            const auto on = value == QLatin1String("true");
            for (auto *btn : findChildren<QPushButton *>()) {
                const auto tip = btn->toolTip();
                if ((command == QLatin1String(".uno:Bold") && tip == QLatin1String("Fett"))
                    || (command == QLatin1String(".uno:Italic") && tip == QLatin1String("Kursiv"))
                    || (command == QLatin1String(".uno:Underline") && tip == QLatin1String("Unterstrichen"))) {
                    QFont f = btn->font();
                    f.setBold(on);
                    btn->setFont(f);
                }
            }
        }
        _syncingState = false;
    });
}

void OfficeWindow::setupView(const QString &localPath, const QUrl &collaboraUrl)
{
    auto *layout = qobject_cast<QVBoxLayout *>(this->layout());

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    // Guard: only try the embedded engine when the bundled LibreOffice is
    // actually found. LokOffice::instance() can crash (segfault in
    // lok_init_2) when the program directory does not exist.
    const auto loPath = LokOffice::installPath();
    if (LokOffice::isSupported() && !loPath.isEmpty()) {
        auto *scroll = new QScrollArea(this);
        scroll->setObjectName(QStringLiteral("PanelScroll"));
        scroll->setWidgetResizable(false);
        scroll->setAlignment(Qt::AlignCenter);

        _view = new OfficeDocumentView(scroll);
        _loaded = _view->load(localPath);

        if (_loaded) {
            scroll->setWidget(_view);
            layout->addWidget(scroll, 1);

            connectViewState();

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
            return;
        }
        // Load failed: clean up and fall through to the browser fallback.
        qWarning() << "Embedded engine failed to load" << localPath;
        scroll->deleteLater();
        _view = nullptr;
    } else {
        qWarning() << "Bundled LibreOffice not found at" << loPath;
    }
#endif

    // Fallback 1: launch the bundled desktop LibreOffice with the document —
    // a complete Word replacement even when tile embedding is unavailable.
    if (LokOffice::isSupported() && !loPath.isEmpty()) {
        launchExternalLibreOffice(loPath);
        return;
    }

    // Fallback 2: open in the system browser (macOS always, Win/Linux when
    // neither the embedded engine nor a desktop LibreOffice is available).
    Q_UNUSED(localPath)
    if (collaboraUrl.isValid() && !collaboraUrl.isEmpty()) {
#ifdef BUILD_WITH_WEBENGINE
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
#else
        QDesktopServices::openUrl(collaboraUrl);
        _loaded = true;
#endif
    } else {
        auto *hint = new QLabel(QStringLiteral(
            "Dokument konnte nicht geöffnet werden.\n"
            "Kein eingebettetes Office verfügbar und keine Server-URL."), this);
        hint->setObjectName(QStringLiteral("PanelPlaceholder"));
        hint->setAlignment(Qt::AlignCenter);
        _fallbackView = hint;
        layout->addWidget(hint, 1);
    }
}

void OfficeWindow::launchExternalLibreOffice(const QString &loProgram)
{
    // Runs the full desktop Writer/Calc/Impress from the bundled tree —
    // every editing feature, no browser involved. The save/upload cycle is
    // handled by watching the file for changes.
    const auto soffice = loProgram + QStringLiteral("/soffice.exe");
#ifdef Q_OS_WIN
    const auto sofficeBin = QFile::exists(soffice) ? soffice
        : loProgram + QStringLiteral("/soffice.bin");
#else
    Q_UNUSED(soffice)
    const auto sofficeBin = loProgram + QStringLiteral("/soffice");
#endif
    if (!QFile::exists(sofficeBin)) {
        qCWarning(lcOfficeWindow) << "External soffice not found:" << sofficeBin;
        return;
    }

    _externalEditing = true;
    qCInfo(lcOfficeWindow) << "Launching external LibreOffice for" << _localPath;
    QProcess::startDetached(sofficeBin, {QDir::toNativeSeparators(_localPath)});

    auto *hint = new QLabel(QStringLiteral(
        "Das Dokument wird in LibreOffice ge\u00F6ffnet.\n"
        "\u00C4nderungen werden beim Speichern automatisch hochgeladen."), this);
    hint->setObjectName(QStringLiteral("PanelPlaceholder"));
    hint->setAlignment(Qt::AlignCenter);
    _fallbackView = hint;
    auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
    layout->addWidget(hint, 1);
    _loaded = true;

    // Upload on external save: watch the file, debounce, notify the manager.
    auto *watcher = new QFileSystemWatcher(this);
    watcher->addPath(_localPath);
    connect(watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        QTimer::singleShot(1500, this, [this]() {
            emit documentSaved(_localPath);
        });
    });
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
