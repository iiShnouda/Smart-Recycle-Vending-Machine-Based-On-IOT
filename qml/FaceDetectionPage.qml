import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../components"

/*
 * FaceDetectionPage — user-side face login.
 *
 * The Python sidecar (scripts.sidecar_identify_selfcam) OPENS THE CAMERA
 * ITSELF, runs recognition AND writes a live preview frame to
 * /tmp/rewingo_face.jpg. This page shows that preview (a separate, parallel
 * process does the detection) and reacts to the sidecar's JSON events:
 *
 *   identified → MainPage
 *   unknown / error / 12s timeout → ConsentPage (registration)
 */
Rectangle {
    id: page
    objectName: "faceDetectionPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    property int  status: 0       // 0 = scanning, 1 = matched, 2 = noMatch, 3 = error
    property int  previewTick: 0  // bumps to reload the live preview frame

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // ── Lifecycle: the sidecar owns the camera now ───────────────
    Component.onCompleted: {
        Idle.touch()
        FaceRec.identify()
    }
    Component.onDestruction: FaceRec.cancel()
    StackView.onActivated:   Idle.touch()
    StackView.onDeactivated: FaceRec.cancel()

    // Reload the preview frame the sidecar writes (~8 fps).
    Timer {
        interval: 120; running: status === 0; repeat: true
        onTriggered: page.previewTick++
    }

    // ── FaceRec events ───────────────────────────────────────────
    Connections {
        target: FaceRec
        function onIdentified(name, score) {
            status = 1
            successPause.userName = name
            successPause.start()
        }
        function onUnknown(bestScore) { status = 2; failurePause.start() }
        function onFailed(reason)     { status = 3; failurePause.start() }
    }

    Timer {
        id: successPause
        interval: 900; repeat: false
        property string userId
        property string userName
        onTriggered: {
            stackView.push("qrc:/Recycle_Vending_Machine_LCD/qml/MainPage.qml",
                           { userName: userName, userId: userId })
        }
    }
    Timer {
        id: failurePause
        interval: 1500; repeat: false
        onTriggered: {
            stackView.replace(
                "qrc:/Recycle_Vending_Machine_LCD/qml/registration/ConsentPage.qml")
        }
    }
    // Safety net: if the sidecar never returns a result, don't get stuck.
    // 20 s — must comfortably exceed camera-open (~3 s) + model-load (~2 s) +
    // the recognise window (~8 s), or it kicks enrolled users to registration
    // before recognition even finishes. A real match still early-exits in ~2 s.
    Timer {
        id: scanTimeout
        interval: 20000
        running: status === 0
        repeat: false
        onTriggered: {
            if (status !== 0) return
            status = 2
            FaceRec.cancel()
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
                return qsTr("Look at the camera")
            }
            color: status === 1 ? "#16A34A"
                 : status === 2 ? "#DC2626"
                 : status === 3 ? "#DC2626" : "#0891B2"
            font.pixelSize: 26
            font.weight: Font.DemiBold
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // ── Live camera preview (frames written by the recognition sidecar) ──
    Item {
        id: ring
        anchors.centerIn: parent
        width: 760; height: 760

        FaceIdIcon {
            anchors.fill: parent
            color: status === 1 ? "#16A34A"
                 : status === 2 ? "#DC2626" : "#0891B2"
            scanLine: status === 0
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width  * 0.66
            height: parent.height * 0.66
            radius: width / 2
            color: "#1A1D1A"
            clip: true

            // The actual camera image, reloaded each tick. Empty source when
            // we have a result so the ✓/✗ overlay shows on a clean disc.
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                cache: false
                // C++ provider reads the sidecar's frame fresh each tick — the
                // old file:///tmp/...?t= approach left this disc blank on the Pi.
                source: status === 0
                        ? "image://facepreview/" + previewTick
                        : ""
            }

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
            if (status === 2) return qsTr("New here? Let's get you registered")
            if (status === 3) return FaceRec.status
            return FaceRec.status.length > 0 ? FaceRec.status : qsTr("Scanning…")
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
