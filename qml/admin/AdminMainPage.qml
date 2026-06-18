import QtQuick
import QtQuick.Controls
import Qt.labs.settings
import Recycle_Vending_Machine_LCD

/*
 * AdminMainPage — dashboard after successful face scan.
 *
 *   ┌──────────────────────────────────────┐
 *   │ [Logout]    Welcome, <Admin name>    │
 *   ├──────────────────────────────────────┤
 *   │  ┌──────────┐  ┌──────────┐          │
 *   │  │ Products │  │ Analytics│          │
 *   │  └──────────┘  └──────────┘          │
 *   │  ┌──────────┐  ┌──────────┐          │
 *   │  │ Diagnose │  │   Logs   │          │
 *   │  └──────────┘  └──────────┘          │
 *   ├──────────────────────────────────────┤
 *   │ Today: 32 recycles • 14 vendings     │
 *   └──────────────────────────────────────┘
 */
Rectangle {
    id: page
    objectName: "adminMainPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view

    // Admin pages never time out — they re-disable on every entry in case
    // some earlier navigation re-enabled the timer.
    Component.onCompleted: {
        Idle.disable()
        // Apply the admin's preferred language on each entry to this page.
        if (adminSettings.lang !== "")
            appManager.selectLanguage(adminSettings.lang)
        // First-admin bootstrap: if this face got in only because no admin
        // exists yet, offer to make it THE admin (locks the gate to admins).
        if (AdminAuth.isBootstrapAdmin) promoteDialog.open()
    }
    StackView.onActivated: Idle.disable()

    // Persistent admin language pref.
    Settings {
        id: adminSettings
        category: "admin"
        property string lang: "en"
    }

    // ============ Top bar ============
    Item {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 24
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        height: 90

        // Logout button (also re-arms reed monitor for next admin trigger)
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 130
            height: 90
            radius: 45
            color: "#DC2626"
            TapHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                onTapped: {
                    AdminAuth.logout()
                    appManager.rearmReed()
                    Idle.enable()                  // resume normal idle behavior
                    while (stackView && stackView.depth > 1) stackView.pop()
                }
            }
            Row {
                anchors.centerIn: parent
                spacing: 8
                Text { text: "⎋"; color: "#FFFFFF"; font.pixelSize: 30; font.weight: Font.Black
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("Exit"); color: "#FFFFFF"; font.pixelSize: 22; font.weight: Font.ExtraBold
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // Language toggle (top-right) — flips EN ↔ AR + persists.
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 130; height: 90; radius: 45
            color: "transparent"
            border.width: 2; border.color: "#7A8B6A"
            TapHandler {
                onTapped: {
                    const next = adminSettings.lang === "ar" ? "en" : "ar"
                    adminSettings.lang = next
                    appManager.selectLanguage(next)
                }
            }
            Text {
                anchors.centerIn: parent
                text: adminSettings.lang === "ar" ? "العربية" : "English"
                color: "#1F2A1B"
                font.pixelSize: 18; font.weight: Font.ExtraBold
            }
        }
    }

    // ============ First-admin bootstrap dialog ============
    // Shown once, when someone enters via the no-admin bootstrap. Promoting
    // writes role=admin onto their face in faces.db, after which ONLY admins
    // (e.g. shenoo) can open this panel.
    Dialog {
        id: promoteDialog
        property bool busy: false
        property string note: ""
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true; focus: true
        closePolicy: Popup.NoAutoClose
        Overlay.modal: Rectangle { color: "#A0000000" }
        width: 720; height: 460
        standardButtons: Dialog.NoButton

        Connections {
            target: FaceRec
            function onRoleSet(userId, role, ok) {
                promoteDialog.busy = false
                promoteDialog.note = ok ? qsTr("Done — you are now the admin.")
                                        : qsTr("Couldn't save. Try again.")
                if (ok) closeTimer.start()
            }
        }
        Timer { id: closeTimer; interval: 1100; onTriggered: promoteDialog.close() }

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 16
            Text { text: "🔐"; font.pixelSize: 72
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Make this face the admin?")
                   color: "#1F2A1B"; font.pixelSize: 28; font.weight: Font.ExtraBold
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("No admin is set yet, so anyone enrolled can open this panel. "
                            + "Set %1 as the admin to lock it to you only.")
                        .arg(AdminAuth.adminName.length > 0 ? AdminAuth.adminName : qsTr("this face"))
                   color: "#5A6B52"; font.pixelSize: 18
                   horizontalAlignment: Text.AlignHCenter; width: parent.width
                   wrapMode: Text.WordWrap
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: promoteDialog.note; color: "#0891B2"; font.pixelSize: 18
                   font.weight: Font.Bold; visible: promoteDialog.note.length > 0
                   anchors.horizontalCenter: parent.horizontalCenter }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16
                Rectangle {
                    width: 220; height: 80; radius: 40; color: "#16A34A"
                    opacity: promoteDialog.busy ? 0.6 : 1.0
                    TapHandler {
                        enabled: !promoteDialog.busy
                        onTapped: {
                            promoteDialog.busy = true
                            promoteDialog.note = qsTr("Saving…")
                            FaceRec.setRole(parseInt(AdminAuth.adminId), "admin")
                        }
                    }
                    Text { anchors.centerIn: parent; text: qsTr("Make me admin")
                           color: "#FFFFFF"; font.pixelSize: 20; font.weight: Font.ExtraBold }
                }
                Rectangle {
                    width: 150; height: 80; radius: 40; color: "#1A1D1A"
                    TapHandler { onTapped: promoteDialog.close() }
                    Text { anchors.centerIn: parent; text: qsTr("Later")
                           color: "#FFFFFF"; font.pixelSize: 20; font.weight: Font.ExtraBold }
                }
            }
        }
    }

    // ============ Update-available banner ============
    // Auto-fires when admin enters this page if a newer version has
    // been detected. The banner stays until the admin taps it (which
    // navigates to AdminAboutPage and silences the "new" toast for
    // this version — see UpdateChecker QSettings tracking).
    Rectangle {
        id: updateBanner
        anchors.top: topBar.bottom
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        height: visible ? 84 : 0
        Behavior on height { NumberAnimation { duration: 160 } }
        visible: UpdateInfo.updateAvailable
        radius: 18
        color: "#FEF3C7"
        border.width: 2; border.color: "#FBBF24"

        Row {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            Rectangle {
                width: 56; height: 56; radius: 28
                color: "#FBBF24"
                anchors.verticalCenter: parent.verticalCenter
                Text { anchors.centerIn: parent; text: "⤓"
                       color: "#FFFFFF"
                       font.pixelSize: 32; font.weight: Font.Black }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 56 - 200 - 28
                spacing: 2
                Text {
                    text: qsTr("New version available: v") +
                          UpdateInfo.latestVersion
                    color: "#92400E"
                    font.pixelSize: 20; font.weight: Font.ExtraBold
                }
                Text {
                    text: qsTr("You're on v%1 — tap to view release notes.")
                              .arg(UpdateInfo.currentVersion)
                    color: "#92400E"
                    font.pixelSize: 13
                }
            }
            Rectangle {
                width: 180; height: 56; radius: 28
                color: "#92400E"
                anchors.verticalCenter: parent.verticalCenter
                TapHandler {
                    onTapped: stackView.push(Qt.resolvedUrl("AdminAboutPage.qml"))
                }
                Text { anchors.centerIn: parent
                       text: qsTr("Show details")
                       color: "#FFFFFF"
                       font.pixelSize: 16; font.weight: Font.ExtraBold }
            }
        }
    }

    // ============ Welcome header ============
    Column {
        id: header
        anchors.top: updateBanner.bottom
        anchors.topMargin: updateBanner.visible ? 16 : 30
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        Text {
            text: qsTr("Admin Panel")
            color: "#1F2A1B"
            font.pixelSize: 60
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: qsTr("Welcome, ") + AdminAuth.adminName
            color: "#0891B2"
            font.pixelSize: 28
            font.weight: Font.ExtraBold
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Point value in EGP — read-only (set by the developer, not the admin).
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: egpRow.implicitWidth + 44
            height: 54; radius: 27
            color: "#ECFDF5"; border.width: 1; border.color: "#16A34A"
            Row {
                id: egpRow
                anchors.centerIn: parent
                spacing: 10
                Text { text: "💱"; font.pixelSize: 24
                       anchors.verticalCenter: parent.verticalCenter }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("1 point = %1 EGP")
                              .arg(RecycleSession.pointValueEGP.toFixed(2))
                    color: "#166534"; font.pixelSize: 21; font.weight: Font.ExtraBold
                }
            }
        }

    }

    // ============ Nav grid ============
    Grid {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        columns: 2
        spacing: 28
        width: parent.width * 0.86

        component NavTile : Item {
            property string label
            property string subtitle
            property color accentColor: "#7A8B6A"
            signal tapped()
            width: (grid.width - grid.spacing) / 2
            height: 280

            Rectangle {
                anchors.fill: card
                anchors.margins: -10
                radius: 32
                color: "#000000"
                opacity: 0.06
                y: 10
                z: -1
            }
            Rectangle {
                id: card
                anchors.fill: parent
                radius: 28
                color: "#FFFFFF"
                border.width: 2
                border.color: "#D8E0CF"

                // Sage accent bar (left edge)
                Rectangle {
                    width: 10; radius: 5
                    color: parent.parent.parent.accentColor
                    anchors.left: parent.left; anchors.leftMargin: 18
                    anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.topMargin: 22; anchors.bottomMargin: 22
                }
                Column {
                    anchors.centerIn: parent
                    spacing: 14
                    width: parent.width - 80
                    Text {
                        text: parent.parent.parent.label
                        color: "#1F2A1B"
                        font.pixelSize: 44
                        font.weight: Font.Black
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: parent.parent.parent.subtitle
                        color: "#5A6B52"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Rectangle {                              // thin separator
                        width: parent.width * 0.4; height: 2
                        color: parent.parent.parent.accentColor
                        opacity: 0.6
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "Open →"
                        color: parent.parent.parent.accentColor
                        font.pixelSize: 18
                        font.weight: Font.ExtraBold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
                TapHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                    onTapped: parent.parent.tapped()
                }
            }
        }

        NavTile {
            label: qsTr("Products"); subtitle: qsTr("Manage the 8 vending slots")
            accentColor: "#7A8B6A"
            onTapped: stackView.push(Qt.resolvedUrl("AdminProductsPage.qml"))
        }
        NavTile {
            label: qsTr("Analytics"); subtitle: qsTr("Sales & recycling stats")
            accentColor: "#0891B2"
            onTapped: stackView.push(Qt.resolvedUrl("AdminAnalyticsPage.qml"))
        }
        NavTile {
            label: qsTr("Diagnostics"); subtitle: qsTr("Test motors and sensors")
            accentColor: "#92400E"
            onTapped: stackView.push(Qt.resolvedUrl("AdminDiagnosticsPage.qml"))
        }
        NavTile {
            label: qsTr("Logs"); subtitle: qsTr("System events & maintenance")
            accentColor: "#5A6B52"
            onTapped: stackView.push(Qt.resolvedUrl("AdminLogsPage.qml"))
        }
        NavTile {
            label: qsTr("Faults"); subtitle: qsTr("Failed dispenses by slot")
            accentColor: "#DC2626"
            onTapped: stackView.push(Qt.resolvedUrl("AdminFaultsPage.qml"))
        }
        NavTile {
            label: qsTr("Inventory"); subtitle: qsTr("Live counts & restock log")
            accentColor: "#16A34A"
            onTapped: stackView.push(Qt.resolvedUrl("AdminInventoryPage.qml"))
        }
        NavTile {
            label: qsTr("About"); subtitle: qsTr("Version, updates, license")
            accentColor: "#0891B2"
            onTapped: stackView.push(Qt.resolvedUrl("AdminAboutPage.qml"))
        }
    }
}
