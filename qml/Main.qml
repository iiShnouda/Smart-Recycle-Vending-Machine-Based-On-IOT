import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.VirtualKeyboard
import Recycle_Vending_Machine_LCD

Window {
    id: window
    visible: true
    visibility: Window.Windowed
    // Production display: 15.6" LCD in portrait → 1080 × 1920
    width: 1080
    height: 1500
    minimumWidth: 1080
   minimumHeight: 1920
    color: "#F2F4ED"
    title: "ReWinGo"

    // ===== Window mode =====
    // DEV: normal Windows chrome (min/max/exit visible).
    // When you deploy to the Pi: uncomment the FullScreen + Frameless lines
    // and comment out the Windowed line.

    //flags: Qt.Window           // explicit chrome
    // visibility: Window.FullScreen        // ← deploy mode
    // flags: Qt.FramelessWindowHint | Qt.Window

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#F2F4ED" }
            GradientStop { position: 1.0; color: "#E8EEDB" }
        }
    }

    StackView {
        id: mainStackView
        objectName: "mainStackView"
        anchors.fill: parent
        initialItem: sleepModeComponent

        // ── Morph transitions (cross-fade + scale, Material "shared-axis Z") ──
        // The incoming page grows in from slightly small while the outgoing
        // one zooms out and fades — reads as one screen morphing into the
        // next rather than a hard cut. Applied to every push/pop, so the
        // whole app (MainPage → VendingPage, admin, recycle, …) animates.
        pushEnter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 260; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale";   from: 0.92; to: 1.0; duration: 320; easing.type: Easing.OutCubic }
            }
        }
        pushExit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InCubic }
                NumberAnimation { property: "scale";   from: 1.0; to: 1.08; duration: 320; easing.type: Easing.InCubic }
            }
        }
        popEnter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 260; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale";   from: 1.08; to: 1.0; duration: 320; easing.type: Easing.OutCubic }
            }
        }
        popExit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InCubic }
                NumberAnimation { property: "scale";   from: 1.0; to: 0.92; duration: 320; easing.type: Easing.InCubic }
            }
        }

        // Reset idle timer every time we navigate between pages.
        onCurrentItemChanged: Idle.touch()
    }

    // ===== Idle timer hook =====
    // When 60 s of inactivity passes, pop all the way back to SleepMode.
    Connections {
        target: Idle
        function onTimedOut() {
            while (mainStackView.depth > 1) mainStackView.pop()
        }
    }

    // Any touch anywhere on the window resets the timer.
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: (mouse) => { Idle.touch(); mouse.accepted = false }
    }

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
        function onAdminRequested() {
            // CRITICAL: kill the idle timer BEFORE any page loads so it
            // can't fire mid-admin session. Re-enabled by Exit button.
            Idle.disable()
            mainStackView.push("qrc:/Recycle_Vending_Machine_LCD/qml/admin/AdminGatePage.qml")
        }
    }

    // ===== On-screen keyboard =====
    // Auto-appears whenever a TextField gets focus. Above EVERYTHING else
    // (z=Number.MAX_VALUE) so it floats above modal Dialogs too.
    InputPanel {
        id: keyboard
        z: 1000000
        parent: Overlay.overlay      // sit on Qt Quick's overlay layer,
                                     // which is drawn on top of dialogs
        width: window.width
        x: 0
        y: active ? window.height - height : window.height
        Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        z: 9999
        text: { langTick; return qsTr("Copyright © 2025-2026 ReWinGo Team") }
        color: "#5A6B52"
        font.pixelSize: 14
        opacity: 0.9
        visible: !keyboard.active   // hide when keyboard is up
    }

    Component { id: sleepModeComponent; SleepMode {} }
}
