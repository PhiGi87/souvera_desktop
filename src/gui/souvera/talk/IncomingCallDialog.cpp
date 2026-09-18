/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "IncomingCallDialog.h"
#include "settings/MediaDeviceSettings.h"
#include "theme/SouveraTheme.h"

#include <QAudioDevice>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QMediaDevices>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#include <QAudioSink>

namespace OCC {

namespace {

// Dual-tone ring: 440 + 480 Hz, 2 s on / 4 s off (German incoming ring).
constexpr int RingCycleMs = 6000;
constexpr int RingToneMs = 2000;
constexpr int RingChunkMs = 100;
constexpr int RingHz1 = 440;
constexpr int RingHz2 = 480;
constexpr int RingSampleRate = 48000;

// Generates the next RingChunkMs slice of the endless ring pattern
// continuing at phase.
QByteArray makeIncomingRingChunk(qint64 &phase)
{
    constexpr double kTwoPi = 6.28318530717958647692;
    const int samples = RingSampleRate * RingChunkMs / 1000;
    const qint64 cycleSamples = qint64(RingSampleRate) * RingCycleMs / 1000;
    const qint64 toneSamples = qint64(RingSampleRate) * RingToneMs / 1000;
    QByteArray data;
    data.resize(samples * 2);
    auto *out = reinterpret_cast<qint16 *>(data.data());
    for (int i = 0; i < samples; ++i, ++phase) {
        const qint64 inCycle = phase % cycleSamples;
        if (inCycle < toneSamples) {
            const double fade = qMin(1.0, double(inCycle) / (RingSampleRate / 100));
            const auto v = qSin(kTwoPi * RingHz1 * inCycle / RingSampleRate) * 0.2 * 32767.0 * fade
                         + qSin(kTwoPi * RingHz2 * inCycle / RingSampleRate) * 0.2 * 32767.0 * fade;
            out[i] = qint16(v);
        } else {
            out[i] = 0;
        }
    }
    return data;
}

} // namespace

IncomingCallDialog::IncomingCallDialog(const QString &roomToken, const QString &callerName,
                                       QWidget *parent)
    : QDialog(parent, Qt::Window | Qt::WindowStaysOnTopHint)
    , _roomToken(roomToken)
{
    setWindowTitle(QStringLiteral("Eingehender Anruf"));
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedSize(380, 280);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *icon = new QLabel(QStringLiteral("\U0001F4DE"), this);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral("font-size: 48px;"));
    layout->addWidget(icon);

    _callerLabel = new QLabel(callerName, this);
    _callerLabel->setObjectName(QStringLiteral("IncomingCallerName"));
    _callerLabel->setAlignment(Qt::AlignCenter);
    _callerLabel->setWordWrap(true);
    layout->addWidget(_callerLabel);

    auto *hint = new QLabel(QStringLiteral("Eingehender Anruf"), this);
    hint->setObjectName(QStringLiteral("IncomingCallHint"));
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    layout->addStretch();

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(16);

    _declineButton = new QPushButton(this);
    _declineButton->setObjectName(QStringLiteral("IncomingDeclineButton"));
    _declineButton->setFixedSize(64, 64);
    _declineButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("phone-off"), SouveraTheme::Color::TextPrimary));
    _declineButton->setIconSize(QSize(28, 28));
    _declineButton->setToolTip(QStringLiteral("Ablehnen"));
    connect(_declineButton, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(_declineButton);
    buttons->addStretch();

    _acceptButton = new QPushButton(this);
    _acceptButton->setObjectName(QStringLiteral("IncomingAcceptButton"));
    _acceptButton->setFixedSize(64, 64);
    _acceptButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("phone-incoming"), SouveraTheme::Color::TextPrimary));
    _acceptButton->setIconSize(QSize(28, 28));
    _acceptButton->setToolTip(QStringLiteral("Annehmen"));
    connect(_acceptButton, &QPushButton::clicked, this, [this]() {
        emit accepted(_roomToken);
        accept();
    });
    buttons->addWidget(_acceptButton);
    buttons->addStretch();

    layout->addLayout(buttons);

    startRing();
}

IncomingCallDialog::~IncomingCallDialog()
{
    stopRing();
}

void IncomingCallDialog::startRing()
{
    if (_ringSink) return;
    QAudioFormat format;
    format.setSampleRate(RingSampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    // Honor the output device the user picked in Audio & Video settings.
    const QAudioDevice device = MediaDeviceSettings::outputDevice();
    _ringSink = new QAudioSink(device, format, this);
    _ringIo = _ringSink->start();
    if (!_ringIo) return;
    // Feed small chunks on a short timer so writes never block the UI thread.
    _ringTimer = new QTimer(this);
    _ringTimer->setInterval(RingChunkMs);
    connect(_ringTimer, &QTimer::timeout, this, [this]() {
        if (_ringIo) _ringIo->write(makeIncomingRingChunk(_ringPhase));
    });
    _ringTimer->start();
}

void IncomingCallDialog::stopRing()
{
    if (_ringTimer) { _ringTimer->stop(); delete _ringTimer; _ringTimer = nullptr; }
    if (_ringIo) { _ringIo->close(); _ringIo = nullptr; }
    if (_ringSink) { _ringSink->stop(); delete _ringSink; _ringSink = nullptr; }
}

void IncomingCallDialog::closeEvent(QCloseEvent *event)
{
    stopRing();
    QDialog::closeEvent(event);
}

} // namespace OCC
