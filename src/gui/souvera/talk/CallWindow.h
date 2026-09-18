/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include <QElapsedTimer>
#include <QHash>
#include <QVBoxLayout>
#include <QUrl>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class QAudioSink;
class QIODevice;

namespace OCC {

class TalkOcsApi;
class TalkSignalingClient;
#ifdef HAVE_GSTREAMER
class TalkMediaEngine;
#endif

/**
 * @brief Native call window — the single UI surface for the full call
 *        lifecycle (connecting, ringing, in-call, ended).
 *
 * Opens immediately when the user taps "Anrufen" (showing a connecting
 * state), starts the ringback tone once the call object exists on the
 * server, and switches to the connected view only when a remote
 * participant has actually joined the call. Ended calls show feedback
 * briefly and close themselves.
 */
class CallWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CallWindow(TalkOcsApi *api, TalkSignalingClient *signaling,
                        const QString &token, const QString &displayName,
                        const QUrl &roomUrl, const QString &ownActorId = {},
                        QWidget *parent = nullptr);
    ~CallWindow() override;

    /** Sets the room session id (from the REST join response). */
    void setRoomSessionId(const QString &sessionId) { _roomSessionId = sessionId; }

private:
    enum class State { Connecting, Ringing, InCall, Ended };

    void buildUi(const QString &displayName);
    void setState(State state, const QString &error = {});
    void refreshParticipants();
    void updateTileRendering();
    void applyInCallState();
    void startRingTone();
    void stopRingTone();
    void leaveCallAndCleanup();
    void releaseRoomSession();
    void closeEvent(QCloseEvent *event) override;

    TalkOcsApi *_api = nullptr;
    TalkSignalingClient *_signaling = nullptr;
#ifdef HAVE_GSTREAMER
    TalkMediaEngine *_mediaEngine = nullptr;
#endif
    QString _token;
    QString _ownActorId;
    QString _roomSessionId;
    QUrl _roomUrl;
    State _state = State::Connecting;
    bool _callLeft = false;
    QHash<QString, QString> _names;    // actorId -> display name (REST)
    QHash<QString, int> _inCallFlags;  // actorId -> inCall bitmask (REST/signaling)
    QList<QPair<QString, bool>> _tiles; // ordered (name, inCall) snapshot

    QLabel *_titleLabel = nullptr;
    QLabel *_videoLabel = nullptr;
    QLabel *_durationLabel = nullptr;
    QLabel *_stateLabel = nullptr;
    QLabel *_stateDot = nullptr;
    QWidget *_tilesArea = nullptr;
    QVBoxLayout *_tilesLayout = nullptr;
    QPushButton *_micButton = nullptr;
    QPushButton *_hangupButton = nullptr;
    QPushButton *_mediaButton = nullptr;
    QTimer *_durationTimer = nullptr;
    QTimer *_pollTimer = nullptr;
    QElapsedTimer _elapsed;
    QAudioSink *_ringSink = nullptr;
    QIODevice *_ringIo = nullptr;
    QTimer *_ringTimer = nullptr;
    qint64 _ringPhase = 0;
};

} // namespace OCC

#endif // CALLWINDOW_H
