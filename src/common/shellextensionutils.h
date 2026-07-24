/*
 * SPDX-FileCopyrightText: 2022 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "config.h"
#include <QByteArray>
#include <QString>
#include <QVariantMap>

namespace VfsShellExtensions {
QString serverNameForApplicationName(const QString &applicationName);
QString serverNameForApplicationNameDefault();

namespace Protocol {
    static constexpr auto CustomStateProviderRequestKey = "customStateProviderRequest";
    static constexpr auto CustomStateDataKey = "customStateData";
    static constexpr auto CustomStateStatesKey = "states";
    static constexpr auto FilePathKey = "filePath";
    static constexpr auto ThumbnailProviderRequestKey = "thumbnailProviderRequest";
    static constexpr auto ThumbnailProviderRequestFileSizeKey = "fileSize";
    static constexpr auto ThumnailProviderDataKey = "thumbnailData";
    static constexpr auto Version = "2.0";

    QByteArray createJsonMessage(const QVariantMap &message);
    bool validateProtocolVersion(const QVariantMap &message);
}
}
