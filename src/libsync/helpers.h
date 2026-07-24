/*
 * SPDX-FileCopyrightText: 2022 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "owncloudlib.h"
#include <QByteArray>

namespace OCC
{
/** Strips quotes and gzip annotations */
OWNCLOUDSYNC_EXPORT QByteArray parseEtag(const char *header);

} // namespace OCC