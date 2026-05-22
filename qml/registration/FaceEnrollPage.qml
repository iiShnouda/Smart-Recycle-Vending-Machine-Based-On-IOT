import QtQuick
import QtQuick.Controls
import QtMultimedia
import Recycle_Vending_Machine_LCD

/*
 * FaceEnrollPage — captures 5 face frames via the Pi camera and asks
 * FaceService to compute + store the embedding.
 *
 * Visual: round camera preview ringed by a progress arc (0 → 5 steps).
 */
Rectangle {
    id: page
    objectName: "faceEnrollPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    // Temp user info — admin can fill in a real name on RegistrationCompletePage
    property string newUserId:   "u_" + Date.now()
    property string newUserName: "New User"

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    Component.onCompleted: FaceService.startEnrollment(newUserId, newUserName)
    Component.onDestruction: FaceService.cancel()

    // ──────── Camera ────────
    CaptureSession {
        id: cs
        camera: Camera { id: cam; active: true }
        videoOutput: videoOut
    }

    // Frame timer — grabs the latest video frame ~10× per second
    // and feeds it to FaceService.
    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: {
            const img = videoOut.videoSink.videoFrame.toImage()
            if (img && !img.isNull) FaceService.feedFrame(img)
        }
    }

    Connections {
        target: FaceService
        function onEnrollSucceeded(uid) {
            stackView.replace(
                "qrc:/qt/qml/Recycle_Vending_Machine_LCD/qml/registration/RegistrationCompletePage.qml",
                { userId: uid })
        }
        function onEnrollFailed(reason) {
            errorBanner.text = qsTr("Failed: ") + reason
        }
    }

    // ──────── UI ────────
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left
        anchors.topMargin: 30; anchors.leftMargin: 30
        width: 90; height: 90; radius: 45
        color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"
        TapHandler { onTapped: stackView.pop() }
        Text { anchors.centerIn: parent; text: "←"; font.pixelSize: 36; color: "#1F2A1B" }
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 60
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4
        Text {
            text: { langTick; return qsTr("Face Registration") }
            color: "#1F2A1B"; font.pixelSize: 50; font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: { langTick; return qsTr("Look at the camera and stay still") }
            color: "#5A6B52"; font.pixelSize: 20
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // Round camera preview with progress arc
    Item {
        id: ringWrap
        anchors.centerIn: parent
        width: 520; height: 520

        // Progress arc (Canvas)
        Canvas {
            id: arc
            anchors.fill: parent
            property int progress: FaceService.enrollProgress
            property int total:    FaceService.enrollTotal
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const cx = width/2, cy = height/2
                const r  = Math.min(width, height)/2 - 18
                // background ring
                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, 2*Math.PI)
                ctx.lineWidth   = 16
                ctx.strokeStyle = "#D8E0CF"
                ctx.stroke()
                // foreground arc
                const start = -Math.PI/2
                const end   = start + (2*Math.PI) * (progress / total)
                ctx.beginPath()
                ctx.arc(cx, cy, r, start, end)
                ctx.lineWidth   = 16
                ctx.strokeStyle = "#0891B2"
                ctx.lineCap     = "round"
                ctx.stroke()
            }
            Connections {
                target: FaceService
                function onProgressChanged() { arc.requestPaint() }
            }
        }

        // Round video — clipped to a circle by a Rectangle with circular mask
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 60
            height: parent.height - 60
            radius: width / 2
            color: "#1A1D1A"
            clip: true
            VideoOutput {
                id: videoOut
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectCrop
            }
        }
    }

    Column {
        anchors.top: ringWrap.bottom
        anchors.topMargin: 30
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: { langTick;
                    return FaceService.enrollProgress + " / "
                         + FaceService.enrollTotal + "  " + qsTr("captured") }
            color: "#1F2A1B"; font.pixelSize: 24; font.weight: Font.ExtraBold
        }
        Text {
            id: errorBanner
            anchors.horizontalCenter: parent.horizontalCenter
            text: ""
            color: "#DC2626"; font.pixelSize: 16
        }
    }
}
