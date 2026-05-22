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

    // ============ Welcome header ============
    Column {
        id: header
        anchors.top: topBar.bottom
        anchors.topMargin: 30
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
    }
}
