/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include <QElapsedTimer>
#include <QHash>
#include <QUrl>
#include <QWidget>

class QLabel;
class QListWidget;
class QTimer;

namespace OCC {

class TalkOcsApi;
class TalkSignalingClient;

/**
 * @brief Native call window for a Talk room.
 *
 * The window represents the desktop client as a call participant (registered
 * over the Talk REST API and the high-performance-backend signaling), shows
 * the live participant list with in-call badges and the elapsed time, and
 * deregisters on leave/close. Audio/video transport is provided by the
 * separate media engine layer.
 */
class CallWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CallWindow(TalkOcsApi *api, TalkSignalingClient *signaling,
                        const QString &token, const QString &displayName,
                        const QUrl &roomUrl, QWidget *parent = nullptr);
    ~CallWindow() override;

private:
    void closeEvent(QCloseEvent *event) override;

    TalkOcsApi *_api = nullptr;
    TalkSignalingClient *_signaling = nullptr;
    QString _token;
    QUrl _roomUrl;
    bool _inCall = true;
    QHash<QString, QString> _names; // actorId -> display name (REST)
    QHash<QString, int> _inCallFlags; // actorId -> inCall (signaling)
    QLabel *_durationLabel = nullptr;
    QLabel *_stateLabel = nullptr;
    QListWidget *_participants = nullptr;
    QTimer *_durationTimer = nullptr;
    QTimer *_pollTimer = nullptr;
    QElapsedTimer _elapsed;
};

} // namespace OCC

#endif // CALLWINDOW_H
