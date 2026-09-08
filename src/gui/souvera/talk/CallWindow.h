/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include <QWidget>

class QLineEdit;

namespace OCC {

/**
 * @brief Embedded Talk room window for video calls.
 *
 * With QtWebEngine available the Talk room is rendered inside the app and
 * microphone/camera permissions are granted automatically. Without WebEngine
 * the room opens in the system browser instead.
 */
class CallWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CallWindow(const QUrl &roomUrl, const QString &roomName,
                        QWidget *parent = nullptr);

private:
    void setupUi(const QUrl &roomUrl, const QString &roomName);

    QWidget *_view = nullptr;
};

} // namespace OCC

#endif // CALLWINDOW_H
