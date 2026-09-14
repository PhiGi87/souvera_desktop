/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AudioVideoSettings.h"
#include "theme/SouveraMetrics.h"
#include <QAudioDevice>
#include <QAudioSink>
#include <QAudioSource>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QLoggingCategory>
#include <QMediaDevices>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

Q_LOGGING_CATEGORY(lcAudioVideoSettings, "souvera.settings.audiovideo")

namespace OCC {

namespace Metrics = Sou::Metrics;

namespace {

constexpr int SampleRate = 48000;
constexpr int ToneMs = 500;
constexpr int ToneHz = 440;
constexpr int LevelTimerMs = 60;

QString settingsGroup()
{
    return QStringLiteral("souvera/call");
}

// A 0.5 s 440 Hz sine with short fade in/out, 48 kHz mono 16-bit.
QByteArray makeTestTone()
{
    constexpr double kTwoPi = 6.28318530717958647692;
    const int samples = SampleRate * ToneMs / 1000;
    QByteArray data;
    data.resize(samples * 2);
    auto *out = reinterpret_cast<qint16 *>(data.data());
    const int fadeIn = SampleRate / 100;
    const int fadeOut = SampleRate / 50;
    for (int i = 0; i < samples; ++i) {
        double envelope = 1.0;
        if (i < fadeIn) envelope = double(i) / fadeIn;
        else if (i > samples - fadeOut) envelope = double(samples - i) / fadeOut;
        const auto value = qSin(kTwoPi * ToneHz * i / SampleRate) * 0.3 * 32767.0 * envelope;
        out[i] = qint16(value);
    }
    return data;
}

} // namespace

AudioVideoSettings::AudioVideoSettings(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Metrics::CardSpacing);

    auto *inputTitle = new QLabel(QStringLiteral("Mikrofon"), this);
    inputTitle->setObjectName(QStringLiteral("SettingsCardTitle"));
    layout->addWidget(inputTitle);

    _inputCombo = new QComboBox(this);
    _inputCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    layout->addWidget(_inputCombo);

    auto *micRow = new QHBoxLayout;
    micRow->setSpacing(Metrics::SpacingS);
    _micTestButton = new QPushButton(QStringLiteral("Mikrofon testen"), this);
    _micTestButton->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    _micTestButton->setCheckable(true);
    micRow->addWidget(_micTestButton);

    _levelBar = new QProgressBar(this);
    _levelBar->setObjectName(QStringLiteral("AudioLevelBar"));
    _levelBar->setRange(0, 100);
    _levelBar->setValue(0);
    _levelBar->setTextVisible(false);
    micRow->addWidget(_levelBar, 1);
    layout->addLayout(micRow);

    auto *outputTitle = new QLabel(QStringLiteral("Lautsprecher"), this);
    outputTitle->setObjectName(QStringLiteral("SettingsCardTitle"));
    layout->addWidget(outputTitle);

    _outputCombo = new QComboBox(this);
    _outputCombo->setObjectName(QStringLiteral("AudioDeviceCombo"));
    layout->addWidget(_outputCombo);

    auto *outputRow = new QHBoxLayout;
    outputRow->setSpacing(Metrics::SpacingS);
    _outputTestButton = new QPushButton(QStringLiteral("Ausgabe testen"), this);
    _outputTestButton->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    outputRow->addWidget(_outputTestButton);
    outputRow->addStretch();
    layout->addLayout(outputRow);

    auto *hint = new QLabel(
        QStringLiteral("Die Auswahl wird f\u00FCr Anrufe in Link verwendet."), this);
    hint->setObjectName(QStringLiteral("FolderStatusLabel"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch();

    _mediaDevices = new QMediaDevices(this);
    populateDevices();
    connect(_mediaDevices, &QMediaDevices::audioInputsChanged, this, &AudioVideoSettings::populateDevices);
    connect(_mediaDevices, &QMediaDevices::audioOutputsChanged, this, &AudioVideoSettings::populateDevices);

    connect(_inputCombo, &QComboBox::activated, this, [this](int) {
        storeSelection();
        if (_audioSource) {
            // The running test captures from the previously selected device.
            stopMicTest();
            startMicTest();
        }
    });
    connect(_outputCombo, &QComboBox::activated, this, [this](int) { storeSelection(); });

    connect(_micTestButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) startMicTest(); else stopMicTest();
    });
    connect(_outputTestButton, &QPushButton::clicked, this, [this]() { playTestTone(); });

    _levelTimer = new QTimer(this);
    _levelTimer->setInterval(LevelTimerMs);
    connect(_levelTimer, &QTimer::timeout, this, [this]() {
        if (!_sourceIo) return;
        // Peak level of the captured block, smoothed for a readable meter.
        const auto data = _sourceIo->readAll();
        const auto *samples = reinterpret_cast<const qint16 *>(data.constData());
        const auto count = data.size() / 2;
        qint16 peak = 0;
        for (int i = 0; i < count; ++i) {
            peak = qMax(peak, qint16(qAbs(samples[i])));
        }
        const auto level = int(qMin(100.0, peak / 32767.0 * 100.0 * 1.4));
        _levelSmoothed = qMax(level, _levelSmoothed * 70 / 100);
        _levelBar->setValue(_levelSmoothed);
    });
}

AudioVideoSettings::~AudioVideoSettings()
{
    stopMicTest();
    stopTestTone();
}

void AudioVideoSettings::populateDevices()
{
    _inputCombo->blockSignals(true);
    _outputCombo->blockSignals(true);
    _inputCombo->clear();
    _outputCombo->clear();

    _inputCombo->addItem(QStringLiteral("Systemstandard"), QString());
    const auto inputs = QMediaDevices::audioInputs();
    for (const auto &device : inputs) {
        _inputCombo->addItem(device.description(), device.id());
    }
    _outputCombo->addItem(QStringLiteral("Systemstandard"), QString());
    const auto outputs = QMediaDevices::audioOutputs();
    for (const auto &device : outputs) {
        _outputCombo->addItem(device.description(), device.id());
    }

    // Restore the persisted selection.
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const auto storedInput = settings.value(QStringLiteral("audioInputId")).toString();
    const auto storedOutput = settings.value(QStringLiteral("audioOutputId")).toString();
    settings.endGroup();

    const int inputIndex = _inputCombo->findData(storedInput);
    _inputCombo->setCurrentIndex(inputIndex >= 0 ? inputIndex : 0);
    const int outputIndex = _outputCombo->findData(storedOutput);
    _outputCombo->setCurrentIndex(outputIndex >= 0 ? outputIndex : 0);

    _inputCombo->blockSignals(false);
    _outputCombo->blockSignals(false);
}

void AudioVideoSettings::storeSelection()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.setValue(QStringLiteral("audioInputId"), _inputCombo->currentData().toString());
    settings.setValue(QStringLiteral("audioOutputId"), _outputCombo->currentData().toString());
    settings.endGroup();
}

QAudioDevice AudioVideoSettings::inputDevice()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const auto stored = settings.value(QStringLiteral("audioInputId")).toString();
    settings.endGroup();
    if (!stored.isEmpty()) {
        const auto inputs = QMediaDevices::audioInputs();
        for (const auto &device : inputs) {
            if (device.id() == stored.toUtf8()) return device;
        }
    }
    return QMediaDevices::defaultAudioInput();
}

QAudioDevice AudioVideoSettings::outputDevice()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const auto stored = settings.value(QStringLiteral("audioOutputId")).toString();
    settings.endGroup();
    if (!stored.isEmpty()) {
        const auto outputs = QMediaDevices::audioOutputs();
        for (const auto &device : outputs) {
            if (device.id() == stored.toUtf8()) return device;
        }
    }
    return QMediaDevices::defaultAudioOutput();
}

void AudioVideoSettings::startMicTest()
{
    const auto device = inputDevice();
    if (device.isNull()) {
        _micTestButton->setChecked(false);
        return;
    }

    QAudioFormat format;
    format.setSampleRate(SampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    _audioSource = new QAudioSource(device, format, this);
    _sourceIo = _audioSource->start();
    if (!_sourceIo) {
        qCWarning(lcAudioVideoSettings) << "Microphone test failed to start";
        delete _audioSource;
        _audioSource = nullptr;
        _micTestButton->setChecked(false);
        return;
    }
    _levelTimer->start();
    _micTestButton->setText(QStringLiteral("Test beenden"));
}

void AudioVideoSettings::stopMicTest()
{
    _levelTimer->stop();
    if (_sourceIo) {
        _sourceIo->close();
        _sourceIo = nullptr;
    }
    if (_audioSource) {
        _audioSource->stop();
        delete _audioSource;
        _audioSource = nullptr;
    }
    _levelSmoothed = 0;
    _levelBar->setValue(0);
    _micTestButton->setText(QStringLiteral("Mikrofon testen"));
}

void AudioVideoSettings::playTestTone()
{
    stopTestTone();
    const auto device = outputDevice();
    if (device.isNull()) return;

    QAudioFormat format;
    format.setSampleRate(SampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    _audioSink = new QAudioSink(device, format, this);
    _sinkIo = _audioSink->start();
    if (!_sinkIo) {
        qCWarning(lcAudioVideoSettings) << "Output test failed to start";
        delete _audioSink;
        _audioSink = nullptr;
        return;
    }
    _sinkIo->write(makeTestTone());
    _toneStopTimer = new QTimer(this);
    _toneStopTimer->setSingleShot(true);
    _toneStopTimer->setInterval(ToneMs + 200);
    connect(_toneStopTimer, &QTimer::timeout, this, &AudioVideoSettings::stopTestTone);
    _toneStopTimer->start();
}

void AudioVideoSettings::stopTestTone()
{
    if (_toneStopTimer) {
        _toneStopTimer->stop();
        _toneStopTimer->deleteLater();
        _toneStopTimer = nullptr;
    }
    if (_sinkIo) {
        _sinkIo->close();
        _sinkIo = nullptr;
    }
    if (_audioSink) {
        _audioSink->stop();
        delete _audioSink;
        _audioSink = nullptr;
    }
}

// Settings pages live inside a QStackedWidget for the whole app lifetime;
// a running mic test must never continue once the page is hidden.
void AudioVideoSettings::hideEvent(QHideEvent *event)
{
    stopMicTest();
    stopTestTone();
    QWidget::hideEvent(event);
}

} // namespace OCC
