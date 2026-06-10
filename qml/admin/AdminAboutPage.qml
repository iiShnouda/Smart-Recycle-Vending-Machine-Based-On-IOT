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

    Component.onCompleted: {
        Idle.disable()
        // If a previous (auto) check already found an update, pop the
        // prompt immediately on entering the page; otherwise kick off a
        // fresh check so it's current.
        if (UpdateInfo.updateAvailable) showUpdatePopup = true
        else UpdateInfo.checkNow()
    }
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
                text: qsTr("ReWinGo")
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
        height: UpdateInfo.updateAvailable ? 360 : 130
        Behavior on height { NumberAnimation { duration: 160 } }
        radius: 22
        color: UpdateInfo.updateAvailable ? "#FEF3C7" : "#FFFFFF"
        border.width: 2
        border.color: UpdateInfo.updateAvailable ? "#FBBF24" : "#D8E0CF"

        // Live install-flow state. The button label / progress bar listen
        // to these instead of trying to track UpdateInfo.busy alone.
        property string installStatus: ""        // "downloading" | "installing" | "" (idle)
        property real   installProgress: 0.0     // 0..1 during download

        Connections {
            target: UpdateInfo
            function onDownloadProgress(received, total) {
                if (total > 0) {
                    updateCard.installStatus  = "downloading"
                    updateCard.installProgress = received / total
                }
            }
            function onInstallStarted() {
                updateCard.installStatus = "installing"
                updateCard.installProgress = 1.0
            }
            function onInstallFinished(msg) {
                if (msg.length > 0)
                    installError.text = msg
                else
                    installError.text = qsTr("Restarting…")
                updateCard.installStatus = ""
            }
            function onInstallFailed(reason) {
                installError.text = qsTr("Install failed: ") + reason
                updateCard.installStatus = ""
                updateCard.installProgress = 0.0
            }
        }

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
                    // Check → Install swap: this button hides the moment an
                    // update is found; the green Install button below takes over.
                    visible: !UpdateInfo.updateAvailable
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

            // ── Install Update button + progress (only when an update is available)
            Rectangle {
                visible: UpdateInfo.updateAvailable
                width: parent.width; height: 70; radius: 35
                color: updateCard.installStatus !== "" ? "#9CA3AF" : "#16A34A"

                TapHandler {
                    enabled: updateCard.installStatus === ""
                    onTapped: UpdateInfo.downloadAndInstall()
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 12
                    BusyIndicator {
                        visible: updateCard.installStatus !== ""
                        running: updateCard.installStatus !== ""
                        width: 28; height: 28
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: updateCard.installStatus === "downloading"
                              ? qsTr("Downloading… %1%")
                                    .arg(Math.round(updateCard.installProgress * 100))
                            : updateCard.installStatus === "installing"
                              ? qsTr("Installing — the app will restart…")
                              : qsTr("⤓  Download & install v") + UpdateInfo.latestVersion
                        color: "#FFFFFF"
                        font.pixelSize: 18; font.weight: Font.ExtraBold
                    }
                }
            }
            Text {
                id: installError
                visible: UpdateInfo.updateAvailable && text.length > 0
                text: ""
                color: text.indexOf("failed") >= 0 ? "#DC2626" : "#16A34A"
                font.pixelSize: 12
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

        // Clean, app-style footer — just a copyright line. (Removed the
        // "Built with Qt 6 / OpenCV / …" tech-stack list and the repo URL;
        // those are dev-facing and don't belong in a shipped kiosk About.)
        Text {
            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            text: "© 2026 ReWinGo"
            color: "#5A6B52"
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
    }

    // ============ Update-available popup ============
    // Auto-pops the moment a check (manual or automatic) finds a newer
    // version, so the admin gets an explicit "Install now / Later" prompt
    // instead of having to spot the inline button.
    property bool   showUpdatePopup: false
    property string popupStatus:     ""     // "" | "downloading" | "installing"
    property real   popupProgress:   0.0

    Connections {
        target: UpdateInfo
        function onLatestVersionChanged() {
            if (UpdateInfo.updateAvailable) page.showUpdatePopup = true
        }
        function onNewUpdateDetected(v, notes) { page.showUpdatePopup = true }
        function onDownloadProgress(recv, total) {
            if (total > 0) { page.popupStatus = "downloading"
                             page.popupProgress = recv / total }
        }
        function onInstallStarted()  { page.popupStatus = "installing"
                                       page.popupProgress = 1.0 }
        function onInstallFailed(r)  { page.popupStatus = ""; page.popupProgress = 0 }
    }

    Rectangle {
        id: updatePopup
        anchors.fill: parent
        visible: page.showUpdatePopup
        color: "#CC000000"          // dim backdrop
        z: 100000
        // Swallow taps on the backdrop so they don't fall through.
        MouseArea { anchors.fill: parent }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.72
            radius: 28
            color: "#FFFFFF"
            border.width: 2
            border.color: "#FBBF24"
            height: popupCol.implicitHeight + 56

            Column {
                id: popupCol
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 28
                spacing: 16

                Row {
                    spacing: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                    Rectangle {
                        width: 56; height: 56; radius: 28; color: "#FBBF24"
                        anchors.verticalCenter: parent.verticalCenter
                        Text { anchors.centerIn: parent; text: "⤓"
                               font.pixelSize: 34; font.weight: Font.Black
                               color: "#FFFFFF" }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Update available")
                        color: "#1F2A1B"
                        font.pixelSize: 30; font.weight: Font.ExtraBold
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Version ") + UpdateInfo.latestVersion +
                          qsTr("  (you have ") + UpdateInfo.currentVersion + ")"
                    color: "#5A6B52"
                    font.pixelSize: 18; font.weight: Font.DemiBold
                }

                // Progress text while installing.
                Text {
                    visible: page.popupStatus !== ""
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: page.popupStatus === "downloading"
                          ? qsTr("Downloading… %1%").arg(Math.round(page.popupProgress*100))
                          : qsTr("Installing — the app will restart…")
                    color: "#0891B2"
                    font.pixelSize: 18; font.weight: Font.Bold
                }

                // Buttons (hidden once install starts).
                Row {
                    visible: page.popupStatus === ""
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 18

                    Rectangle {
                        width: 200; height: 72; radius: 36; color: "#16A34A"
                        Text { anchors.centerIn: parent
                               text: qsTr("Install now")
                               color: "#FFFFFF"; font.pixelSize: 22
                               font.weight: Font.ExtraBold }
                        TapHandler { onTapped: UpdateInfo.downloadAndInstall() }
                    }
                    Rectangle {
                        width: 160; height: 72; radius: 36
                        color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"
                        Text { anchors.centerIn: parent
                               text: qsTr("Later")
                               color: "#1F2A1B"; font.pixelSize: 22
                               font.weight: Font.ExtraBold }
                        TapHandler { onTapped: page.showUpdatePopup = false }
                    }
                }

                BusyIndicator {
                    visible: page.popupStatus !== ""
                    running: page.popupStatus !== ""
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 44; height: 44
                }
            }
        }
    }
}
