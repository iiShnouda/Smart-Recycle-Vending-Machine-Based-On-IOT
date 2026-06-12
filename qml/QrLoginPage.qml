import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * QrLoginPage — Discord-style QR sign-in. Shows the privacy warning + a QR the
 * user scans with the ReWinGo phone app. The app tells the backend, which
 * relays the linked user to this kiosk via MachineLink -> we log them in.
 */
Rectangle {
    id: page
    objectName: "qrLoginPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    property int langTick: 0
    Connections { target: appManager; function onLanguageChanged() { langTick++ } }

    property int previewTick: 0
    Timer { interval: 400; running: true; repeat: true; onTriggered: page.previewTick++ }

    // 1-minute timeout — if nobody scans, leave the QR page (the token dies).
    Timer {
        interval: 60000; running: true; repeat: false
        onTriggered: { MachineLink.cancel(); if (stackView) stackView.pop() }
    }

    Component.onCompleted: { Idle.disable(); MachineLink.beginQrSession() }
    Component.onDestruction: MachineLink.cancel()
    StackView.onActivated:   Idle.disable()

    Connections {
        target: MachineLink
        function onLoginReceived(userId, name, points) {
            Idle.touch()
            stackView.replace("qrc:/Recycle_Vending_Machine_LCD/qml/MainPage.qml",
                              { userName: name, userId: userId })
        }
    }

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
        spacing: 6
        Text { text: { langTick; return qsTr("Scan to sign in") }
               color: "#1F2A1B"; font.pixelSize: 52; font.weight: Font.Black
               anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: { langTick; return qsTr("Open the ReWinGo app and scan this code") }
               color: "#5A6B52"; font.pixelSize: 20
               anchors.horizontalCenter: parent.horizontalCenter }
    }

    // Privacy warning (same message as the consent page)
    Rectangle {
        id: warn
        anchors.top: parent.top; anchors.topMargin: 150
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.72; height: 76; radius: 16
        color: "#FEF3C7"; border.width: 2; border.color: "#FBBF24"
        Text {
            anchors.centerIn: parent; width: parent.width - 32
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: { langTick; return qsTr("By scanning, you agree to link this machine to your ReWinGo account and share your name & points.") }
            color: "#92400E"; font.pixelSize: 15
        }
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

    // Status footer
    Text {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 80
        anchors.horizontalCenter: parent.horizontalCenter
        horizontalAlignment: Text.AlignHCenter
        text: {
            langTick
            if (MachineLink.state === "linked") return qsTr("Signing you in…")
            return MachineLink.connected ? qsTr("Waiting for you to scan…")
                                         : qsTr("Connecting…")
        }
        color: MachineLink.state === "linked" ? "#16A34A" : "#5A6B52"
        font.pixelSize: 18; font.weight: Font.DemiBold

        SequentialAnimation on opacity {
            running: MachineLink.state !== "linked"
            loops: Animation.Infinite
            NumberAnimation { to: 0.45; duration: 1000 }
            NumberAnimation { to: 1.0;  duration: 1000 }
        }
    }
}
