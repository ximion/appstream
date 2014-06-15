/* Copyright (c) 2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

import QtQuick 2.2
import QtQuick.Controls 1.0


Rectangle {
    visible: true
    width: parent.width
    height: 80
    border.color: "steelblue"
    border.width: 2
    radius: 4
    color: "transparent"
    clip: true
    id: panel

    property string name: "???"
    property string summary: "???"
    property url iconUrl: ""

    Row {
        id: panelRow
        anchors.fill: parent
        anchors.margins: 10

        Image {
            id: icon
            //anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 8
            width: 64
            height: 64
            source: panel.iconUrl
        }

        Column {
            spacing: 4
            anchors.margins: 10

            Label {
                id: nameLabel
                text: panel.name
                anchors.leftMargin: 8
                font.bold: true
                font.pixelSize: 14
            }

            Label {
                id: summaryLabel
                text: panel.summary
            }

        }
    }

        Button {
            id: detailsButton
            text: qsTr("Details")
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.margins: 12
            height: 20
        }

}
