/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AUDIOVIDEOSETTINGS_H
#define AUDIOVIDEOSETTINGS_H
#include <QAudioDevice>
#include <QCameraDevice>
#include <QHideEvent>
#include <QWidget>

class QComboBox;
class QLabel;
class QMediaDevices;
class QProgressBar;
class QPushButton;
class QCamera;
class QMediaCaptureSession;
class QAudioSource;
class QAudioSink;
class QIODevice;
class QTimer;
class QVideoSink;

namespace OCC {

/**
 * @brief Settings page for call audio and video devices.
 *
 * Lets the user pick the microphone (input), speaker (output) and camera
 * (video) devices used for calls. The selection persists in the application
 * settings and can be verified right here: the output test button plays a
 * short tone through the selected speaker, the microphone test shows a live
 * level meter from the selected input and the camera test shows a live
 * preview from the selected camera.
 */
class AudioVideoSettings : public QWidget
{
    Q_OBJECT
public:
    explicit AudioVideoSettings(QWidget *parent = nullptr);
    ~AudioVideoSettings() override;

    /** Resolves the configured input device (or the system default). */
    [[nodiscard]] static QAudioDevice inputDevice();
    /** Resolves the configured output device (or the system default). */
    [[nodiscard]] static QAudioDevice outputDevice();
    /** Resolves the configured camera (or the system default). */
    [[nodiscard]] static QCameraDevice cameraDevice();

private:
    void populateDevices();
    void storeSelection();
    void startMicTest();
    void stopMicTest();
    void playTestTone();
    void stopTestTone();
    void startCameraPreview();
    void stopCameraPreview();
    void hideEvent(QHideEvent *event) override;

    QMediaDevices *_mediaDevices = nullptr;
    QComboBox *_inputCombo = nullptr;
    QComboBox *_outputCombo = nullptr;
    QComboBox *_cameraCombo = nullptr;
    QProgressBar *_levelBar = nullptr;
    QPushButton *_micTestButton = nullptr;
    QPushButton *_outputTestButton = nullptr;
    QPushButton *_cameraTestButton = nullptr;
    QLabel *_cameraPreview = nullptr;
    QCamera *_camera = nullptr;
    QMediaCaptureSession *_captureSession = nullptr;
    QVideoSink *_previewSink = nullptr;
    QAudioSource *_audioSource = nullptr;
    QIODevice *_sourceIo = nullptr;
    QAudioSink *_audioSink = nullptr;
    QIODevice *_sinkIo = nullptr;
    QTimer *_levelTimer = nullptr;
    QTimer *_toneStopTimer = nullptr;
    int _levelSmoothed = 0;
};

} // namespace OCC

#endif // AUDIOVIDEOSETTINGS_H
