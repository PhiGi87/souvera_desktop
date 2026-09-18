/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MediaDeviceSettings.h"

#include <QMediaDevices>
#include <QSettings>

namespace OCC {

namespace {
QString settingsGroup()
{
    return QStringLiteral("souvera/call");
}
} // namespace

namespace MediaDeviceSettings {

QAudioDevice inputDevice()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const auto stored = settings.value(QStringLiteral("audioInputId")).toString();
    settings.endGroup();
    if (!stored.isEmpty()) {
        const auto inputs = QMediaDevices::audioInputs();
        for (const auto &device : inputs) {
            if (device.id() == stored.toUtf8()) return device;
        }
    }
    return QMediaDevices::defaultAudioInput();
}

QAudioDevice outputDevice()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const auto stored = settings.value(QStringLiteral("audioOutputId")).toString();
    settings.endGroup();
    if (!stored.isEmpty()) {
        const auto outputs = QMediaDevices::audioOutputs();
        for (const auto &device : outputs) {
            if (device.id() == stored.toUtf8()) return device;
        }
    }
    return QMediaDevices::defaultAudioOutput();
}

QString inputDescription()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const auto value = settings.value(QStringLiteral("audioInputDescription")).toString();
    settings.endGroup();
    return value;
}

QString cameraDescription()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const auto value = settings.value(QStringLiteral("cameraDescription")).toString();
    settings.endGroup();
    return value;
}

} // namespace MediaDeviceSettings

} // namespace OCC
