/*
 * SPDX-FileCopyrightText: 2018 Souvera (Host-On Service Provider GmbH)
 * SPDX-FileCopyrightText: 2011 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtGlobal>

#include <cmath>
#include <csignal>

#ifdef Q_OS_UNIX
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "application.h"
#include "cocoainitializer.h"
#include "theme.h"
#include "common/utility.h"

#if defined(BUILD_UPDATER)
#include "updater/updater.h"
#endif

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <QQuickStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QOperatingSystemVersion>

using namespace OCC;

// ---------------------------------------------------------------------------
// Persistent startup debug log.
//
// Installs a Qt message handler that writes EVERY log line (qDebug, qInfo,
// qWarning, qCritical) to a file with immediate flush. This captures the
// exact point of a crash — the last line in the file is the last thing
// the app did before dying.
//
// Log location:
//   Windows: %LOCALAPPDATA%\Souvera Workspace\startup.log
//   Linux:   ~/.local/share/Souvera Workspace/startup.log
//   macOS:   ~/Library/Logs/Souvera Workspace/startup.log
// ---------------------------------------------------------------------------

namespace {

QFile g_logFile;
QTextStream g_logStream;
QMutex g_logMutex;

void startupMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&g_logMutex);

    const auto timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const auto category = context.category ? QString::fromLatin1(context.category) : QStringLiteral("default");
    const auto file = context.file ? QString::fromLatin1(context.file) : QString();
    const auto line = context.line > 0 ? QStringLiteral(":%1").arg(context.line) : QString();

    QString level;
    switch (type) {
    case QtDebugMsg:    level = QStringLiteral("DEBUG"); break;
    case QtInfoMsg:     level = QStringLiteral("INFO "); break;
    case QtWarningMsg:  level = QStringLiteral("WARN "); break;
    case QtCriticalMsg: level = QStringLiteral("CRIT "); break;
    case QtFatalMsg:    level = QStringLiteral("FATAL"); break;
    }

    if (g_logFile.isOpen()) {
        g_logStream << timestamp << " [" << level << "] [" << category << "] "
                    << msg
                    << "  (" << file << line << ")"
                    << Qt::endl;
        g_logStream.flush();
    }

    fprintf(stderr, "%s [%s] [%s] %s\n",
            qPrintable(timestamp), qPrintable(level), qPrintable(category), qPrintable(msg));
    fflush(stderr);

    if (type == QtFatalMsg) {
        if (g_logFile.isOpen()) {
            g_logStream.flush();
            g_logFile.flush();
        }
        abort();
    }
}

QString startupLogPath()
{
    // Fixed path that does NOT depend on QApplication being created.
    // %LOCALAPPDATA%\Souvera\startup.log on Windows,
    // ~/.local/share/Souvera/startup.log on Linux,
    // ~/Library/Application Support/Souvera/startup.log on macOS.
#ifdef Q_OS_WIN
    const auto base = qEnvironmentVariable("LOCALAPPDATA");
    if (!base.isEmpty()) return base + QStringLiteral("/Souvera/startup.log");
#endif
    const auto home = QDir::homePath();
    return home + QStringLiteral("/.local/share/Souvera/startup.log");
}

void installStartupLogger()
{
    const auto logPath = startupLogPath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());

    // Truncate: each session gets a fresh log so the last entry is always
    // the last action before a crash.
    g_logFile.setFileName(logPath);
    if (g_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        g_logStream.setDevice(&g_logFile);
        g_logStream << "=== Souvera Workspace started: "
                    << QDateTime::currentDateTime().toString(Qt::ISODate)
                    << " ===" << Qt::endl;
        g_logStream.flush();
        qInstallMessageHandler(startupMessageHandler);
        qDebug() << "Startup logger installed:" << logPath;
    } else {
        fprintf(stderr, "FATAL: Could not open startup log: %s\n", qPrintable(logPath));
    }
}

} // namespace

void warnSystray()
{
    QMessageBox::critical(
        nullptr,
        qApp->translate("main.cpp", "System Tray not available"),
        qApp->translate("main.cpp", "%1 requires on a working system tray. "
                                    "If you are running XFCE, please follow "
                                    "<a href=\"http://docs.xfce.org/xfce/xfce4-panel/systray\">these instructions</a>. "
                                    "Otherwise, please install a system tray application such as \"trayer\" and try again.")
            .arg(Theme::instance()->appNameGUI()),
        QMessageBox::Ok
    );
}

int main(int argc, char **argv)
{
    // Persistent debug log — must be installed before ANY other code runs
    // so we capture the initialization sequence and any crash point.
    installStartupLogger();
    qDebug() << "=== main() entered ===";

#ifdef Q_OS_WIN
    SetDllDirectory(L"");
    qputenv("QML_IMPORT_PATH", (QDir::currentPath() + QStringLiteral("/qml")).toLatin1());
#endif

    Q_INIT_RESOURCE(resources);
    qDebug() << "Q_INIT_RESOURCE(resources) done";
    Q_INIT_RESOURCE(theme);
    qDebug() << "Q_INIT_RESOURCE(theme) done";
    // Souvera Workspace resources (stylesheet + sidebar icons) live in a
    // static library; without explicit initialisation the linker drops them
    // and the app would run with a completely unstyled UI.
    Q_INIT_RESOURCE(souvera);
    qDebug() << "Q_INIT_RESOURCE(souvera) done";

    // OpenSSL 1.1.0: No explicit initialisation or de-initialisation is necessary.
#ifdef Q_OS_MACOS
    Mac::CocoaInitializer cocoaInit; // RIIA
#endif

    auto surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setOption(QSurfaceFormat::ResetNotification);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    auto qmlStyle = QStringLiteral("Fusion");
    auto widgetsStyle = QStringLiteral("");

#if defined Q_OS_MACOS
    qmlStyle = QStringLiteral("macOS");
#elif defined Q_OS_WIN
    if (const auto osVersion = QOperatingSystemVersion::current().version(); osVersion < QOperatingSystemVersion::Windows11.version()) {
        qmlStyle = QStringLiteral("Universal");
        widgetsStyle = QStringLiteral("Fusion");
        if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_UNIVERSAL_THEME")) {
            // initialise theme with the light/dark mode setting from the OS
            qputenv("QT_QUICK_CONTROLS_UNIVERSAL_THEME", "System");
        }

        if (osVersion < QOperatingSystemVersion::Windows10_1809.version() && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            // for Windows Server 2016 to display text as expected, see #8064
            qputenv("QT_QPA_PLATFORM", "windows:nodirectwrite");
        }
    } else {
        qmlStyle = QStringLiteral("FluentWinUI3");
        widgetsStyle = QStringLiteral("windows11");
    }
#endif

    QQuickStyle::setStyle(qmlStyle);

#if defined KF6DBusAddons_FOUND && KF6DBusAddons_FOUND
    QCoreApplication::setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN_DBUS));
    QCoreApplication::setApplicationName(QLatin1String(APPLICATION_EXECUTABLE));
#endif

    OCC::Application app(argc, argv);

    if (!widgetsStyle.isEmpty()) {
        QApplication::setStyle(QStyleFactory::create(widgetsStyle));
    }

#ifndef Q_OS_WIN
    signal(SIGPIPE, SIG_IGN);
#endif
    if (app.giveHelp()) {
        app.showHelp();
        return 0;
    }
    if (app.versionOnly()) {
        app.showVersion();
        return 0;
    }

// check a environment variable for core dumps
#ifdef Q_OS_UNIX
    if (!qEnvironmentVariableIsEmpty("OWNCLOUD_CORE_DUMP")) {
        struct rlimit core_limit{};
        core_limit.rlim_cur = RLIM_INFINITY;
        core_limit.rlim_max = RLIM_INFINITY;

        if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
            fprintf(stderr, "Unable to set core dump limit\n");
        } else {
            qCInfo(lcApplication) << "Core dumps enabled";
        }
    }
#endif

#if defined(BUILD_UPDATER)
    // if handleStartup returns true, main()
    // needs to terminate here, e.g. because
    // the updater is triggered
    Updater *updater = Updater::instance();
    if (updater && updater->handleStartup()) {
        return 1;
    }
#endif

    // if the application is already running, notify it.
    if (app.isRunning()) {
        qCInfo(lcApplication) << "Already running, exiting...";
        if (app.isSessionRestored()) {
            // This call is mirrored with the one in Application::slotParseMessage
            qCInfo(lcApplication) << "Session was restored, don't notify app!";
            return -1;
        }

        QStringList args = app.arguments();
        if (args.size() > 1) {
            QString msg = args.join(QLatin1String("|"));
            if (!app.sendMessage(QLatin1String("MSG_PARSEOPTIONS:") + msg))
                return -1;
        } else if (!app.backgroundMode() && !app.sendMessage(QLatin1String("MSG_SHOWMAINDIALOG"))) {
            return -1;
        }
        return 0;
    }

    // We can't call isSystemTrayAvailable with appmenu-qt5 begause it hides the systemtray
    // (issue #4693)
    if (qgetenv("QT_QPA_PLATFORMTHEME") != "appmenu-qt5")
    {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            // If the systemtray is not there, we will wait one second for it to maybe start
            // (eg boot time) then we show the settings dialog if there is still no systemtray.
            // On XFCE however, we show a message box with explainaition how to install a systemtray.
            qCInfo(lcApplication) << "System tray is not available, waiting...";
            Utility::sleep(1);

            auto desktopSession = qgetenv("XDG_CURRENT_DESKTOP").toLower();
            if (desktopSession.isEmpty()) {
                desktopSession = qgetenv("DESKTOP_SESSION").toLower();
            }
            if (desktopSession == "xfce") {
                int attempts = 0;
                while (!QSystemTrayIcon::isSystemTrayAvailable()) {
                    attempts++;
                    if (attempts >= 30) {
                        qCWarning(lcApplication) << "System tray unavailable (xfce)";
                        warnSystray();
                        break;
                    }
                    Utility::sleep(1);
                }
            }

            if (QSystemTrayIcon::isSystemTrayAvailable()) {
                app.tryTrayAgain();
            } else if (!app.backgroundMode() && !AccountManager::instance()->accounts().isEmpty()) {
                if (desktopSession != "ubuntu") {
                    qCInfo(lcApplication) << "System tray still not available, showing window and trying again later";
                    app.showMainDialog();
                    QTimer::singleShot(10000, &app, &Application::tryTrayAgain);
                } else {
                    qCInfo(lcApplication) << "System tray still not available, but assuming it's fine on 'ubuntu' desktop";
                }
            }
        }
    }

    return app.exec();
}
