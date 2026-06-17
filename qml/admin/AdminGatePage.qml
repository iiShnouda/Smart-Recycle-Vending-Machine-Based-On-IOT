import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../../components"   // CameraPreview (not a global module type)

/*
 * AdminGatePage — face-scan gate before the admin panel.
 *
 * States (from AdminAuth.state):
 *   IDLE      → "Tap to scan"            light grey ring
 *   SCANNING  → animated rings           ice-blue, pulsating
 *   ACCEPTED  → green check-mark         ✓
 *   REJECTED  → red X                    ✗  (attemptsRemaining decrements)
 *   LOCKED    → red lock icon            30 s lockout countdown
 *
 * On ACCEPTED → unlocked() signal fires → we push AdminMainPage.
 */
Rectangle {
    id: page
    objectName: "adminGatePage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view

    // Reset state on entry + disable idle timer for the whole admin flow.
    // Re-enable is done by the Exit button in AdminMainPage, NOT here —
    // because Component.onDestruction fires on every navigation push too,
    // which would re-enable the timer while admin is still inside the panel.
    Component.onCompleted: { AdminAuth.reset(); Idle.disable() }

    Connections {
        target: AdminAuth
        function onUnlocked() {
            // Small delay so the user sees the green check
            unlockDelay.start()
        }
    }

    Timer {
        id: unlockDelay
        interval: 700
        onTriggered: stackView.replace(Qt.resolvedUrl("AdminMainPage.qml"))
    }

    // ============ Top bar ============
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 30
        anchors.leftMargin: 30
        width: 90
        height: 90
        radius: 45
        color: "#FFFFFF"
        border.width: 2
        border.color: "#D8E0CF"
        z: 10
        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: {
                // Closing without auth = back to main flow + re-arm reed
                appManager.rearmReed()
                while (stackView && stackView.depth > 1) stackView.pop()
            }
        }
        Text {
            anchors.centerIn: parent
            text: "✕"
            font.pixelSize: 32
            font.weight: Font.Black
            color: "#1F2A1B"
        }
    }

    // ============ Header ============
    Column {
        anchors.top: parent.top
        anchors.topMargin: 70
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10
        Text {
            text: "Admin Access"
            color: "#1F2A1B"
            font.pixelSize: 64
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: AdminAuth.state === AdminAuth.LOCKED
                  ? "Too many failed attempts. Locked for 30 s."
                  : "Look at the camera"
            color: "#5A6B52"
            font.pixelSize: 22
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // ============ Scan ring ============
    Item {
        id: ring
        anchors.centerIn: parent
        width: 420; height: 420

        // Pulsing inner glow (only when scanning)
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 10
            border.color: ring.ringColor()
            opacity: 0.35
            scale: AdminAuth.state === AdminAuth.SCANNING ? 1.05 : 1.0
            Behavior on scale {
                NumberAnimation { duration: 1200; easing.type: Easing.InOutSine }
            }

            SequentialAnimation on opacity {
                running: AdminAuth.state === AdminAuth.SCANNING
                loops: Animation.Infinite
                NumberAnimation { to: 0.10; duration: 800 }
                NumberAnimation { to: 0.45; duration: 800 }
            }
        }

        // Main ring
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 12
            border.color: ring.ringColor()
        }

        // Inner ring (light tint)
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 80
            height: parent.height - 80
            radius: width / 2
            color: "#FFFFFF"
            border.width: 2
            border.color: "#D8E0CF"
        }

        // Live face preview while scanning (admin login IS face login).
        Item {
            anchors.centerIn: parent
            width: parent.width - 96
            height: parent.height - 96
            visible: AdminAuth.state === AdminAuth.SCANNING
            CameraPreview {
                anchors.fill: parent
                circular: true
                maskColor: "#FFFFFF"
                active: AdminAuth.state === AdminAuth.SCANNING
            }
        }

        // Center icon (hidden while the camera is up)
        Text {
            anchors.centerIn: parent
            visible: AdminAuth.state !== AdminAuth.SCANNING
            text: {
                switch (AdminAuth.state) {
                    case AdminAuth.SCANNING: return "👤"
                    case AdminAuth.ACCEPTED: return "✓"
                    case AdminAuth.REJECTED: return "✗"
                    case AdminAuth.LOCKED:   return "🔒"
                    default:                 return "👤"
                }
            }
            font.pixelSize: AdminAuth.state === AdminAuth.ACCEPTED
                          || AdminAuth.state === AdminAuth.REJECTED
                          ? 170 : 140
            color: ring.ringColor()
            font.weight: Font.Black
        }

        // Tap to start
        TapHandler {
            enabled: AdminAuth.state === AdminAuth.IDLE
                  || AdminAuth.state === AdminAuth.REJECTED
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: AdminAuth.startScan()
        }

        function ringColor() {
            switch (AdminAuth.state) {
                case AdminAuth.ACCEPTED: return "#16A34A"   // green
                case AdminAuth.REJECTED: return "#DC2626"   // red
                case AdminAuth.LOCKED:   return "#DC2626"
                case AdminAuth.SCANNING: return "#0891B2"   // ice blue
                default:                 return "#7A8B6A"   // sage
            }
        }
    }

    // ============ Status text ============
    Column {
        anchors.top: ring.bottom
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#1F2A1B"
            font.pixelSize: 30
            font.weight: Font.ExtraBold
            text: {
                switch (AdminAuth.state) {
                    case AdminAuth.IDLE:     return "Tap to scan"
                    case AdminAuth.SCANNING: return "Scanning..."
                    case AdminAuth.ACCEPTED: return "Welcome, " + AdminAuth.adminName
                    case AdminAuth.REJECTED: return "Not recognised or not an admin"
                    case AdminAuth.LOCKED:   return "Locked"
                    default:                 return ""
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#5A6B52"
            font.pixelSize: 20
            visible: AdminAuth.state === AdminAuth.REJECTED
                  || AdminAuth.state === AdminAuth.IDLE
            text: "Attempts remaining: " + AdminAuth.attemptsRemaining
        }
    }
}
