/*
 * SPDX-FileCopyrightText: 2023 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import Style

Image {
    id: root

    property alias icon: icon

    cache: true
    mipmap: true
    fillMode: Image.PreserveAspectFit

    Image {
        id: icon

        anchors.centerIn: parent

        cache: true
        mipmap: true
        fillMode: Image.PreserveAspectFit
        visible: source !== ""
    }
}
