/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "IncomingCallDialog.h"
#include "theme/SouveraTheme.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QMediaDevices>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QAudioSink>

namespace OCC {

namespace {

// Dual-tone ring: 440 + 480 Hz, 2 s on / 4 s off (German incoming ring).
constexpr int RingCycleMs = 6000;
constexpr int RingToneMs = 2000;
constexpr int RingHz1 = 440;
constexpr int RingHz2 = 480;

QByteArray makeIncomingRingCycle()
{
    constexpr double kTwoPi = 6.28318530717958647692;
    const int sampleRate = 48000;
    const int total = sampleRate * RingCycleMs / 1000;
    const int toneSamples = sampleRate * RingToneMs / 1000;
    QByteArray data;
    data.resize(total * 2);
    auto *out = reinterpret_cast<qint16 *>(data.data());
    for (int i = 0; i < total; ++i) {
        if (i < toneSamples) {
            const double fade = qMin(1.0, double(i) / (sampleRate / 100));
            const auto v = qSin(kTwoPi * RingHz1 * i / sampleRate) * 0.2 * 32767.0 * fade
                         + qSin(kTwoPi * RingHz2 * i / sampleRate) * 0.2 * 32767.0 * fade;
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
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    _ringSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    _ringIo = _ringSink->start();
    if (!_ringIo) return;
    _ringIo->write(makeIncomingRingCycle());
    _ringTimer = new QTimer(this);
    _ringTimer->setInterval(RingCycleMs);
    connect(_ringTimer, &QTimer::timeout, this, [this]() {
        if (_ringIo) _ringIo->write(makeIncomingRingCycle());
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
