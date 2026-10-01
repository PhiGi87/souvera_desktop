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
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include <QColorDialog>
#include <QGridLayout>
#include <QVector>
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
    setupView(localPath, collaboraUrl);
}

namespace {
// Writer ships its Colibre icons named after the UNO command; the app
// bundles the needed subset so the toolbar looks native without depending
// on the LibreOffice installation at runtime.
QIcon iconFromUno(const QString &unoSuffix)
{
    const auto path = QStringLiteral(":/souvera/office/icons/lc_%1.png").arg(unoSuffix);
    if (QFile::exists(path)) return QIcon(path);
    return QIcon();
}

// Standard Writer-like color grid (name -> RGB hex without #).
const QVector<QPair<QString, QString>> &writerColors()
{
    static const QVector<QPair<QString, QString>> colors = {
        {"Schwarz", "000000"}, {"Dunkelgrau 4", "111111"}, {"Dunkelgrau 3", "1C1C1C"},
        {"Dunkelgrau 2", "333333"}, {"Dunkelgrau 1", "666666"}, {"Grau", "808080"},
        {"Hellgrau 1", "999999"}, {"Hellgrau 2", "B2B2B2"}, {"Hellgrau 3", "CCCCCC"},
        {"Wei\u00DF", "FFFFFF"},
        {"Gelb", "FFFF00"}, {"Gold", "FFBF00"}, {"Orange", "FF8000"}, {"Ziegel", "FF4000"},
        {"Rot", "FF0000"}, {"Magenta", "BF0041"}, {"Purpur", "800080"}, {"Indigo", "55308D"},
        {"Blau", "2A6099"}, {"Petrol", "158466"}, {"Gr\u00FCn", "00A933"},
        {"Hellgelb", "FFFFBF"}, {"Hellorange", "FFF0BF"}, {"Hellrot", "FFC0C0"},
        {"Hellmagenta", "E8B2CE"}, {"Hellpurpur", "D1BBFF"}, {"Hellblau", "B8CEE4"},
        {"Hellpetrol", "B5E0D0"}, {"Hellgr\u00FCn", "B5E6B8"}, {"Grau 2", "E6E6E6"},
    };
    return colors;
}
} // namespace

QAction *OfficeWindow::makeColorButton(const QString &iconUno, const QString &tooltip,
                                       const char *uno, QToolBar *toolbar)
{
    // MenuButtonPopup: the button applies the last color, the arrow opens
    // the Writer-style color grid.
    auto *button = new QToolButton(toolbar);
    button->setIcon(iconFromUno(iconUno));
    button->setToolTip(tooltip);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);

    auto apply = [this, uno](const QString &hex) {
        if (!_view) return;
        bool ok = false;
        const auto rgb = hex.toUInt(&ok, 16);
        if (!ok) return;
        // LOK typed JSON argument form for UNO long properties.
        _view->unoCommandArgs(uno,
            {{QByteArray(uno + 5), QJsonObject{{"type", "long"}, {"value", double(rgb)}}}});
    };

    auto *menu = new QMenu(button);
    auto *grid = new QWidget(menu);
    auto *gridLayout = new QGridLayout(grid);
    gridLayout->setContentsMargins(6, 6, 6, 6);
    gridLayout->setSpacing(2);
    const auto &colors = writerColors();
    const int columns = 10;
    for (int i = 0; i < colors.size(); ++i) {
        auto *swatch = new QToolButton(grid);
        swatch->setFixedSize(22, 22);
        swatch->setToolTip(colors[i].first);
        swatch->setStyleSheet(QStringLiteral(
            "background-color: #%1; border: 1px solid rgba(128,128,128,120);")
            .arg(colors[i].second));
        connect(swatch, &QToolButton::clicked, menu, [menu, apply, color = colors[i].second]() {
            apply(color);
            menu->close();
        });
        gridLayout->addWidget(swatch, i / columns, i % columns);
    }
    auto *dialogAction = menu->addAction(QStringLiteral("Eigene Farbe\u2026"));
    connect(dialogAction, &QAction::triggered, button, [this, apply]() {
        const auto color = QColorDialog::getColor(Qt::black, this, QStringLiteral("Farbe w\u00E4hlen"));
        if (color.isValid()) apply(QString::number(color.rgb() & 0xFFFFFF, 16).rightJustified(6, QLatin1Char('0')));
    });
    auto *container = new QWidget(menu);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(2, 2, 2, 2);
    containerLayout->addWidget(grid);
    auto *wrap = new QWidgetAction(menu);
    wrap->setDefaultWidget(container);
    menu->addAction(wrap);
    menu->addAction(dialogAction);

    button->setMenu(menu);
    connect(button, &QToolButton::clicked, button, [apply]() {
        apply(QStringLiteral("FF0000")); // default Writer font color red
    });
    return toolbar->addWidget(button);
}

void OfficeWindow::setupToolbar(const QString &displayName)
{
    // A single Writer-style toolbar with the original LibreOffice Colibre
    // icons, grouped exactly like the desktop Writer default toolbar.
    auto *toolbar = new QToolBar(this);
    toolbar->setObjectName(QStringLiteral("OfficeToolBar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    auto *titleLabel = new QLabel(QStringLiteral("\U0001F4C4 %1").arg(displayName), this);
    titleLabel->setObjectName(QStringLiteral("PanelTitle"));
    titleLabel->setContentsMargins(Metrics::CardMargin, 0, Metrics::SpacingM, 0);
    toolbar->addWidget(titleLabel);

    auto addCmd = [this, toolbar](const char *iconUno, const QString &tooltip,
                                  const char *uno) -> QAction * {
        auto *action = new QAction(
            iconFromUno(QLatin1String(iconUno)), tooltip, toolbar);
        connect(action, &QAction::triggered, this, [this, uno]() {
            if (_view) _view->unoCommand(uno);
        });
        toolbar->addAction(action);
        return action;
    };

    // Document group
    _saveBtn = new QPushButton(QStringLiteral("Speichern"), toolbar);
    _saveBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(_saveBtn, &QPushButton::clicked, this, [this]() { save(); });
    toolbar->addWidget(_saveBtn);
    toolbar->addSeparator();

    addCmd("undo", QStringLiteral("R\u00FCckg\u00E4ngig"), ".uno:Undo");
    addCmd("redo", QStringLiteral("Wiederholen"), ".uno:Redo");
    toolbar->addSeparator();

    // Clipboard group
    addCmd("cut", QStringLiteral("Ausschneiden"), ".uno:Cut");
    addCmd("copy", QStringLiteral("Kopieren"), ".uno:Copy");
    addCmd("paste", QStringLiteral("Einf\u00FCgen"), ".uno:Paste");
    toolbar->addSeparator();

    // Styles (Writer dropdown with the common paragraph styles)
    _styleCombo = new QComboBox(toolbar);
    _styleCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    _styleCombo->setMinimumWidth(130);
    _styleCombo->addItem(QStringLiteral("Standard"));
    _styleCombo->addItem(QStringLiteral("Titel"));
    _styleCombo->addItem(QStringLiteral("\u00DCberschrift 1"));
    _styleCombo->addItem(QStringLiteral("\u00DCberschrift 2"));
    _styleCombo->addItem(QStringLiteral("\u00DCberschrift 3"));
    _styleCombo->addItem(QStringLiteral("Aufz\u00E4hlungszeichen"));
    _styleCombo->addItem(QStringLiteral("Nummerierung"));
    _styleCombo->setToolTip(QStringLiteral("Absatzformat"));
    connect(_styleCombo, &QComboBox::activated, this, [this](int index) {
        if (!_view) return;
        static const char *styles[] = {"Standard", "Title", "Heading 1",
                                       "Heading 2", "Heading 3",
                                       "List Bullet", "List Number"};
        _view->unoCommandArgs(".uno:StyleApply",
                              {{"Style", styles[index]}, {"Family", "ParagraphStyles"}});
    });
    toolbar->addWidget(_styleCombo);

    // Font family + size
    _fontCombo = new QComboBox(toolbar);
    _fontCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    _fontCombo->setEditable(true);
    for (const auto *f : {"Liberation Serif", "Liberation Sans", "Liberation Mono",
                          "Arial", "Times New Roman", "Calibri", "Verdana", "Georgia",
                          "Open Sans"}) {
        _fontCombo->addItem(QString::fromUtf8(f));
    }
    _fontCombo->setToolTip(QStringLiteral("Schriftart"));
    _fontCombo->setMinimumWidth(150);
    connect(_fontCombo, &QComboBox::activated, this, [this](int index) {
        if (_view) {
            _view->unoCommandArgs(".uno:CharFontName",
                                  {{"CharFontName", _fontCombo->itemText(index)}});
        }
    });
    toolbar->addWidget(_fontCombo);

    _sizeCombo = new QComboBox(toolbar);
    _sizeCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    _sizeCombo->setEditable(true);
    _sizeCombo->setMinimumWidth(60);
    for (int size : {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 48, 72}) {
        _sizeCombo->addItem(QString::number(size));
    }
    _sizeCombo->setToolTip(QStringLiteral("Schriftgr\u00F6\u00DFe"));
    connect(_sizeCombo, &QComboBox::activated, this, [this](int index) {
        if (_view) {
            _view->unoCommandArgs(".uno:FontHeight",
                                  {{"FontHeight", _sizeCombo->itemText(index).toDouble()}});
        }
    });
    toolbar->addWidget(_sizeCombo);
    toolbar->addSeparator();

    // Character emphasis — the B/I/U buttons keep the Writer toggle look
    // via checkable actions synced from the document state.
    _boldAction = addCmd("bold", QStringLiteral("Fett"), ".uno:Bold");
    _boldAction->setCheckable(true);
    _italicAction = addCmd("italic", QStringLiteral("Kursiv"), ".uno:Italic");
    _italicAction->setCheckable(true);
    _underlineAction = addCmd("underline", QStringLiteral("Unterstrichen"), ".uno:Underline");
    _underlineAction->setCheckable(true);

    // Font color + highlight with Writer-style color grid dropdowns.
    toolbar->addAction(makeColorButton(QStringLiteral("fontcolor"),
        QStringLiteral("Schriftfarbe"), ".uno:FontColor", toolbar));
    toolbar->addAction(makeColorButton(QStringLiteral("backcolor"),
        QStringLiteral("Hervorhebung"), ".uno:BackColor", toolbar));
    toolbar->addSeparator();

    // Alignments
    addCmd("alignleft", QStringLiteral("Linksb\u00FCndig"), ".uno:AlignLeft");
    addCmd("aligncenter", QStringLiteral("Zentriert"), ".uno:AlignHorizontalCenter");
    addCmd("alignright", QStringLiteral("Rechtsb\u00FCndig"), ".uno:AlignRight");
    addCmd("alignblock", QStringLiteral("Blocksatz"), ".uno:AlignJustified");
    toolbar->addSeparator();

    // Lists + indent
    addCmd("defaultbullet", QStringLiteral("Aufz\u00E4hlung"), ".uno:DefaultBullet");
    addCmd("defaultnumbering", QStringLiteral("Nummerierung"), ".uno:DefaultNumbering");
    addCmd("incrementindent", QStringLiteral("Einzug vergr\u00F6\u00DFern"), ".uno:IncrementIndent");
    addCmd("decrementindent", QStringLiteral("Einzug verkleinern"), ".uno:DecrementIndent");
    toolbar->addSeparator();

    // Insert group
    addCmd("inserttable", QStringLiteral("Tabelle einf\u00FCgen"), ".uno:InsertTable");
    addCmd("insertgraphic", QStringLiteral("Bild einf\u00FCgen"), ".uno:InsertGraphic");
    toolbar->addSeparator();

    addCmd("searchdialog", QStringLiteral("Suchen und ersetzen"), ".uno:SearchDialog");
    addCmd("print", QStringLiteral("Drucken"), ".uno:Print");
    addCmd("exportdirecttopdf", QStringLiteral("Als PDF exportieren"), ".uno:ExportDirectToPDF");

    toolbar->addSeparator();
    _zoomLabel = new QLabel(QStringLiteral("100 %"), toolbar);
    _zoomLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    toolbar->addWidget(_zoomLabel);
    auto *zoomOut = new QToolButton(toolbar);
    zoomOut->setText(QStringLiteral("\u2212"));
    connect(zoomOut, &QToolButton::clicked, this, [this]() {
        if (_view) _view->setZoom(_view->zoom() - 0.1);
    });
    toolbar->addWidget(zoomOut);
    auto *zoomIn = new QToolButton(toolbar);
    zoomIn->setText(QStringLiteral("+"));
    connect(zoomIn, &QToolButton::clicked, this, [this]() {
        if (_view) _view->setZoom(_view->zoom() + 0.1);
    });
    toolbar->addWidget(zoomIn);

    auto *layout2 = qobject_cast<QVBoxLayout *>(this->layout());
    layout2->addWidget(toolbar);
}

void OfficeWindow::connectViewState()
{
    if (!_view) return;
    // Keep the style/font/size combos and the B/I/U toggles in sync with
    // the cursor position, driven by LOK state callbacks.
    connect(_view, &OfficeDocumentView::unoStateChanged, this,
            [this](const QString &command, const QString &value) {
        if (_syncingState) return;
        _syncingState = true;
        if (command == QLatin1String(".uno:CharFontName")) {
            const auto idx = _fontCombo->findText(value);
            if (idx < 0) _fontCombo->setCurrentText(value);
            else _fontCombo->setCurrentIndex(idx);
        } else if (command == QLatin1String(".uno:FontHeight")) {
            bool ok = false;
            const auto size = value.toDouble(&ok);
            if (ok && size > 0) {
                _sizeCombo->setCurrentText(QString::number(qRound(size)));
            }
        } else if (command == QLatin1String(".uno:StyleApply")) {
            static const QHash<QString, int> styleIndex = {
                {QStringLiteral("Standard"), 0}, {QStringLiteral("Title"), 1},
                {QStringLiteral("Heading 1"), 2}, {QStringLiteral("Heading 2"), 3},
                {QStringLiteral("Heading 3"), 4}, {QStringLiteral("List Bullet"), 5},
                {QStringLiteral("List Number"), 6}};
            const auto idx = styleIndex.value(value, -1);
            if (idx >= 0) _styleCombo->setCurrentIndex(idx);
        } else if (command == QLatin1String(".uno:Bold")) {
            _boldAction->setChecked(value == QLatin1String("true"));
        } else if (command == QLatin1String(".uno:Italic")) {
            _italicAction->setChecked(value == QLatin1String("true"));
        } else if (command == QLatin1String(".uno:Underline")) {
            _underlineAction->setChecked(value == QLatin1String("true"));
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

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    // Fallback 1: launch the bundled desktop LibreOffice with the document —
    // a complete Word replacement even when tile embedding is unavailable.
    if (LokOffice::isSupported() && !loPath.isEmpty()) {
        launchExternalLibreOffice(loPath);
        return;
    }
#endif

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
