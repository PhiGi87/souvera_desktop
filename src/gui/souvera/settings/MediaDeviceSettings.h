/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MEDIADEVICESETTINGS_H
#define MEDIADEVICESETTINGS_H

#include <QAudioDevice>
#include <QString>

namespace OCC {

/**
 * @brief Central access to the audio/video devices the user picked in the
 *        Audio & Video settings page.
 *
 * All consumers (call window ring tones, incoming call dialog, media
 * engine sources) must go through these helpers — they own the
 * "souvera/call" settings group. Reading the raw keys without the group
 * silently yields the system default instead of the chosen device.
 */
namespace MediaDeviceSettings {

/** The microphone the user selected, or the system default. */
QAudioDevice inputDevice();

/** The speaker the user selected, or the system default. */
QAudioDevice outputDevice();

/** Display name of the selected microphone ("Systemstandard" when unset). */
QString inputDescription();

/** Display name of the selected camera ("Systemstandard" when unset). */
QString cameraDescription();

} // namespace MediaDeviceSettings

} // namespace OCC

#endif // MEDIADEVICESETTINGS_H
