import QtQuick
import QtQuick.Controls
import Qt.labs.platform as Platform
import Recycle_Vending_Machine_LCD

/*
 * AdminAboutPage — version info + manual update check.
 *
 *   ┌─────────────────────────────────────────────┐
 *   │ [←]   About ReWinGo                         │
 *   ├─────────────────────────────────────────────┤
 *   │                                             │
 *   │     🔁  ReWinGo Kiosk                       │
 *   │     v 0.1.0                                 │
 *   │     Build: 2026-05-25 14:32 UTC             │
 *   │     Kiosk ID: abc12345-…                    │
 *   │                                             │
 *   ├─────────────────────────────────────────────┤
 *   │  [Check for updates]   last: 14:00 UTC      │
 *   │                                             │
 *   │  ╔═══════════════════════════════════════╗  │
 *   │  ║  Update available: v0.2.0             ║  │
 *   │  ║  – fix dispense stall on slot 5       ║  │
 *   │  ║  – speed up boot scan                 ║  │
 *   │  ║  [Open release page]                  ║  │
 *   │  ╚═══════════════════════════════════════╝  │
 *   ├─────────────────────────────────────────────┤
 *   │  Dependencies                               │
 *   │   • Qt 6.x                                  │
 *   │   • OpenCV 4.x                              │
 *   │   • Open Food Facts (catalog lookup)        │
 *   │   • MongoDB Atlas (cloud sync)              │
 *   │                                             │
 *   │  License: MIT — see LICENSE file            │
 *   │  © 2026 ReWinGo Contributors                │
 *   └─────────────────────────────────────────────┘
 */
Rectangle {
    id: page
    objectName: "adminAboutPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view

    Component.onCompleted: Idle.disable()
    StackView.onActivated: Idle.disable()

    // ============ Top bar ============
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 110
        color: "#1F2A1B"

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            width: 70; height: 70; radius: 35
            color: "#FFFFFF"
            TapHandler { onTapped: stackView.pop() }
            Text { anchors.centerIn: parent; text: "←"
                   font.pixelSize: 32; color: "#1F2A1B" }
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("About ReWinGo")
            color: "#FFFFFF"
            font.pixelSize: 36; font.weight: Font.Black
        }
    }

    // ============ Identity card ============
    Rectangle {
        id: identityCard
        anchors.top: topBar.bottom
        anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.94
        height: 240
        radius: 22
        color: "#FFFFFF"
        border.width: 2; border.color: "#D8E0CF"

        Column {
            anchors.centerIn: parent
            spacing: 12

            Text {
                text: "♻"
                color: "#16A34A"
                font.pixelSize: 80
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("ReWinGo Kiosk")
                color: "#1F2A1B"
                font.pixelSize: 32; font.weight: Font.Black
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: "v " + UpdateInfo.currentVersion
                color: "#0891B2"
                font.pixelSize: 22; font.weight: Font.ExtraBold
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("Kiosk ID: ") + (appManager.kioskId || "?")
                color: "#5A6B52"
                font.pixelSize: 13
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ============ Update card ============
    Rectangle {
        id: updateCard
        anchors.top: identityCard.bottom
        anchors.topMargin: 18
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.94
        height: UpdateInfo.updateAvailable ? 280 : 130
        Behavior on height { NumberAnimation { duration: 160 } }
        radius: 22
        color: UpdateInfo.updateAvailable ? "#FEF3C7" : "#FFFFFF"
        border.width: 2
        border.color: UpdateInfo.updateAvailable ? "#FBBF24" : "#D8E0CF"

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Row {
                width: parent.width
                spacing: 14

                Column {
                    width: parent.width - 240
                    spacing: 2
                    Text {
                        text: UpdateInfo.updateAvailable
                              ? qsTr("Update available: v") + UpdateInfo.latestVersion
                              : qsTr("Up to date")
                        color: UpdateInfo.updateAvailable ? "#92400E" : "#1F2A1B"
                        font.pixelSize: 22; font.weight: Font.ExtraBold
                    }
                    Text {
                        text: UpdateInfo.lastCheckedAt.toString().length > 0
                              ? qsTr("Last checked: ") +
                                Qt.formatDateTime(UpdateInfo.lastCheckedAt,
                                                  "yyyy-MM-dd HH:mm")
                              : qsTr("Not checked yet")
                        color: "#5A6B52"
                        font.pixelSize: 13
                    }
                }

                Rectangle {
                    width: 220; height: 56; radius: 28
                    color: UpdateInfo.busy ? "#9CA3AF" : "#0891B2"
                    anchors.verticalCenter: parent.verticalCenter
                    TapHandler {
                        enabled: !UpdateInfo.busy
                        onTapped: UpdateInfo.checkNow()
                    }
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        BusyIndicator {
                            visible: UpdateInfo.busy
                            running: UpdateInfo.busy
                            width: 24; height: 24
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: UpdateInfo.busy
                                  ? qsTr("Checking…")
                                  : qsTr("Check for updates")
                            color: "#FFFFFF"
                            font.pixelSize: 16; font.weight: Font.ExtraBold
                        }
                    }
                }
            }

            // ── Release notes (only when an update is available) ──────
            Rectangle {
                visible: UpdateInfo.updateAvailable
                width: parent.width
                height: parent.height - 100
                radius: 14
                color: "#FFFFFF"
                border.width: 1; border.color: "#FBBF24"

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true
                    Text {
                        width: updateCard.width - 60
                        text: UpdateInfo.releaseNotes.length > 0
                              ? UpdateInfo.releaseNotes
                              : qsTr("(no release notes)")
                        color: "#1F2A1B"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    // ============ Footer (deps + license) ============
    Rectangle {
        anchors.top: updateCard.bottom
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.94
        radius: 18
        color: "#FFFFFF"
        border.width: 2; border.color: "#D8E0CF"

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 8

            Text { text: qsTr("Built with")
                   color: "#1F2A1B"
                   font.pixelSize: 16; font.weight: Font.ExtraBold }

            Text { text: "•  Qt 6 (LGPL)"
                   color: "#5A6B52"; font.pixelSize: 14 }
            Text { text: "•  OpenCV 4 (Apache 2.0)"
                   color: "#5A6B52"; font.pixelSize: 14 }
            Text { text: "•  Open Food Facts (Open Database License)"
                   color: "#5A6B52"; font.pixelSize: 14 }
            Text { text: "•  MongoDB Atlas (cloud sync)"
                   color: "#5A6B52"; font.pixelSize: 14 }

            Rectangle { width: parent.width; height: 1; color: "#D8E0CF" }

            Text { text: qsTr("License: MIT — © 2026 ReWinGo Contributors")
                   color: "#1F2A1B"
                   font.pixelSize: 13; font.weight: Font.Bold }
            Text { text: qsTr("github.com/YOUR_USER/rewingo")
                   color: "#0891B2"
                   font.pixelSize: 13 }
        }
    }
}
