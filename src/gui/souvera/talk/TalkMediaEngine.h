/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKMEDIAENGINE_H
#define TALKMEDIAENGINE_H

#include "config.h"

#ifdef HAVE_GSTREAMER

#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QString>

#include <gst/gst.h>
#include <gst/app/app.h>

namespace OCC {

class TalkSignalingClient;

/**
 * @brief Native WebRTC media engine for Talk calls using GStreamer
 *        webrtcbin and the high-performance-backend MCU flow.
 *
 * Lifecycle:
 *  1. start(roomToken, roomSessionId) — builds the pipeline with
 *     webrtcbin (audio+video send, audio receive, video appsink) and
 *     starts the GStreamer pipeline. webrtcbin fires
 *     on-negotiation-needed → sends a requestoffer via signaling.
 *  2. The MCU responds with an offer; the engine sets it as the remote
 *     description, creates an answer and sends it back via signaling.
 *  3. ICE candidates are exchanged via signaling in both directions.
 *  4. stop() releases the pipeline.
 */
class TalkMediaEngine : public QObject
{
    Q_OBJECT
public:
    explicit TalkMediaEngine(TalkSignalingClient *signaling, QObject *parent = nullptr);
    ~TalkMediaEngine() override;

    void start(const QString &roomToken, const QString &roomSessionId);
    void stop();
    void setMicrophoneEnabled(bool enabled);

signals:
    void mediaConnected();
    void mediaStopped();
    void remoteVideoFrame(QImage frame);
    void errorOccurred(const QString &message);
    void sendSignalingMessage(const QJsonObject &data);

private:
    void buildPipeline();
    void destroyPipeline();
    void requestOffer();
    void handleSignalingMessage(const QJsonObject &data);
    void handleOffer(const QString &sdp);
    void handleCandidate(const QJsonObject &candidate);
    void sendAnswer(const QString &sdp);
    void sendIceCandidate(const QString &candidate, guint mlineIndex);

    static void onNegotiationNeededCb(GstElement *webrtcbin, gpointer user_data);
    static void onWebrtcPadAddedCb(GstElement *webrtcbin, GstPad *newPad, gpointer user_data);
    static GstElement *createSourceElement(bool audio);
    static void onIceCandidateCb(GstElement *webrtcbin, guint mlineIndex,
                                 gchararray candidate, gpointer user_data);
    static void onIceGatheringStateNotifyCb(GstElement *webrtcbin, gpointer user_data);
    static void onRemoteDescriptionSetCb(GstPromise *promise, gpointer user_data);
    static void onAnswerCreatedCb(GstPromise *promise, gpointer user_data);
    static GstFlowReturn onVideoAppsinkCb(GstAppSink *appsink, gpointer user_data);

    TalkSignalingClient *_signaling = nullptr;
    QString _roomToken;
    QString _roomSessionId;
    QString _offerSid;   // publisher session id from the MCU offer (echoed back)
    QString _roomType = QStringLiteral("video");
    GstElement *_pipeline = nullptr;
    GstElement *_webrtcbin = nullptr;
    bool _mediaConnected = false;
    bool _micEnabled = true;
    gulong _negotiationHandlerId = 0;
    gulong _iceCandidateHandlerId = 0;
};

} // namespace OCC

#endif // HAVE_GSTREAMER

#endif // TALKMEDIAENGINE_H
