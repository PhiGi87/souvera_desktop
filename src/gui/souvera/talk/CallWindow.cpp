/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CallWindow.h"

#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"
#include "config.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

#ifdef BUILD_WITH_WEBENGINE
#include <QAuthenticator>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#endif

namespace OCC {

CallWindow::CallWindow(AccountState *accountState, const QUrl &roomUrl,
                       const QString &roomName, QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("Anruf \u2014 %1").arg(roomName));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1100, 720);

    setupUi(accountState, roomUrl, roomName);
}

void CallWindow::setupUi(AccountState *accountState, const QUrl &roomUrl, const QString &roomName)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("PanelToolbar"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 8, 16, 8);

    auto *titleLabel = new QLabel(QStringLiteral("\U0001F4DE %1").arg(roomName), header);
    titleLabel->setObjectName(QStringLiteral("PanelTitle"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto *browserBtn = new QPushButton(QStringLiteral("Im Browser \u00F6ffnen"), header);
    browserBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(browserBtn, &QPushButton::clicked, this, [roomUrl]() {
        QDesktopServices::openUrl(roomUrl);
    });
    headerLayout->addWidget(browserBtn);

    layout->addWidget(header);

#ifdef BUILD_WITH_WEBENGINE
    auto *view = new QWebEngineView(this);
    auto *page = new QWebEnginePage(QWebEngineProfile::defaultProfile(), view);

    // Answer HTTP basic auth challenges with the account credentials so the
    // Talk web app opens with the user's session instead of a login prompt.
    connect(page, &QWebEnginePage::authenticationRequired, this,
            [accountState](const QUrl &, QAuthenticator *authenticator) {
        if (!accountState || !accountState->account() || !authenticator) return;
        const auto creds = accountState->account()->credentials();
        if (!creds) return;
        authenticator->setUser(creds->user());
        authenticator->setPassword(creds->password());
    });

    // Video calls need microphone and camera; desktop sharing is requested by Talk.
    connect(page, &QWebEnginePage::featurePermissionRequested, this,
            [page](const QUrl &origin, QWebEnginePage::Feature feature) {
        switch (feature) {
        case QWebEnginePage::MediaAudioCapture:
        case QWebEnginePage::MediaVideoCapture:
        case QWebEnginePage::MediaAudioVideoCapture:
        case QWebEnginePage::DesktopVideoCapture:
        case QWebEnginePage::DesktopAudioVideoCapture:
        case QWebEnginePage::Notifications:
            page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionGrantedByUser);
            break;
        default:
            page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionDeniedByUser);
            break;
        }
    });

    view->setPage(page);
    view->load(roomUrl);
    _view = view;
    layout->addWidget(view, 1);
#else
    auto *fallback = new QWidget(this);
    auto *fallbackLayout = new QVBoxLayout(fallback);
    auto *hint = new QLabel(QStringLiteral(
        "Der eingebettete Anruf ist in diesem Build nicht verf\u00FCgbar.\n"
        "Der Raum wurde im Browser ge\u00F6ffnet \u2014 dieses Fenster kann geschlossen werden."),
        fallback);
    hint->setObjectName(QStringLiteral("PanelPlaceholder"));
    hint->setAlignment(Qt::AlignCenter);
    fallbackLayout->addWidget(hint);
    _view = fallback;
    layout->addWidget(fallback, 1);

    QDesktopServices::openUrl(roomUrl);
#endif
}

} // namespace OCC
