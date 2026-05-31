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
    // The kiosk owns the camera (QtMultimedia → VideoOutput below). Each
    // frame is JPEG-encoded and piped over stdin to the Python sidecar,
    // which runs MediaPipe liveness + ArcFace and streams JSON events
    // back. The kiosk never opens a separate cv2 window.
    Component.onCompleted: {
        Idle.touch()
        cam.active = true
        FaceRec.identify()
    }
    Component.onDestruction: {
        FaceRec.cancel()
        cam.active = false       // power down camera
    }
    StackView.onActivated:   Idle.touch()
    StackView.onDeactivated: { FaceRec.cancel(); cam.active = false }

    // ── Camera + frame pump ──────────────────────────────────────
    CaptureSession {
        id: cs
        camera: Camera { id: cam; active: false }
        videoOutput: videoOut
    }
    Timer {
        // ~10 fps is more than enough for MediaPipe + ArcFace; trying
        // to push 30 fps just floods the pipe with redundant frames.
        interval: 100
        running: cam.active && status === 0 && FaceRec.running
        repeat: true
        onTriggered: {
            const img = videoOut.videoSink.videoFrame.toImage()
            if (img && !img.isNull) FaceRec.feedFrame(img)
        }
    }

    // ── FaceRec events ───────────────────────────────────────────
    Connections {
        target: FaceRec
        function onIdentified(name, score) {
            status = 1
            successPause.userId = ""        // sidecar doesn't expose IDs yet
            successPause.userName = name
            successPause.start()
        }
        function onUnknown(bestScore) {
            status = 2
            failurePause.start()
        }
        function onFailed(reason) {
            status = 3
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
            stackView.push("qrc:/Recycle_Vending_Machine_LCD/qml/MainPage.qml",
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
                "qrc:/Recycle_Vending_Machine_LCD/qml/registration/ConsentPage.qml")
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
            text: {
                langTick
                if (status === 1) return qsTr("Welcome back!")
                if (status === 2) return qsTr("Not recognised")
                if (status === 3) return qsTr("Error")
                // status 0 = scanning — drive prompt off the sidecar stage.
                switch (FaceRec.stage) {
                case "BLINK":      return qsTr("Blink twice")
                                          + "  (" + FaceRec.blinkCount + "/" + FaceRec.blinkRequired + ")"
                case "TURN_RIGHT": return qsTr("Turn your head RIGHT  →")
                case "TURN_LEFT":  return qsTr("←  Turn your head LEFT")
                case "RECOGNIZE":  return qsTr("Hold still…")
                default:           return qsTr("Look at the camera")
                }
            }
            color: status === 1 ? "#16A34A"
                 : status === 2 ? "#DC2626"
                 : status === 3 ? "#DC2626" : "#0891B2"
            font.pixelSize: 26
            font.weight: Font.DemiBold
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
        horizontalAlignment: Text.AlignHCenter
        width: page.width * 0.86
        wrapMode: Text.WordWrap
        text: {
            langTick
            if (status === 1) return qsTr("Logging you in")
            if (status === 2) return qsTr("Switching to QR login")
            if (status === 3) return FaceRec.status   // contains the error reason
            return FaceRec.status.length > 0 ? FaceRec.status : qsTr("Starting…")
        }
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
