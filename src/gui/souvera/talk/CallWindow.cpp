/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CallWindow.h"
#include "TalkOcsApi.h"
#include "TalkSignalingClient.h"

#include "theme/SouveraTheme.h"

#include <QCloseEvent>
#include <QColor>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace OCC {

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
    resize(420, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("\U0001F4DE %1").arg(displayName), this);
    title->setObjectName(QStringLiteral("PanelTitle"));
    layout->addWidget(title);

    _durationLabel = new QLabel(QStringLiteral("00:00"), this);
    _durationLabel->setObjectName(QStringLiteral("CallDuration"));
    layout->addWidget(_durationLabel);

    _stateLabel = new QLabel(QStringLiteral("Verbunden \u2014 du nimmst an diesem Anruf teil."), this);
    _stateLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    _stateLabel->setWordWrap(true);
    layout->addWidget(_stateLabel);

    auto *participantsTitle = new QLabel(QStringLiteral("Teilnehmer"), this);
    participantsTitle->setObjectName(QStringLiteral("SettingsCardTitle"));
    layout->addWidget(participantsTitle);

    _participants = new QListWidget(this);
    _participants->setObjectName(QStringLiteral("CallParticipants"));
    layout->addWidget(_participants, 1);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);

    auto *leaveBtn = new QPushButton(QStringLiteral("Anruf beenden"), this);
    leaveBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
    connect(leaveBtn, &QPushButton::clicked, this, [this]() {
        if (_inCall && _api) {
            _api->leaveCall(_token);
        }
        close();
    });
    buttons->addWidget(leaveBtn);
    buttons->addStretch();

    auto *mediaBtn = new QPushButton(QStringLiteral("Ton & Bild im Browser"), this);
    mediaBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    mediaBtn->setToolTip(QStringLiteral("\u00DCbergang bis zur nativen Medien-Engine"));
    connect(mediaBtn, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(_roomUrl);
    });
    buttons->addWidget(mediaBtn);
    layout->addLayout(buttons);

    _durationTimer = new QTimer(this);
    _elapsed.start();
    connect(_durationTimer, &QTimer::timeout, this, [this]() {
        const auto secs = _elapsed.elapsed() / 1000;
        _durationLabel->setText(QStringLiteral("%1:%2:%3")
            .arg(secs / 3600, 2, 10, QLatin1Char('0'))
            .arg((secs / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(secs % 60, 2, 10, QLatin1Char('0')));
    });
    _durationTimer->start(1000);

    _pollTimer = new QTimer(this);
    _pollTimer->setInterval(5000);
    connect(_pollTimer, &QTimer::timeout, this, [this]() {
        if (_api) _api->fetchParticipants(_token);
    });
    _pollTimer->start();

    if (_api) {
        connect(_api, &TalkOcsApi::participantsReceived, this,
                [this](const QString &token, const QVector<TalkParticipant> &participants) {
            if (token != _token) return;
            _names.clear();
            for (const auto &p : participants) {
                if (!p.actorId.isEmpty()) {
                    _names.insert(p.actorId, p.displayName);
                }
            }
            // The REST names and the signaling in-call flags are merged in
            // the rendering pass driven by the signaling updates.
            const auto *theme = SouveraTheme::instance();
            const auto inCallColor = theme->color(SouveraTheme::Color::Success).name();
            const auto idleColor = theme->color(SouveraTheme::Color::TextMuted).name();
            _participants->clear();
            for (auto it = _names.cbegin(); it != _names.cend(); ++it) {
                const auto inCall = _inCallFlags.value(it.key()) != 0;
                auto *item = new QListWidgetItem(it.value(), _participants);
                item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
                item->setForeground(QColor(inCall ? inCallColor : idleColor));
            }
        });
        connect(_api, &TalkOcsApi::callLeft, this, [this](const QString &token) {
            if (token == _token) {
                _inCall = false;
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
            const auto *theme = SouveraTheme::instance();
            const auto inCallColor = theme->color(SouveraTheme::Color::Success).name();
            const auto idleColor = theme->color(SouveraTheme::Color::TextMuted).name();
            _participants->clear();
            for (auto it = _names.cbegin(); it != _names.cend(); ++it) {
                const auto inCall = _inCallFlags.value(it.key()) != 0;
                auto *item = new QListWidgetItem(it.value(), _participants);
                item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
                item->setForeground(QColor(inCall ? inCallColor : idleColor));
            }
        });
    }
}

CallWindow::~CallWindow() = default;

void CallWindow::closeEvent(QCloseEvent *event)
{
    if (_inCall && _api) {
        // Never leave the server-side call state dangling behind the window.
        _api->leaveCall(_token);
        _inCall = false;
    }
    if (_signaling) {
        _signaling->leaveRoom();
    }
    QWidget::closeEvent(event);
}

} // namespace OCC
