import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../../components"   // CameraPreview lives here (not a global module type)

/*
 * FaceEnrollPage — self-capture face registration.
 *
 * The Python sidecar (scripts.enroll_selfcam) opens the camera itself, auto-
 * advances through 3 head poses with NO screen press, writes a live preview to
 * /tmp/rewingo_face.jpg, and stores the embedding in the same faces.db login
 * reads. This page shows that preview inside a TRUE circle (a Canvas "donut"
 * paints the page background over the square image's corners) with the progress
 * ring OUTSIDE the circle, counting captured poses N / 3.
 */
Rectangle {
    id: page
    objectName: "faceEnrollPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    // The verified mobile number, passed in from RegistrationDetailsPage. The
    // face is enrolled here, then linked to this number.
    property string pendingMobile: ""
    property string enrolledUserId: ""

    property int langTick: 0
    property int previewTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Mobile-first: the number was already entered + verified on the previous
    // page. Capture the face here, then write the verified mobile onto the new
    // user row (finalizeUser) and finish.
    Component.onCompleted: FaceRec.enroll("")
    Component.onDestruction: FaceRec.cancel()
    StackView.onDeactivated:  FaceRec.cancel()

    function goComplete() {
        stackView.replace(
            "qrc:/Recycle_Vending_Machine_LCD/qml/registration/RegistrationCompletePage.qml",
            { userId: page.enrolledUserId,
              userName: page.pendingMobile, userMobile: page.pendingMobile })
    }

    Connections {
        target: FaceRec
        function onEnrolled(name, userId) {
            // Face captured → link the verified mobile to this new user, then go.
            page.enrolledUserId = "" + userId
            FaceRec.finalizeUser(userId, page.pendingMobile, page.pendingMobile)
        }
        function onFinalized(userId) { page.goComplete() }
        function onFailed(reason) {
            // finalize failed but the face IS enrolled → still finish; only an
            // enroll failure (before we got a userId) is a hard error.
            if (page.enrolledUserId.length > 0) page.goComplete()
            else errorBanner.text = qsTr("Couldn't register: ") + reason
        }
        function onEnrollProgressChanged() { arc.requestPaint() }
    }

    // ──────── Back ────────
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left
        anchors.topMargin: 30; anchors.leftMargin: 30
        width: 90; height: 90; radius: 45; z: 10
        color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"
        TapHandler { onTapped: stackView.pop() }
        Text { anchors.centerIn: parent; text: "←"; font.pixelSize: 36; color: "#1F2A1B" }
    }

    // ──────── Title ────────
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
            text: { langTick; return FaceRec.status.length > 0
                                      ? FaceRec.status
                                      : qsTr("Look at the camera and stay still") }
            color: "#5A6B52"; font.pixelSize: 22
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // ──────── Round preview + progress ring (ring OUTSIDE the face) ────────
    Item {
        id: ringWrap
        anchors.centerIn: parent
        width: 560; height: 560

        property real ringR: width / 2 - 16   // progress ring radius (outer)
        property real faceR: width / 2 - 78    // face circle radius (inner, smaller)

        // Progress arc — drawn at the OUTER radius so it rings the face.
        Canvas {
            id: arc
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const cx = width / 2, cy = height / 2, r = ringWrap.ringR
                // background ring
                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                ctx.lineWidth = 18
                ctx.strokeStyle = "#D8E0CF"
                ctx.stroke()
                // foreground progress
                const total = Math.max(1, FaceRec.enrollTotal)
                const frac  = Math.max(0, Math.min(1, FaceRec.enrollCount / total))
                if (frac > 0) {
                    const start = -Math.PI / 2
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, start, start + 2 * Math.PI * frac)
                    ctx.lineWidth = 18
                    ctx.strokeStyle = "#0891B2"
                    ctx.lineCap = "round"
                    ctx.stroke()
                }
            }
            Connections {
                target: FaceRec
                function onEnrollProgressChanged() { arc.requestPaint() }
            }
        }

        // Flicker-free circular live preview (double-buffered image provider).
        Item {
            anchors.centerIn: parent
            width: ringWrap.faceR * 2
            height: ringWrap.faceR * 2

            CameraPreview { anchors.fill: parent; maskColor: page.color }
        }
    }

    // ──────── Count + error ────────
    Column {
        anchors.top: ringWrap.bottom
        anchors.topMargin: 26
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: FaceRec.enrollCount + " / " + FaceRec.enrollTotal
            color: "#1F2A1B"; font.pixelSize: 30; font.weight: Font.Black
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: { langTick; return qsTr("poses captured") }
            color: "#5A6B52"; font.pixelSize: 18
        }
        Text {
            id: errorBanner
            anchors.horizontalCenter: parent.horizontalCenter
            text: ""
            color: "#DC2626"; font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            width: page.width * 0.8
            wrapMode: Text.WordWrap
        }
    }
}
