/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TalkMediaEngine.h"

#ifdef HAVE_GSTREAMER

#include "TalkSignalingClient.h"

#include <QImage>
#include <QLoggingCategory>
#include <QSettings>

#include <gst/app/app.h>
#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

Q_LOGGING_CATEGORY(lcTalkMediaEngine, "souvera.talk.mediaengine", QtInfoMsg)

namespace OCC {

namespace {
constexpr auto kWebrtcbinName = "talk-sendrecv";
constexpr auto kVideoAppsinkName = "remote-video-sink";
constexpr double kTwoPi = 6.28318530717958647692;
} // namespace

TalkMediaEngine::TalkMediaEngine(TalkSignalingClient *signaling, QObject *parent)
    : QObject(parent)
    , _signaling(signaling)
{
    if (_signaling) {
        connect(_signaling, &TalkSignalingClient::mediaMessageReceived,
                this, &TalkMediaEngine::handleSignalingMessage);
        connect(this, &TalkMediaEngine::sendSignalingMessage, this,
                [this](const QJsonObject &data) {
            if (_signaling) _signaling->sendMediaMessage(data);
        });
    }
}

TalkMediaEngine::~TalkMediaEngine()
{
    stop();
}

void TalkMediaEngine::start(const QString &roomToken, const QString &roomSessionId)
{
    _roomToken = roomToken;
    _roomSessionId = roomSessionId;
    buildPipeline();
}

void TalkMediaEngine::stop()
{
    destroyPipeline();
    if (_mediaConnected) {
        _mediaConnected = false;
        emit mediaStopped();
    }
}

void TalkMediaEngine::setMicrophoneEnabled(bool enabled)
{
    _micEnabled = enabled;
    if (_pipeline) {
        auto *src = gst_bin_get_by_name(GST_BIN(_pipeline), "audio-src");
        if (src) {
            g_object_set(src, "mute", !enabled, nullptr);
            gst_object_unref(src);
        }
    }
}

void TalkMediaEngine::buildPipeline()
{
    if (_pipeline) return;

    _pipeline = gst_pipeline_new(nullptr);
    _webrtcbin = gst_element_factory_make("webrtcbin", kWebrtcbinName);
    if (!_webrtcbin || !_pipeline) {
        qCCritical(lcTalkMediaEngine) << "Failed to create webrtcbin";
        emit errorOccurred(QStringLiteral("GStreamer webrtcbin nicht verf\u00FCgbar."));
        if (_pipeline) { gst_object_unref(_pipeline); _pipeline = nullptr; }
        if (_webrtcbin) { gst_object_unref(_webrtcbin); _webrtcbin = nullptr; }
        return;
    }

    g_object_set_data_full(G_OBJECT(_webrtcbin), "engine-ptr", this, nullptr);
    g_object_set(_webrtcbin, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, nullptr);
    gst_bin_add(GST_BIN(_pipeline), _webrtcbin);

    // Audio send
    auto *audiosrc = createSourceElement(true);
    if (audiosrc) gst_object_ref_sink(audiosrc);
    if (audiosrc) {
        auto *conv = gst_element_factory_make("audioconvert", nullptr);
        auto *resample = gst_element_factory_make("audioresample", nullptr);
        auto *queue = gst_element_factory_make("queue", nullptr);
        auto *enc = gst_element_factory_make("opusenc", nullptr);
        auto *pay = gst_element_factory_make("rtpopuspay", nullptr);
        gst_bin_add_many(GST_BIN(_pipeline), audiosrc, conv, resample, queue, enc, pay, nullptr);
        gst_element_link_many(audiosrc, conv, resample, queue, enc, pay, nullptr);
        auto *srcPad = gst_element_get_static_pad(pay, "src");
        auto *sinkPad = gst_element_request_pad_simple(_webrtcbin, "sink_%u");
        gst_pad_link(srcPad, sinkPad);
        gst_object_unref(srcPad); gst_object_unref(sinkPad);
    } else {
        qCWarning(lcTalkMediaEngine) << "No audio source, audio send disabled";
    }

    // Video send
    auto *videosrc = createSourceElement(false);
    if (videosrc) gst_object_ref_sink(videosrc);
    if (videosrc) {
        auto *conv = gst_element_factory_make("videoconvert", nullptr);
        auto *scale = gst_element_factory_make("videoscale", nullptr);
        auto *rate = gst_element_factory_make("videorate", nullptr);
        auto *capsflt = gst_element_factory_make("capsfilter", nullptr);
        auto *caps = gst_caps_from_string("video/x-raw,width=640,height=480,framerate=15/1");
        g_object_set(capsflt, "caps", caps, nullptr); gst_caps_unref(caps);
        auto *queue = gst_element_factory_make("queue", nullptr);
        auto *enc = gst_element_factory_make("vp8enc", nullptr);
        g_object_set(enc, "deadline", 1, "cpu-used", 8, "threads", 2, nullptr);
        auto *pay = gst_element_factory_make("rtpvp8pay", nullptr);
        g_object_set(pay, "mtu", 1200, nullptr);
        gst_bin_add_many(GST_BIN(_pipeline), videosrc, conv, scale, rate, capsflt, queue, enc, pay, nullptr);
        gst_element_link_many(videosrc, conv, scale, rate, capsflt, queue, enc, pay, nullptr);
        auto *srcPad = gst_element_get_static_pad(pay, "src");
        auto *sinkPad = gst_element_request_pad_simple(_webrtcbin, "sink_%u");
        gst_pad_link(srcPad, sinkPad);
        gst_object_unref(srcPad); gst_object_unref(sinkPad);
    } else {
        qCWarning(lcTalkMediaEngine) << "No video source, video send disabled";
    }

    // Remote media: handle pad-added on webrtcbin for incoming streams.
    g_signal_connect(_webrtcbin, "pad-added",
        G_CALLBACK(&TalkMediaEngine::onWebrtcPadAddedCb), this);

    // Video receive: appsink → QImage
    auto *videosink = gst_element_factory_make("appsink", kVideoAppsinkName);
    if (videosink) {
        g_object_set(videosink, "emit-signals", TRUE, "sync", FALSE,
                     "max-buffers", 2, "drop", TRUE, nullptr);
        auto *caps = gst_caps_from_string("video/x-raw,format=RGB");
        g_object_set(videosink, "caps", caps, nullptr);
        gst_caps_unref(caps);
        gst_bin_add(GST_BIN(_pipeline), videosink);
        g_signal_connect(videosink, "new-sample",
            G_CALLBACK(&TalkMediaEngine::onVideoAppsinkCb), this);
    }

    _negotiationHandlerId = g_signal_connect(_webrtcbin, "on-negotiation-needed",
        G_CALLBACK(&TalkMediaEngine::onNegotiationNeededCb), this);
    _iceCandidateHandlerId = g_signal_connect(_webrtcbin, "on-ice-candidate",
        G_CALLBACK(&TalkMediaEngine::onIceCandidateCb), this);
    g_signal_connect(_webrtcbin, "on-ice-gathering-state-notify",
        G_CALLBACK(&TalkMediaEngine::onIceGatheringStateNotifyCb), this);

    gst_element_set_state(_pipeline, GST_STATE_PLAYING);
    qCInfo(lcTalkMediaEngine) << "Media pipeline started for room" << _roomToken;
}

void TalkMediaEngine::destroyPipeline()
{
    if (_pipeline) {
        gst_element_set_state(_pipeline, GST_STATE_NULL);
        gst_object_unref(_pipeline);
        _pipeline = nullptr;
        _webrtcbin = nullptr;
    }
    _negotiationHandlerId = 0;
    _iceCandidateHandlerId = 0;
}

// ---------------------------------------------------------------------------
// webrtcbin callbacks
// ---------------------------------------------------------------------------

void TalkMediaEngine::onWebrtcPadAddedCb(GstElement *wb, GstPad *newPad, gpointer user_data)
{
    auto *self = static_cast<TalkMediaEngine *>(user_data);
    auto *parent = gst_element_get_parent(wb); // transfer full
    if (!parent) return;
    auto *bin = GST_BIN(parent);

    auto *caps = gst_pad_get_current_caps(newPad);
    if (!caps) caps = gst_pad_query_caps(newPad, nullptr);
    auto *st = gst_caps_get_structure(caps, 0);
    const gchar *media = gst_structure_get_string(st, "media");
    const bool isAudio = media && g_strcmp0(media, "audio") == 0;
    const bool isVideo = media && g_strcmp0(media, "video") == 0;
    gst_caps_unref(caps);

    if (isAudio) {
        auto *depay = gst_element_factory_make("rtpopusdepay", nullptr);
        auto *dec = gst_element_factory_make("opusdec", nullptr);
        auto *conv = gst_element_factory_make("audioconvert", nullptr);
        auto *resample = gst_element_factory_make("audioresample", nullptr);
        auto *sink = gst_element_factory_make("autoaudiosink", nullptr);
        if (depay && dec && conv && resample && sink) {
            gst_bin_add_many(bin, depay, dec, conv, resample, sink, nullptr);
            gst_element_link_many(depay, dec, conv, resample, sink, nullptr);
            auto *sp = gst_element_get_static_pad(depay, "sink");
            gst_pad_link(newPad, sp);
            gst_object_unref(sp);
            gst_element_sync_state_with_parent(depay);
            gst_element_sync_state_with_parent(dec);
            gst_element_sync_state_with_parent(conv);
            gst_element_sync_state_with_parent(resample);
            gst_element_sync_state_with_parent(sink);
            qCInfo(lcTalkMediaEngine) << "Remote audio pad linked via opus decode chain";
        } else {
            qCWarning(lcTalkMediaEngine) << "Audio decode chain unavailable, dropping remote audio";
            if (depay) gst_object_unref(depay);
            if (dec) gst_object_unref(dec);
            if (conv) gst_object_unref(conv);
            if (resample) gst_object_unref(resample);
            if (sink) gst_object_unref(sink);
        }
    } else if (isVideo) {
        auto *depay = gst_element_factory_make("rtpvp8depay", nullptr);
        auto *dec = gst_element_factory_make("vp8dec", nullptr);
        auto *conv = gst_element_factory_make("videoconvert", nullptr);
        auto *sink = gst_bin_get_by_name(bin, kVideoAppsinkName);
        if (depay && dec && conv && sink) {
            gst_bin_add_many(bin, depay, dec, conv, nullptr);
            gst_element_link_many(depay, dec, conv, sink, nullptr);
            auto *sp = gst_element_get_static_pad(depay, "sink");
            gst_pad_link(newPad, sp);
            gst_object_unref(sp);
            gst_element_sync_state_with_parent(depay);
            gst_element_sync_state_with_parent(dec);
            gst_element_sync_state_with_parent(conv);
            qCInfo(lcTalkMediaEngine) << "Remote video pad linked via vp8 decode chain";
        } else {
            qCWarning(lcTalkMediaEngine) << "Video decode chain unavailable, dropping remote video";
            if (depay) gst_object_unref(depay);
            if (dec) gst_object_unref(dec);
            if (conv) gst_object_unref(conv);
        }
        if (sink) gst_object_unref(sink);
    }
    gst_object_unref(parent);
}

// Returns a GStreamer source element matching the device description the
// user picked in Audio & Video settings (matched against GStreamer device
// display names). Falls back to the auto source when nothing matches or the
// monitor yields no devices.
GstElement *TalkMediaEngine::createSourceElement(bool audio)
{
    const QString settingsKey = audio ? QStringLiteral("audioInputDescription")
                                      : QStringLiteral("cameraDescription");
    QSettings settings;
    const QString wanted = settings.value(settingsKey).toString();
    if (wanted.isEmpty()) {
        return gst_element_factory_make(audio ? "autoaudiosrc" : "autovideosrc", nullptr);
    }

    auto *monitor = gst_device_monitor_new();
    gst_device_monitor_add_filter(monitor,
        audio ? "Audio/Source" : "Video/Source", nullptr);
    gst_device_monitor_start(monitor);
    GstElement *element = nullptr;
    const GList *devices = gst_device_monitor_get_devices(monitor);
    for (const GList *l = devices; l; l = l->next) {
        auto *device = GST_DEVICE(l->data);
        const QString display = QString::fromUtf8(gst_device_get_display_name(device));
        if (display == wanted) {
            element = gst_device_create_element(device, nullptr);
            break;
        }
    }
    gst_device_monitor_stop(monitor);
    gst_object_unref(monitor);
    if (element) {
        qCInfo(lcTalkMediaEngine) << "Using configured device" << wanted;
        return element;
    }
    qCWarning(lcTalkMediaEngine) << "Configured device not found, falling back to auto source";
    return gst_element_factory_make(audio ? "autoaudiosrc" : "autovideosrc", nullptr);
}

void TalkMediaEngine::onNegotiationNeededCb(GstElement *, gpointer user_data)
{
    auto *self = static_cast<TalkMediaEngine *>(user_data);
    qCInfo(lcTalkMediaEngine) << "on-negotiation-needed: requesting offer from MCU";
    self->requestOffer();
}

void TalkMediaEngine::requestOffer()
{
    QJsonObject data;
    data.insert(QStringLiteral("type"), QStringLiteral("requestoffer"));
    data.insert(QStringLiteral("roomType"), QStringLiteral("video"));
    data.insert(QStringLiteral("sid"), _roomSessionId);
    emit sendSignalingMessage(data);
}

void TalkMediaEngine::onIceCandidateCb(GstElement *, guint mlineIndex,
                                       gchararray candidate, gpointer user_data)
{
    auto *self = static_cast<TalkMediaEngine *>(user_data);
    qCInfo(lcTalkMediaEngine) << "Local ICE candidate:" << candidate;
    QJsonObject data;
    data.insert(QStringLiteral("type"), QStringLiteral("candidate"));
    QJsonObject cobj;
    cobj.insert(QStringLiteral("candidate"), QString::fromUtf8(candidate));
    cobj.insert(QStringLiteral("sdpMLineIndex"), int(mlineIndex));
    data.insert(QStringLiteral("payload"), cobj);
    emit self->sendSignalingMessage(data);
}

void TalkMediaEngine::onIceGatheringStateNotifyCb(GstElement *webrtcbin, gpointer)
{
    GstWebRTCICEGatheringState state;
    g_object_get(webrtcbin, "ice-gathering-state", &state, nullptr);
    qCInfo(lcTalkMediaEngine) << "ICE gathering state:" << int(state);
}

void TalkMediaEngine::onRemoteDescriptionSetCb(GstPromise *promise, gpointer user_data)
{
    auto *self = static_cast<TalkMediaEngine *>(user_data);
    gst_promise_unref(promise);
    auto *wb = self->_webrtcbin;
    if (!wb) return;

    GstPromise *answerPromise = gst_promise_new_with_change_func(
        reinterpret_cast<GstPromiseChangeFunc>(&TalkMediaEngine::onAnswerCreatedCb),
        self, nullptr);
    g_signal_emit_by_name(wb, "create-answer", nullptr, nullptr, answerPromise);
}

void TalkMediaEngine::onAnswerCreatedCb(GstPromise *promise, gpointer user_data)
{
    auto *self = static_cast<TalkMediaEngine *>(user_data);
    auto *wb = self->_webrtcbin;
    if (!wb) { gst_promise_unref(promise); return; }

    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *answer = nullptr;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, nullptr);
    gst_promise_unref(promise);
    if (!answer || !answer->sdp) {
        qCWarning(lcTalkMediaEngine) << "Answer creation failed";
        if (answer) gst_webrtc_session_description_free(answer);
        return;
    }

    gchar *sdpText = gst_sdp_message_as_text(answer->sdp);
    qCInfo(lcTalkMediaEngine) << "Answer created:" << strlen(sdpText) << "chars";

    // Set local description with the answer.
    GstSDPMessage *sdpMsg = nullptr;
    gst_sdp_message_new_from_text(sdpText, &sdpMsg);
    GstWebRTCSessionDescription *localDesc =
        gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdpMsg);
    GstPromise *localPromise = gst_promise_new();
    g_signal_emit_by_name(wb, "set-local-description", localDesc, localPromise);
    gst_promise_unref(localPromise);

    // Send the answer via signaling.
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("answer"));
    payload.insert(QStringLiteral("sdp"), QString::fromUtf8(sdpText));
    QJsonObject data;
    data.insert(QStringLiteral("type"), QStringLiteral("answer"));
    data.insert(QStringLiteral("roomType"), QStringLiteral("video"));
    data.insert(QStringLiteral("payload"), payload);
    emit self->sendSignalingMessage(data);

    g_free(sdpText);
    gst_webrtc_session_description_free(localDesc);
    gst_webrtc_session_description_free(answer);

    self->_mediaConnected = true;
    QPointer<TalkMediaEngine> selfPtr(self);
    QMetaObject::invokeMethod(self, [selfPtr]() {
        if (selfPtr) emit selfPtr->mediaConnected();
    }, Qt::QueuedConnection);
}

GstFlowReturn TalkMediaEngine::onVideoAppsinkCb(GstAppSink *appsink, gpointer user_data)
{
    auto *self = static_cast<TalkMediaEngine *>(user_data);
    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_OK;

    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *st = gst_caps_get_structure(caps, 0);
    int w = 0, h = 0;
    gst_structure_get_int(st, "width", &w);
    gst_structure_get_int(st, "height", &h);

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        if (w > 0 && h > 0 && map.size >= gsize(w * h * 3)) {
            QImage frame(map.data, w, h, w * 3, QImage::Format_RGB888);
            if (!frame.isNull()) {
                QImage copy = frame.copy();
                QPointer<TalkMediaEngine> selfPtr(self);
                QMetaObject::invokeMethod(self, [selfPtr, copy]() {
                    if (selfPtr) emit selfPtr->remoteVideoFrame(copy);
                }, Qt::QueuedConnection);
            }
        }
        gst_buffer_unmap(buffer, &map);
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// ---------------------------------------------------------------------------
// Signaling media message handling
// ---------------------------------------------------------------------------

void TalkMediaEngine::handleSignalingMessage(const QJsonObject &data)
{
    const auto type = data.value(QStringLiteral("type")).toString();
    qCInfo(lcTalkMediaEngine) << "Signaling media message:" << type;

    if (type == QStringLiteral("offer")) {
        const auto payload = data.value(QStringLiteral("payload")).toObject();
        const auto sdp = payload.value(QStringLiteral("sdp")).toString();
        if (!sdp.isEmpty()) handleOffer(sdp);
    } else if (type == QStringLiteral("candidate")) {
        const auto candidate = data.value(QStringLiteral("payload")).toObject();
        const auto cstr = candidate.value(QStringLiteral("candidate")).toString();
        const auto mline = guint(candidate.value(QStringLiteral("sdpMLineIndex")).toInt());
        if (!cstr.isEmpty() && _webrtcbin) {
            g_signal_emit_by_name(_webrtcbin, "add-ice-candidate", mline, cstr.toUtf8().constData());
        }
    }
}

void TalkMediaEngine::handleOffer(const QString &sdp)
{
    if (!_webrtcbin) return;
    qCInfo(lcTalkMediaEngine) << "Setting remote offer SDP (" << sdp.size() << "chars)";
    GstSDPMessage *sdpMsg = nullptr;
    if (gst_sdp_message_new_from_text(sdp.toUtf8().constData(), &sdpMsg) != GST_SDP_OK) {
        qCWarning(lcTalkMediaEngine) << "Malformed remote SDP, ignoring offer";
        return;
    }
    GstWebRTCSessionDescription *remoteDesc =
        gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdpMsg);
    GstPromise *promise = gst_promise_new_with_change_func(
        reinterpret_cast<GstPromiseChangeFunc>(&TalkMediaEngine::onRemoteDescriptionSetCb),
        this, nullptr);
    g_signal_emit_by_name(_webrtcbin, "set-remote-description", remoteDesc, promise);
    gst_webrtc_session_description_free(remoteDesc);
}

void TalkMediaEngine::sendAnswer(const QString &sdp)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("answer"));
    payload.insert(QStringLiteral("sdp"), sdp);
    QJsonObject data;
    data.insert(QStringLiteral("type"), QStringLiteral("answer"));
    data.insert(QStringLiteral("roomType"), QStringLiteral("video"));
    data.insert(QStringLiteral("payload"), payload);
    emit sendSignalingMessage(data);
}

void TalkMediaEngine::sendIceCandidate(const QString &candidate, guint mlineIndex)
{
    QJsonObject cobj;
    cobj.insert(QStringLiteral("candidate"), candidate);
    cobj.insert(QStringLiteral("sdpMLineIndex"), int(mlineIndex));
    QJsonObject data;
    data.insert(QStringLiteral("type"), QStringLiteral("candidate"));
    data.insert(QStringLiteral("payload"), cobj);
    emit sendSignalingMessage(data);
}

} // namespace OCC

#endif // HAVE_GSTREAMER
