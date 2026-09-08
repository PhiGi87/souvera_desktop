/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OfficeDocumentView.h"
#include "LokOffice.h"

#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QUrl>
#include <QWheelEvent>

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)

#define LOK_USE_UNSTABLE_API 1
#include "lok/LibreOfficeKitEnums.h"
#include "lok/LibreOfficeKitInit.h"

Q_LOGGING_CATEGORY(lcOfficeView, "souvera.office.view")

namespace {

// 96 dpi => 15 twips per pixel at zoom 1.0
constexpr double TwipsPerPxAt100 = 15.0;
constexpr int TilePx = 256;

double twipsToPx(double twips, double zoom)
{
    return twips * zoom / TwipsPerPxAt100;
}

double pxToTwips(double px, double zoom)
{
    return px * TwipsPerPxAt100 / zoom;
}

int qtMouseButtonToLok(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton: return 1;
    case Qt::RightButton: return 2;
    case Qt::MiddleButton: return 4;
    default: return 0;
    }
}

} // namespace

namespace OCC {

OfficeDocumentView::OfficeDocumentView(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 200);
}

OfficeDocumentView::~OfficeDocumentView()
{
    if (_doc) {
        _doc->pClass->destroy(_doc);
        _doc = nullptr;
    }
}

bool OfficeDocumentView::load(const QString &filePath)
{
    auto *office = LokOffice::instance();
    if (!office) {
        qCWarning(lcOfficeView) << "LibreOfficeKit not available";
        return false;
    }

    if (_doc) {
        _doc->pClass->destroy(_doc);
        _doc = nullptr;
        _tileCache.clear();
        _modified = false;
    }

    _filePath = filePath;
    const auto url = QUrl::fromLocalFile(filePath).toString();

    _doc = office->pClass->documentLoad(office, url.toUtf8().constData());
    if (!_doc) {
        qCWarning(lcOfficeView) << "documentLoad failed for" << url;
        return false;
    }

    _doc->pClass->initializeForRendering(_doc, nullptr);
    _doc->pClass->registerCallback(_doc, &OfficeDocumentView::lokCallback, this);
    refreshDocumentSize();
    scheduleResize();
    update();
    return true;
}

bool OfficeDocumentView::save()
{
    if (!_doc || _filePath.isEmpty()) return false;

    const auto url = QUrl::fromLocalFile(_filePath).toString();
    const auto ok = _doc->pClass->saveAs(_doc, url.toUtf8().constData(), nullptr, nullptr);
    if (ok) {
        _modified = false;
        emit modifiedChanged(false);
    }
    return ok != 0;
}

void OfficeDocumentView::setZoom(double zoom)
{
    zoom = qBound(0.25, zoom, 4.0);
    if (qFuzzyCompare(zoom, _zoom)) return;
    _zoom = zoom;
    _tileCache.clear();
    scheduleResize();
    emit zoomChanged(_zoom);
    update();
}

void OfficeDocumentView::unoCommand(const QByteArray &command)
{
    if (!_doc) return;
    _doc->pClass->postUnoCommand(_doc, command.constData(), nullptr, false);
    markModified();
}

void OfficeDocumentView::refreshDocumentSize()
{
    if (!_doc) return;
    long width = 0;
    long height = 0;
    _doc->pClass->getDocumentSize(_doc, &width, &height);
    _docWidthTwips = width;
    _docHeightTwips = height;
}

void OfficeDocumentView::scheduleResize()
{
    const auto pxWidth = qMax<int>(200, qCeil(twipsToPx(_docWidthTwips, _zoom)));
    const auto pxHeight = qMax<int>(200, qCeil(twipsToPx(_docHeightTwips, _zoom)));
    setFixedSize(pxWidth, pxHeight);
}

void OfficeDocumentView::ensureTiles(const QRect &visiblePx)
{
    if (!_doc) return;

    const auto firstCol = qMax(0, visiblePx.left() / TilePx);
    const auto lastCol = visiblePx.right() / TilePx;
    const auto firstRow = qMax(0, visiblePx.top() / TilePx);
    const auto lastRow = visiblePx.bottom() / TilePx;

    for (auto row = firstRow; row <= lastRow; ++row) {
        for (auto col = firstCol; col <= lastCol; ++col) {
            const auto key = QStringLiteral("%1:%2").arg(col).arg(row);
            if (_tileCache.contains(key)) continue;

            QImage tile(TilePx, TilePx, QImage::Format_ARGB32_Premultiplied);
            tile.fill(Qt::white);
            const auto tileX = static_cast<int>(pxToTwips(col * TilePx, _zoom));
            const auto tileY = static_cast<int>(pxToTwips(row * TilePx, _zoom));
            const auto tileW = static_cast<int>(pxToTwips(TilePx, _zoom));
            const auto tileH = static_cast<int>(pxToTwips(TilePx, _zoom));
            _doc->pClass->paintTile(_doc, tile.bits(), TilePx, TilePx,
                                    tileX, tileY, tileW, tileH);
            _tileCache.insert(key, tile);
        }
    }
}

void OfficeDocumentView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), Qt::white);

    if (!_doc) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Dokument konnte nicht geladen werden."));
        return;
    }

    ensureTiles(event->rect().intersected(rect()));

    const auto firstCol = qMax(0, event->rect().left() / TilePx);
    const auto lastCol = event->rect().right() / TilePx;
    const auto firstRow = qMax(0, event->rect().top() / TilePx);
    const auto lastRow = event->rect().bottom() / TilePx;

    for (auto row = firstRow; row <= lastRow; ++row) {
        for (auto col = firstCol; col <= lastCol; ++col) {
            const auto key = QStringLiteral("%1:%2").arg(col).arg(row);
            const auto it = _tileCache.constFind(key);
            if (it == _tileCache.constEnd()) continue;
            painter.drawImage(col * TilePx, row * TilePx, it.value());
        }
    }
}

void OfficeDocumentView::mousePressEvent(QMouseEvent *event)
{
    if (_doc) {
        _doc->pClass->postMouseEvent(_doc, LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                     event->position().toPoint().x(), event->position().toPoint().y(), 1,
                                     qtMouseButtonToLok(event->button()), 0);
    }
    setFocus();
}

void OfficeDocumentView::mouseReleaseEvent(QMouseEvent *event)
{
    if (_doc) {
        _doc->pClass->postMouseEvent(_doc, LOK_MOUSEEVENT_MOUSEBUTTONUP,
                                     event->position().toPoint().x(), event->position().toPoint().y(), 1,
                                     qtMouseButtonToLok(event->button()), 0);
        markModified();
    }
}

void OfficeDocumentView::mouseMoveEvent(QMouseEvent *event)
{
    if (_doc && (event->buttons() != Qt::NoButton)) {
        _doc->pClass->postMouseEvent(_doc, LOK_MOUSEEVENT_MOUSEMOVE,
                                     event->position().toPoint().x(), event->position().toPoint().y(), 1,
                                     qtMouseButtonToLok(event->button()), 0);
    }
}

void OfficeDocumentView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const auto delta = event->angleDelta().y() > 0 ? 0.1 : -0.1;
        setZoom(_zoom + delta);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void OfficeDocumentView::keyPressEvent(QKeyEvent *event)
{
    if (!_doc) {
        QWidget::keyPressEvent(event);
        return;
    }

    const auto text = event->text();
    if (!text.isEmpty()) {
        const auto ch = text.at(0).unicode();
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYINPUT, ch, 0);
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYUP, ch, 0);
        markModified();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Backspace:
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYINPUT, 8, 0);
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYUP, 8, 0);
        markModified();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYINPUT, 13, 0);
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYUP, 13, 0);
        markModified();
        break;
    case Qt::Key_Escape:
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYINPUT, 27, 0);
        _doc->pClass->postKeyEvent(_doc, LOK_KEYEVENT_KEYUP, 27, 0);
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void OfficeDocumentView::markModified()
{
    if (_modified) return;
    _modified = true;
    emit modifiedChanged(true);
}

void OfficeDocumentView::invalidateTwipsRect(const QRectF &twipsRect)
{
    // An empty rect invalidates everything.
    if (twipsRect.isEmpty()) {
        _tileCache.clear();
        update();
        return;
    }

    const auto pxRect = QRectF(twipsToPx(twipsRect.x(), _zoom),
                               twipsToPx(twipsRect.y(), _zoom),
                               twipsToPx(twipsRect.width(), _zoom),
                               twipsToPx(twipsRect.height(), _zoom))
                             .toRect();
    const auto firstCol = qMax(0, pxRect.left() / TilePx);
    const auto lastCol = pxRect.right() / TilePx;
    const auto firstRow = qMax(0, pxRect.top() / TilePx);
    const auto lastRow = pxRect.bottom() / TilePx;

    auto dirty = QRegion();
    for (auto row = firstRow; row <= lastRow; ++row) {
        for (auto col = firstCol; col <= lastCol; ++col) {
            _tileCache.remove(QStringLiteral("%1:%2").arg(col).arg(row));
            dirty += QRect(col * TilePx, row * TilePx, TilePx, TilePx);
        }
    }
    update(dirty);
}

void OfficeDocumentView::lokCallback(int type, const char *payload, void *data)
{
    auto *self = static_cast<OfficeDocumentView *>(data);
    if (!self) return;

    const auto payloadStr = QString::fromUtf8(payload ? payload : "");

    switch (type) {
    case LOK_CALLBACK_INVALIDATE_TILES: {
        const auto parts = payloadStr.split(QStringLiteral(", "));
        if (parts.size() == 4) {
            bool ok = true;
            const auto x = parts[0].toDouble(&ok);
            const auto y = ok ? parts[1].toDouble(&ok) : 0;
            const auto w = ok ? parts[2].toDouble(&ok) : 0;
            const auto h = ok ? parts[3].toDouble(&ok) : 0;
            if (ok) {
                self->invalidateTwipsRect(QRectF(x, y, w, h));
                return;
            }
        }
        self->invalidateTwipsRect(QRectF());
        break;
    }
    case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED:
        self->refreshDocumentSize();
        self->_tileCache.clear();
        self->scheduleResize();
        self->update();
        break;
    default:
        // Other callbacks (cursor, selection, ...) do not affect tiles in v1.
        break;
    }
}

} // namespace OCC

#else

// Fallback implementation for platforms without LibreOfficeKit (macOS).

namespace OCC {

OfficeDocumentView::OfficeDocumentView(QWidget *parent)
    : QWidget(parent)
{
}

OfficeDocumentView::~OfficeDocumentView() = default;

bool OfficeDocumentView::load(const QString &) { return false; }
bool OfficeDocumentView::save() { return false; }
void OfficeDocumentView::setZoom(double) {}
void OfficeDocumentView::unoCommand(const QByteArray &) {}
void OfficeDocumentView::refreshDocumentSize() {}
void OfficeDocumentView::ensureTiles(const QRect &) {}
void OfficeDocumentView::invalidateTwipsRect(const QRectF &) { Q_UNUSED(_zoom) }
void OfficeDocumentView::scheduleResize() {}
void OfficeDocumentView::markModified() {}
void OfficeDocumentView::lokCallback(int, const char *, void *) {}
void OfficeDocumentView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    painter.setPen(Qt::gray);
    painter.drawText(rect(), Qt::AlignCenter,
                     QStringLiteral("Eingebettetes Office ist auf diesem System nicht verf\u00FCgbar."));
}
void OfficeDocumentView::mousePressEvent(QMouseEvent *) {}
void OfficeDocumentView::mouseReleaseEvent(QMouseEvent *) {}
void OfficeDocumentView::mouseMoveEvent(QMouseEvent *) {}
void OfficeDocumentView::wheelEvent(QWheelEvent *) {}
void OfficeDocumentView::keyPressEvent(QKeyEvent *) {}

} // namespace OCC

#endif
