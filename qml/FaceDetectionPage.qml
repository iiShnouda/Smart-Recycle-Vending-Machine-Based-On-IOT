import QtQuick
import QtQuick.Controls
import QtMultimedia
import Recycle_Vending_Machine_LCD
import "../components"

/*
 * FaceDetectionPage — user-side face login.
 *
 * On entry:
 *   1. Start the camera + the YOLO-backed FaceService::startIdentify()
 *   2. Pump frames from the VideoOutput into FaceService.feedFrame()
 *   3. On match → fetch user from MongoDB → push MainPage
 *   4. On timeout (~10 s) → fall back to QRCodePage
 *
 * On exit (always — even crash/back/timeout):
 *   - Camera switched OFF                ← saves power & wears the sensor less
 *   - FaceService cancelled              ← stops the YOLO loop
 *
 * Visual: live circular camera preview with the iPhone-style Face ID icon
 * pulsing around it, a sub-line for status, and the standard back/exit.
 */
Rectangle {
    id: page
    objectName: "faceDetectionPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    property int  status: 0     // 0 = scanning, 1 = matched, 2 = noMatch, 3 = error

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // ── Lifecycle ────────────────────────────────────────────────
    Component.onCompleted: {
        Idle.touch()
        cam.active = true
        FaceService.startIdentify()
    }
    Component.onDestruction: {
        FaceService.cancel()
        cam.active = false       // power down camera
    }
    StackView.onActivated:   Idle.touch()
    StackView.onDeactivated: { FaceService.cancel(); cam.active = false }

    // ── Camera + frame pump ──────────────────────────────────────
    CaptureSession {
        id: cs
        camera: Camera { id: cam; active: false }
        videoOutput: videoOut
    }
    Timer {
        interval: 120
        running: cam.active && status === 0
        repeat: true
        onTriggered: {
            const img = videoOut.videoSink.videoFrame.toImage()
            if (img && !img.isNull) FaceService.feedFrame(img)
        }
    }

    // ── FaceService events ───────────────────────────────────────
    Connections {
        target: FaceService
        function onMatched(userId, name) {
            status = 1
            successPause.userId = userId
            successPause.userName = name
            successPause.start()
        }
        function onNoMatch() {
            status = 2
            failurePause.start()
        }
    }
    Timer {
        id: successPause
        interval: 900; repeat: false
        property string userId
        property string userName
        onTriggered: {
            cam.active = false
            stackView.push("qrc:/qt/qml/Recycle_Vending_Machine_LCD/qml/MainPage.qml",
                           { userName: userName, userId: userId })
        }
    }
    Timer {
        id: failurePause
        interval: 1500; repeat: false
        onTriggered: {
            // Unknown face → take the user through registration:
            //   ConsentPage  → FaceEnrollPage  → RegistrationCompletePage → MainPage
            cam.active = false
            stackView.replace(
                "qrc:/qt/qml/Recycle_Vending_Machine_LCD/qml/registration/ConsentPage.qml")
        }
    }

    // ── Back button ──────────────────────────────────────────────
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left
        anchors.topMargin: 30; anchors.leftMargin: 30
        width: 90; height: 90; radius: 45
        color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"
        z: 10
        TapHandler { onTapped: { Idle.touch(); stackView.pop() } }
        Text { anchors.centerIn: parent; text: "←"; font.pixelSize: 36; color: "#1F2A1B" }
    }

    // ── Header ───────────────────────────────────────────────────
    Column {
        anchors.top: parent.top; anchors.topMargin: 60
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
        Text { text: { langTick; return qsTr("Face ID") }
               color: "#1F2A1B"
               font.pixelSize: 60; font.weight: Font.Black
               anchors.horizontalCenter: parent.horizontalCenter }
        Text {
            text: { langTick;
                    return status === 0 ? qsTr("Look at the camera")
                         : status === 1 ? qsTr("Welcome back!")
                                        : qsTr("Not recognised") }
            color: status === 1 ? "#16A34A"
                 : status === 2 ? "#DC2626" : "#5A6B52"
            font.pixelSize: 22
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // ── Round camera preview wrapped in iPhone-style brackets ────
    Item {
        id: ring
        anchors.centerIn: parent
        width: 540; height: 540

        // The iPhone Face ID icon, scaled to the full ring area.
        FaceIdIcon {
            anchors.fill: parent
            color: status === 1 ? "#16A34A"
                 : status === 2 ? "#DC2626" : "#0891B2"
            scanLine: status === 0
        }

        // Circular live camera in the middle (covers the icon's face).
        Rectangle {
            anchors.centerIn: parent
            width: parent.width  * 0.62
            height: parent.height * 0.62
            radius: width / 2
            color: "#1A1D1A"
            clip: true
            VideoOutput {
                id: videoOut
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectCrop
            }

            // Big check / X overlay when done
            Text {
                anchors.centerIn: parent
                visible: status !== 0
                text: status === 1 ? "✓" : "✗"
                color: status === 1 ? "#16A34A" : "#DC2626"
                font.pixelSize: 200
                font.weight: Font.Black
                style: Text.Outline
                styleColor: "#FFFFFF"
            }
        }
    }

    // ── Status footer ────────────────────────────────────────────
    Text {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 100
        anchors.horizontalCenter: parent.horizontalCenter
        text: { langTick;
                return status === 0 ? qsTr("Scanning...")
                     : status === 1 ? qsTr("Logging you in")
                                    : qsTr("Switching to QR login") }
        color: "#5A6B52"
        font.pixelSize: 18

        SequentialAnimation on opacity {
            running: status === 0
            loops:   Animation.Infinite
            NumberAnimation { to: 0.4; duration: 1100 }
            NumberAnimation { to: 1.0; duration: 1100 }
        }
    }
}
