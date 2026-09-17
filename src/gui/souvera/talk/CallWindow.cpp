/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CallWindow.h"
#include "TalkOcsApi.h"
#include "TalkSignalingClient.h"
#ifdef HAVE_GSTREAMER
#include "TalkMediaEngine.h"
#endif

#include "theme/SouveraTheme.h"

#include <QCloseEvent>
#include <QColor>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QMediaDevices>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QAudioSink>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCallWindow, "souvera.talk.callwindow")

namespace OCC {

namespace {

constexpr int RingCycleMs = 5000;   // 1 s tone + 4 s silence (German ring)
constexpr int RingToneMs = 1000;
constexpr int RingHz = 425;

constexpr double kTwoPi = 6.28318530717958647692;

QByteArray makeRingCycle()
{
    const int sampleRate = 48000;
    const int total = sampleRate * RingCycleMs / 1000;
    const int toneSamples = sampleRate * RingToneMs / 1000;
    QByteArray data;
    data.resize(total * 2);
    auto *out = reinterpret_cast<qint16 *>(data.data());
    for (int i = 0; i < total; ++i) {
        if (i < toneSamples) {
            const double fade = qMin(1.0, double(i) / (sampleRate / 100));
            out[i] = qint16(qSin(kTwoPi * RingHz * i / sampleRate) * 0.22 * 32767.0 * fade);
        } else {
            out[i] = 0;
        }
    }
    return data;
}

} // namespace

CallWindow::CallWindow(TalkOcsApi *api, TalkSignalingClient *signaling,
                       const QString &token, const QString &displayName,
                       const QUrl &roomUrl, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , _api(api)
    , _signaling(signaling)
    , _token(token)
    , _roomUrl(roomUrl)
{
    setWindowTitle(QStringLiteral("Anruf \u2014 %1").arg(displayName));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(520, 680);
    setObjectName(QStringLiteral("CallWindow"));

    buildUi(displayName);

    // Observe the join chain from TalkPanel (REST join -> signaling -> call).
    if (_api) {
        connect(_api, &TalkOcsApi::callStarted, this, [this](const QString &startedToken) {
            if (startedToken == _token) setState(State::InCall);
        });
        connect(_api, &TalkOcsApi::participantsReceived, this,
                [this](const QString &token, const QVector<TalkParticipant> &participants) {
            if (token != _token) return;
            _names.clear();
            for (const auto &p : participants) {
                if (!p.actorId.isEmpty()) {
                    _names.insert(p.actorId, p.displayName);
                }
            }
            updateTileRendering();
        });
        connect(_api, &TalkOcsApi::callLeft, this, [this](const QString &token) {
            if (token == _token) _callLeft = true;
        });
        connect(_api, &TalkOcsApi::apiError, this, [this](const QString &error) {
            if (_state == State::Connecting) {
                setState(State::Ended, error);
            }
        });
        _api->fetchParticipants(_token);
    }
    if (_signaling) {
        connect(_signaling, &TalkSignalingClient::participantsChanged, this,
                [this](const QString &token, const QVector<TalkParticipant> &participants) {
            if (token != _token) return;
            for (const auto &p : participants) {
                if (p.actorId.isEmpty()) continue;
                _inCallFlags.insert(p.actorId, p.inCall);
                if (_names.value(p.actorId).isEmpty()) {
                    _names.insert(p.actorId, p.actorId);
                }
            }
            updateTileRendering();
        });
    }

    setState(State::Connecting);
    startRingTone();
}

CallWindow::~CallWindow()
{
    stopRingTone();
}

void CallWindow::buildUi(const QString &displayName)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // -- Header --
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("CallHeader"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(24, 20, 24, 16);
    headerLayout->setSpacing(4);

    auto *headerRow = new QHBoxLayout;
    _titleLabel = new QLabel(displayName, header);
    _titleLabel->setObjectName(QStringLiteral("CallTitle"));
    headerRow->addWidget(_titleLabel);
    headerRow->addStretch();
    _durationLabel = new QLabel(this);
    _durationLabel->setObjectName(QStringLiteral("CallDuration"));
    headerRow->addWidget(_durationLabel);
    headerLayout->addLayout(headerRow);

    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(6);
    _stateDot = new QLabel(header);
    _stateDot->setObjectName(QStringLiteral("CallStateDot"));
    _stateDot->setFixedSize(10, 10);
    statusRow->addWidget(_stateDot);
    _stateLabel = new QLabel(header);
    _stateLabel->setObjectName(QStringLiteral("CallStatusLabel"));
    statusRow->addWidget(_stateLabel);
    statusRow->addStretch();
    headerLayout->addLayout(statusRow);

    layout->addWidget(header);

    // -- Participant tiles area --
    _tilesArea = new QWidget(this);
    _tilesArea->setObjectName(QStringLiteral("CallTilesArea"));
    _tilesLayout = new QVBoxLayout(_tilesArea);
    _tilesLayout->setContentsMargins(20, 12, 20, 12);
    _tilesLayout->setSpacing(8);
    _tilesLayout->addStretch();

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("PanelScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(_tilesArea);
    layout->addWidget(scroll, 1);

    // -- Footer: mic | hang up | media --
    auto *footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("CallFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 12, 24, 20);
    footerLayout->setSpacing(16);

    _micButton = new QPushButton(footer);
    _micButton->setObjectName(QStringLiteral("CallToggleButton"));
    _micButton->setFixedSize(56, 56);
    _micButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("mic"), SouveraTheme::Color::TextPrimary));
    _micButton->setIconSize(QSize(24, 24));
    _micButton->setToolTip(QStringLiteral("Mikrofon"));
    _micButton->setCheckable(true);
    footerLayout->addWidget(_micButton);
    footerLayout->addStretch();

    _hangupButton = new QPushButton(footer);
    _hangupButton->setObjectName(QStringLiteral("CallHangupButton"));
    _hangupButton->setFixedSize(64, 64);
    _hangupButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("phone-off"), SouveraTheme::Color::TextPrimary));
    _hangupButton->setIconSize(QSize(28, 28));
    _hangupButton->setToolTip(QStringLiteral("Anruf beenden"));
    connect(_hangupButton, &QPushButton::clicked, this, [this]() {
        if (_state == State::InCall && _api) {
            _api->leaveCall(_token);
        }
        _callLeft = true;
        close();
    });
    footerLayout->addWidget(_hangupButton);
    footerLayout->addStretch();

    _mediaButton = new QPushButton(footer);
    _mediaButton->setObjectName(QStringLiteral("CallToggleButton"));
    _mediaButton->setFixedSize(56, 56);
    _mediaButton->setIcon(SouveraTheme::instance()->icon(QStringLiteral("volume-2"), SouveraTheme::Color::TextPrimary));
    _mediaButton->setIconSize(QSize(24, 24));
    _mediaButton->setToolTip(QStringLiteral("Ton & Bild im Browser"));
    connect(_mediaButton, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(_roomUrl);
    });
    footerLayout->addWidget(_mediaButton);
    layout->addWidget(footer);

    // -- Timers --
    _durationTimer = new QTimer(this);
    _durationTimer->setInterval(1000);
    connect(_durationTimer, &QTimer::timeout, this, [this]() {
        const auto secs = _elapsed.elapsed() / 1000;
        _durationLabel->setText(QStringLiteral("%1:%2:%3")
            .arg(secs / 3600, 2, 10, QLatin1Char('0'))
            .arg((secs / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(secs % 60, 2, 10, QLatin1Char('0')));
    });

    _pollTimer = new QTimer(this);
    _pollTimer->setInterval(5000);
    connect(_pollTimer, &QTimer::timeout, this, [this]() {
        if (_api && _state == State::InCall) _api->fetchParticipants(_token);
    });
    _pollTimer->start();
}

void CallWindow::setState(State state, const QString &error)
{
    if (_state == State::InCall && state != State::InCall) {
        // Transitioning away from an active call: stop the timer.
        _durationTimer->stop();
    }
    _state = state;
    const auto *theme = SouveraTheme::instance();

    switch (state) {
    case State::Connecting:
        _stateDot->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 5px;")
            .arg(theme->color(SouveraTheme::Color::Warning).name()));
        _stateLabel->setText(QStringLiteral("Verbinde\u2026"));
        break;
    case State::InCall:
        stopRingTone();
        _elapsed.start();
        _durationTimer->start();
        _stateDot->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 5px;")
            .arg(theme->color(SouveraTheme::Color::Success).name()));
        _stateLabel->setText(QStringLiteral("Verbunden"));
#ifdef HAVE_GSTREAMER
        // Start the media engine (audio + video over HPB signaling).
        if (!_mediaEngine) {
            _mediaEngine = new TalkMediaEngine(_signaling, this);
            connect(_mediaEngine, &TalkMediaEngine::mediaConnected, this, [this]() {
                _stateLabel->setText(QStringLiteral("Verbunden \u2014 Medien aktiv"));
            });
            connect(_mediaEngine, &TalkMediaEngine::errorOccurred, this,
                    [this](const QString &msg) {
                qCWarning(lcCallWindow) << "Media engine error:" << msg;
            });
            _mediaEngine->start(_token, _roomSessionId);
        }
#endif
        break;
    case State::Ended:
        stopRingTone();
        _durationTimer->stop();
        _stateDot->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 5px;")
            .arg(theme->color(SouveraTheme::Color::Danger).name()));
        _stateLabel->setText(error.isEmpty()
            ? QStringLiteral("Anruf beendet") : error);
        break;
    }
}

void CallWindow::updateTileRendering()
{
    // Clear existing tiles (keep the trailing stretch).
    while (_tilesLayout->count() > 1) {
        auto *item = _tilesLayout->takeAt(0);
        if (auto *w = item->widget()) w->deleteLater();
        delete item;
    }

    const auto *theme = SouveraTheme::instance();
    const auto inCallColor = theme->color(SouveraTheme::Color::Success).name();
    const auto surfaceColor = theme->color(SouveraTheme::Color::Surface).name();
    const auto borderColor = theme->color(SouveraTheme::Color::Border).name();

    for (auto it = _names.cbegin(); it != _names.cend(); ++it) {
        const auto inCall = _inCallFlags.value(it.key()) != 0;
        const auto name = it.value();
        const auto initial = name.left(1).toUpper();

        auto *tile = new QFrame(_tilesArea);
        tile->setObjectName(QStringLiteral("CallTile"));
        tile->setFixedHeight(64);
        auto *tileLayout = new QHBoxLayout(tile);
        tileLayout->setContentsMargins(12, 8, 12, 8);
        tileLayout->setSpacing(12);

        // Avatar circle with initials
        auto *avatar = new QLabel(initial, tile);
        avatar->setObjectName(QStringLiteral("CallAvatar"));
        avatar->setFixedSize(40, 40);
        avatar->setAlignment(Qt::AlignCenter);
        tileLayout->addWidget(avatar);

        auto *nameLabel = new QLabel(name, tile);
        nameLabel->setObjectName(QStringLiteral("CallTileName"));
        tileLayout->addWidget(nameLabel, 1);

        // In-call indicator dot
        auto *dot = new QLabel(tile);
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 5px;").arg(
            inCall ? inCallColor : QStringLiteral("transparent")));
        dot->setToolTip(inCall ? QStringLiteral("Im Gespr\u00E4ch") : QString());
        tileLayout->addWidget(dot);

        tile->setStyleSheet(QStringLiteral(
            "QFrame#CallTile { background: %1; border: 1px solid %2;"
            " border-radius: 10px; }"
            "QLabel#CallAvatar { background: %3; color: %4; border-radius: 20px;"
            " font-weight: 600; font-size: 16px; }"
            "QLabel#CallTileName { color: %4; font-size: 13px; font-weight: 500; }")
            .arg(surfaceColor, borderColor,
                 theme->color(SouveraTheme::Color::SurfaceHover).name(),
                 theme->color(SouveraTheme::Color::TextPrimary).name()));

        _tilesLayout->insertWidget(_tilesLayout->count() - 1, tile);
    }
}

void CallWindow::startRingTone()
{
    if (_ringSink) return;
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    _ringSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    _ringIo = _ringSink->start();
    if (!_ringIo) {
        qCWarning(lcCallWindow) << "Ring tone failed to start";
        delete _ringSink;
        _ringSink = nullptr;
        return;
    }
    _ringIo->write(makeRingCycle());
    // Keep the buffer topped up so the ring cycle loops seamlessly.
    _ringTimer = new QTimer(this);
    _ringTimer->setInterval(RingCycleMs);
    connect(_ringTimer, &QTimer::timeout, this, [this]() {
        if (_ringIo) _ringIo->write(makeRingCycle());
    });
    _ringTimer->start();
}

void CallWindow::stopRingTone()
{
    if (_ringTimer) {
        _ringTimer->stop();
        delete _ringTimer;
        _ringTimer = nullptr;
    }
    if (_ringIo) {
        _ringIo->close();
        _ringIo = nullptr;
    }
    if (_ringSink) {
        _ringSink->stop();
        delete _ringSink;
        _ringSink = nullptr;
    }
}

void CallWindow::closeEvent(QCloseEvent *event)
{
    if (!_callLeft && _api && _state != State::Ended) {
        _api->leaveCall(_token);
        _callLeft = true;
    }
    if (_signaling) {
        _signaling->leaveRoom();
    }
#ifdef HAVE_GSTREAMER
    if (_mediaEngine) {
        _mediaEngine->stop();
    }
#endif
    stopRingTone();
    QWidget::closeEvent(event);
}

} // namespace OCC
