/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OFFICEDOCUMENTVIEW_H
#define OFFICEDOCUMENTVIEW_H

#include <QWidget>
#include <QImage>
#include <QHash>
#include <QJsonObject>

struct _LibreOfficeKitDocument;
typedef struct _LibreOfficeKitDocument LibreOfficeKitDocument;

namespace OCC {

/**
 * @brief Renders and edits a document via LibreOfficeKit tiles.
 *
 * The widget is placed inside a QScrollArea and sized to the document
 * extent. Mouse and keyboard input is forwarded to the engine, tile
 * invalidation callbacks repaint the affected areas.
 */
class OfficeDocumentView : public QWidget
{
    Q_OBJECT
public:
    explicit OfficeDocumentView(QWidget *parent = nullptr);
    ~OfficeDocumentView() override;

    bool load(const QString &filePath);
    bool save();

    void setZoom(double zoom);
    [[nodiscard]] double zoom() const { return _zoom; }

    void unoCommand(const QByteArray &command);
    /** UNO command with JSON arguments, e.g. StyleApply / FontHeight. */
    void unoCommandArgs(const QByteArray &command, const QJsonObject &args);

    [[nodiscard]] bool isModified() const { return _modified; }
    [[nodiscard]] QString filePath() const { return _filePath; }

signals:
    void zoomChanged(double zoom);
    void modifiedChanged(bool modified);

    /** Toolbar state sync: command like ".uno:Bold", value "true"/"false"
     *  for toggles or the raw value (font name, size). */
    void unoStateChanged(const QString &command, const QString &value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void refreshDocumentSize();
    void ensureTiles(const QRect &visiblePx);
    void invalidateTwipsRect(const QRectF &twipsRect);
    void scheduleResize();
    void markModified();

    static void lokCallback(int type, const char *payload, void *data);

    LibreOfficeKitDocument *_doc = nullptr;
    QString _filePath;
    double _zoom = 1.0;
    bool _modified = false;
    long _docWidthTwips = 0;
    long _docHeightTwips = 0;
    QHash<QString, QImage> _tileCache;
};

} // namespace OCC

#endif // OFFICEDOCUMENTVIEW_H
