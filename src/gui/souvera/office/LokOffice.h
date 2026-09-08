/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LOKOFFICE_H
#define LOKOFFICE_H

#include <QString>

struct LibreOfficeKit;

namespace OCC {

/**
 * @brief Loads the bundled LibreOffice core via LibreOfficeKit.
 *
 * Only available on Windows and Linux; LibreOffice does not support the
 * LibreOfficeKit embedding API on macOS (see LibreOfficeKitInit.h).
 * The engine is looked up next to the application binary under
 * "libreoffice/program". The SOUVERA_LO_PATH environment variable can be
 * used to point to an existing LibreOffice installation for development.
 */
class LokOffice
{
public:
    [[nodiscard]] static bool isSupported();
    [[nodiscard]] static QString installPath();

    /**
     * Initializes the office instance (may take a few seconds on first call)
     * and returns the LibreOfficeKit handle, or nullptr on failure.
     */
    [[nodiscard]] static LibreOfficeKit *instance();

    static void shutdown();
};

} // namespace OCC

#endif // LOKOFFICE_H
