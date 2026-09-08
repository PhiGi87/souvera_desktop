/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "LokOffice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
#include "lok/LibreOfficeKitInit.h"
#endif

Q_LOGGING_CATEGORY(lcLokOffice, "souvera.office.lok")

namespace OCC {

namespace {
LibreOfficeKit *g_office = nullptr;
bool g_initTried = false;
}

bool LokOffice::isSupported()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

QString LokOffice::installPath()
{
    const auto envPath = qEnvironmentVariable("SOUVERA_LO_PATH");
    if (!envPath.isEmpty()) {
        return envPath;
    }

    const auto appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/libreoffice/program"),
        appDir + QStringLiteral("/../libreoffice/program"),
        appDir + QStringLiteral("/../../libreoffice/program"),
        appDir + QStringLiteral("/../../libreoffice/Contents/program"),
#if defined(Q_OS_LINUX)
        QStringLiteral("/usr/lib/libreoffice/program"),
        QStringLiteral("/usr/lib64/libreoffice/program"),
#endif
    };

    for (const auto &candidate : candidates) {
        const auto normalized = QDir::cleanPath(candidate);
        if (QFileInfo::exists(normalized)) {
            return normalized;
        }
    }
    return QString();
}

LibreOfficeKit *LokOffice::instance()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (g_initTried) {
        return g_office;
    }
    g_initTried = true;

    const auto path = installPath();
    if (path.isEmpty()) {
        qCWarning(lcLokOffice) << "Bundled LibreOffice not found";
        return nullptr;
    }

    const auto profileDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/office-profile");
    QDir().mkpath(profileDir);
    const auto profileUrl = QUrl::fromLocalFile(profileDir).toString();

    qCInfo(lcLokOffice) << "Initializing LibreOfficeKit from" << path;
    g_office = lok_init_2(path.toUtf8().constData(), profileUrl.toUtf8().constData());
    if (!g_office) {
        qCWarning(lcLokOffice) << "lok_init_2 failed for" << path;
    }
    return g_office;
#else
    return nullptr;
#endif
}

void LokOffice::shutdown()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (g_office) {
        g_office->pClass->destroy(g_office);
        g_office = nullptr;
    }
    g_initTried = false;
#endif
}

} // namespace OCC
