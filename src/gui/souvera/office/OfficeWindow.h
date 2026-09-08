/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OFFICEWINDOW_H
#define OFFICEWINDOW_H

#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;

namespace OCC {

class OfficeDocumentView;

/**
 * @brief Document editor window ("Souvera Office").
 *
 * On Windows and Linux the document is rendered and edited by the bundled
 * LibreOffice engine via LibreOfficeKit (fully offline). On macOS the
 * Collabora editor served by the workspace is embedded instead, because
 * LibreOffice does not provide the embedding API there.
 */
class OfficeWindow : public QWidget
{
    Q_OBJECT
public:
    explicit OfficeWindow(const QString &localPath, const QString &displayName,
                          const QUrl &collaboraUrl, QWidget *parent = nullptr);

    [[nodiscard]] bool documentLoaded() const { return _loaded; }
    [[nodiscard]] bool isModified() const;
    bool save();

    void forceClose();

signals:
    void documentSaved(const QString &localPath);
    void windowClosed(OfficeWindow *window);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupToolbar(const QString &displayName);
    void setupView(const QString &localPath, const QUrl &collaboraUrl);

    QString _localPath;
    bool _loaded = false;
    bool _forceClosing = false;
    OfficeDocumentView *_view = nullptr;
    QWidget *_fallbackView = nullptr;
    QLabel *_zoomLabel = nullptr;
    QPushButton *_saveBtn = nullptr;
};

} // namespace OCC

#endif // OFFICEWINDOW_H
