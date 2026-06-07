import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../components"

/*
 * FaceDetectionPage — user-side face login.
 *
 * The Python sidecar (scripts.sidecar_identify_selfcam) OPENS THE CAMERA
 * ITSELF and streams JSON events back — the kiosk no longer pumps frames
 * over QtMultimedia (that pipeline stalled on the Pi and hung the sidecar).
 * So this page is now just: launch the sidecar, show a scanning animation,
 * and react to identified / unknown / error / timeout.
 *
 *   identified → MainPage
 *   unknown / error / 12s timeout → ConsentPage (registration)
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

    // ── Lifecycle: the sidecar owns the camera now ───────────────
    Component.onCompleted: {
        Idle.touch()
        FaceRec.identify()
    }
    Component.onDestruction: FaceRec.cancel()
    StackView.onActivated:   Idle.touch()
    StackView.onDeactivated: FaceRec.cancel()

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
            // Unknown / error → registration flow:
            //   ConsentPage → FaceEnrollPage → RegistrationCompletePage → MainPage
            stackView.replace(
                "qrc:/Recycle_Vending_Machine_LCD/qml/registration/ConsentPage.qml")
        }
    }
    // Safety net: if the sidecar never returns a result, don't get stuck.
    Timer {
        id: scanTimeout
        interval: 12000
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

    // ── Scanning visual (no live preview — sidecar owns the camera) ──
    Item {
        id: ring
        anchors.centerIn: parent
        width: 760; height: 760

        FaceIdIcon {
            anchors.fill: parent
            color: status === 1 ? "#16A34A"
                 : status === 2 ? "#DC2626" : "#0891B2"
            scanLine: status === 0          // animated scan line while scanning
        }

        // Soft pulsing disc inside the brackets so the user sees it's "alive".
        Rectangle {
            id: disc
            anchors.centerIn: parent
            width: parent.width  * 0.62
            height: parent.height * 0.62
            radius: width / 2
            color: status === 1 ? "#16A34A"
                 : status === 2 ? "#DC2626"
                 : status === 3 ? "#DC2626" : "#0891B2"
            opacity: 0.12

            SequentialAnimation on scale {
                running: status === 0
                loops: Animation.Infinite
                NumberAnimation { to: 1.06; duration: 1100; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.94; duration: 1100; easing.type: Easing.InOutSine }
            }
        }

        Text {
            anchors.centerIn: parent
            text: status === 1 ? "✓" : status === 0 ? "🙂" : "✗"
            color: status === 1 ? "#16A34A"
                 : status === 0 ? "#0891B2" : "#DC2626"
            font.pixelSize: status === 0 ? 150 : 200
            font.weight: Font.Black
            style: Text.Outline
            styleColor: "#FFFFFF"
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
