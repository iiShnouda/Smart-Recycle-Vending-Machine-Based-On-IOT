import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * ClaimQrPage — OPTIONAL step after registration. Mints a one-time claim token,
 * tells the backend about the pending account (over MQTT), and shows a
 * "REWINGO-CLAIM:<token>" QR. The user scans it in the ReWinGo app while logged
 * in, and the backend auto-links the kiosk account to their app user (no
 * password). Skipping is fine — this never blocks finishing registration.
 */
Rectangle {
    id: page
    objectName: "claimQrPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view
    property string userName: ""
    property string userMobile: ""

    property int langTick: 0
    Connections { target: appManager; function onLanguageChanged() { langTick++ } }

    property int previewTick: 0
    Timer { interval: 400; running: true; repeat: true; onTriggered: page.previewTick++ }

    Component.onCompleted: { Idle.disable(); MachineLink.beginClaim(userName, userMobile) }
    StackView.onActivated:  Idle.disable()

    // Back
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left
        anchors.topMargin: 30; anchors.leftMargin: 30
        width: 90; height: 90; radius: 45; z: 10
        color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"
        TapHandler { onTapped: stackView.pop() }
        Text { anchors.centerIn: parent; text: "←"; font.pixelSize: 36; color: "#1F2A1B" }
    }

    Column {
        anchors.top: parent.top; anchors.topMargin: 46
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8
        Text { text: { langTick; return qsTr("Connect the mobile app") }
               color: "#1F2A1B"; font.pixelSize: 52; font.weight: Font.Black
               anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: { langTick; return qsTr("Open the ReWinGo app and scan this code to link your account") }
               color: "#5A6B52"; font.pixelSize: 26; font.weight: Font.DemiBold
               width: page.width * 0.8; horizontalAlignment: Text.AlignHCenter
               wrapMode: Text.WordWrap
               anchors.horizontalCenter: parent.horizontalCenter }
    }

    // QR card
    Rectangle {
        anchors.centerIn: parent
        width: 430; height: 430; radius: 30
        color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"

        Image {
            id: qrImg
            anchors.centerIn: parent
            width: 360; height: 360
            fillMode: Image.PreserveAspectFit
            cache: false
            asynchronous: true
            source: MachineLink.qrImagePath.length > 0
                    ? "file://" + MachineLink.qrImagePath + "?t=" + previewTick : ""
        }
        BusyIndicator {
            anchors.centerIn: parent
            running: MachineLink.qrImagePath.length === 0 || qrImg.status !== Image.Ready
            visible: running
            width: 64; height: 64
        }
    }

    Text {
        anchors.bottom: doneBtn.top; anchors.bottomMargin: 26
        anchors.horizontalCenter: parent.horizontalCenter
        horizontalAlignment: Text.AlignHCenter
        width: page.width * 0.8; wrapMode: Text.WordWrap
        text: { langTick; return qsTr("After scanning, set your password and email in the app.") }
        color: "#5A6B52"; font.pixelSize: 22; font.weight: Font.DemiBold
    }

    // Finish (whether or not they scanned — auto-link happens in the app).
    Rectangle {
        id: doneBtn
        anchors.bottom: parent.bottom; anchors.bottomMargin: 70
        anchors.horizontalCenter: parent.horizontalCenter
        width: 320; height: 96; radius: 48; color: "#1A1D1A"
        scale: doneTap.pressed ? 0.96 : 1.0
        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
        TapHandler {
            id: doneTap
            onTapped: { while (stackView && stackView.depth > 1) stackView.pop() }
        }
        Text { anchors.centerIn: parent
               text: { langTick; return qsTr("Done") + " →" }
               color: "#FFFFFF"; font.pixelSize: 28; font.weight: Font.ExtraBold }
    }
}
