/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include <QWidget>

class QLineEdit;

namespace OCC {

class AccountState;

/**
 * @brief Embedded Talk room window for video calls.
 *
 * With QtWebEngine available the Talk room is rendered inside the app,
 * microphone/camera permissions are granted automatically and HTTP basic
 * auth challenges are answered with the account credentials, so the room
 * opens with the user's session. Without WebEngine the room opens in the
 * system browser instead.
 */
class CallWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CallWindow(AccountState *accountState, const QUrl &roomUrl,
                        const QString &roomName, QWidget *parent = nullptr);

private:
    void setupUi(AccountState *accountState, const QUrl &roomUrl, const QString &roomName);

    QWidget *_view = nullptr;
};

} // namespace OCC

#endif // CALLWINDOW_H
