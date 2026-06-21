import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * RegistrationDetailsPage — MOBILE FIRST. Collects the phone number and verifies
 * it belongs to an EXISTING registered ReWinGo app account (via
 * MachineLink.verifyMobile → backend `users` lookup) BEFORE the face is scanned.
 * Only a verified number proceeds to FaceEnrollPage; an unregistered number is
 * rejected here. Uses the on-screen virtual keyboard.
 */
Rectangle {
    id: page
    objectName: "registrationDetailsPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    // The number we're verifying (carried to the face page on success).
    property string pendingMobile: ""

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Backend's answer to verifyMobile(). Proceed to the face scan ONLY if the
    // number is already a registered app account.
    Connections {
        target: MachineLink
        function onMobileVerified(phone, exists) {
            if (phone !== page.pendingMobile) return        // stale answer
            verifyTimeout.stop()
            saving.running = false
            if (exists) {
                stackView.replace(
                    "qrc:/Recycle_Vending_Machine_LCD/qml/registration/FaceEnrollPage.qml",
                    { pendingMobile: page.pendingMobile })
            } else {
                err.text = qsTr("This number isn't registered in the ReWinGo app. Create an account in the app first.")
            }
        }
    }

    // Don't hang if the backend never answers (e.g. offline).
    Timer {
        id: verifyTimeout
        interval: 8000; repeat: false
        onTriggered: {
            saving.running = false
            err.text = qsTr("Couldn't reach the server. Check the connection and try again.")
        }
    }

    function proceed() {
        const mob = mobileField.text.trim()
        if (mob.length < 6) { err.text = qsTr("Please enter a valid mobile number"); return }
        page.pendingMobile = mob
        err.text = ""
        saving.running = true
        verifyTimeout.restart()
        MachineLink.verifyMobile(mob)        // → onMobileVerified
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
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 150
        width: 680
        spacing: 22

        Text {
            text: { langTick; return qsTr("Link your account") }
            color: "#1F2A1B"; font.pixelSize: 52; font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: { langTick; return qsTr("Enter your ReWinGo app mobile number — we'll check it, then scan your face") }
            color: "#5A6B52"; font.pixelSize: 22; width: parent.width
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Mobile number
        Column {
            width: parent.width; spacing: 8
            Text { text: { langTick; return qsTr("Your ReWinGo app phone number") }
                   color: "#1F2A1B"; font.pixelSize: 22; font.weight: Font.DemiBold }
            Text { text: { langTick; return qsTr("Enter the SAME phone number you used to create your account in the ReWinGo app — this links your face to that account so your points are saved.") }
                   color: "#5A6B52"; font.pixelSize: 15; wrapMode: Text.WordWrap
                   width: parent.width }
            Rectangle {
                width: parent.width; height: 92; radius: 18
                color: "#FFFFFF"; border.width: 2
                border.color: mobileField.activeFocus ? "#0891B2" : "#D8E0CF"
                TextField {
                    id: mobileField
                    anchors.fill: parent
                    anchors.margins: 6
                    font.pixelSize: 30
                    verticalAlignment: TextInput.AlignVCenter
                    leftPadding: 18
                    placeholderText: qsTr("e.g. 01xxxxxxxxx")
                    background: Item {}
                    inputMethodHints: Qt.ImhDigitsOnly
                    onAccepted: page.proceed()
                }
            }
        }

        Text {
            id: err; text: ""
            color: "#DC2626"; font.pixelSize: 18
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Continue
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 320; height: 96; radius: 48
            color: "#1A1D1A"
            opacity: saving.running ? 0.6 : 1.0
            TapHandler { enabled: !saving.running; onTapped: page.proceed() }
            Row {
                anchors.centerIn: parent; spacing: 10
                visible: !saving.running
                Text { text: { langTick; return qsTr("Continue") }
                       color: "#FFFFFF"; font.pixelSize: 28; font.weight: Font.ExtraBold }
                Text { text: "→"; color: "#00E5FF"; font.pixelSize: 28; font.weight: Font.Black }
            }
            BusyIndicator {
                id: saving
                anchors.centerIn: parent
                running: false
                visible: running
            }
        }
    }
}
