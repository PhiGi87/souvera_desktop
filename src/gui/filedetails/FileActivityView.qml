/*
 * SPDX-FileCopyrightText: 2022 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "../tray"

ActivityList {
    id: root

    property alias localPath: activityListModel.localPath
    property alias accountState: activityListModel.accountState

    isFileActivityList: true
    model: FileActivityListModel {
        id: activityListModel
    }
}
