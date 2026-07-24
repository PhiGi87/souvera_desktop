/*
 * SPDX-FileCopyrightText: 2022 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls

Label {
    function resetToPlainText() {
        if (textFormat !== Text.PlainText) {
            console.log("WARNING: this label was set to a non-plain text format. Resetting to plain text.")
            textFormat = Text.PlainText;
        }
    }

    textFormat: Text.PlainText
    onTextFormatChanged: resetToPlainText()
    Component.onCompleted: resetToPlainText()
}
