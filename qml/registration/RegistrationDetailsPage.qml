import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * RegistrationDetailsPage — collects the new user's name + mobile number
 * before the face capture. The name is stored with the face so login greets
 * the real person ("Welcome <name>"); the mobile number keys the temp account
 * the phone app later claims. Uses the on-screen virtual keyboard.
 */
Rectangle {
    id: page
    objectName: "registrationDetailsPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    function proceed() {
        const nm = nameField.text.trim()
        if (nm.length === 0) { err.text = qsTr("Please enter your name"); return }
        const mob = mobileField.text.trim()
        if (mob.length < 6)  { err.text = qsTr("Please enter a valid mobile number"); return }
        stackView.replace(
            "qrc:/Recycle_Vending_Machine_LCD/qml/registration/FaceEnrollPage.qml",
            { newUserName: nm, newUserMobile: mob })
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
            text: { langTick; return qsTr("Create your account") }
            color: "#1F2A1B"; font.pixelSize: 52; font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: { langTick; return qsTr("Enter your details, then we'll scan your face") }
            color: "#5A6B52"; font.pixelSize: 22
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Name
        Column {
            width: parent.width; spacing: 8
            Text { text: { langTick; return qsTr("Name") }
                   color: "#1F2A1B"; font.pixelSize: 22; font.weight: Font.DemiBold }
            Rectangle {
                width: parent.width; height: 92; radius: 18
                color: "#FFFFFF"; border.width: 2
                border.color: nameField.activeFocus ? "#0891B2" : "#D8E0CF"
                TextField {
                    id: nameField
                    anchors.fill: parent
                    anchors.margins: 6
                    font.pixelSize: 30
                    verticalAlignment: TextInput.AlignVCenter
                    leftPadding: 18
                    placeholderText: qsTr("Your name")
                    background: Item {}
                    inputMethodHints: Qt.ImhNoPredictiveText
                    onAccepted: mobileField.forceActiveFocus()
                }
            }
        }

        // Mobile number
        Column {
            width: parent.width; spacing: 8
            Text { text: { langTick; return qsTr("Mobile number") }
                   color: "#1F2A1B"; font.pixelSize: 22; font.weight: Font.DemiBold }
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
            TapHandler { onTapped: page.proceed() }
            Row {
                anchors.centerIn: parent; spacing: 10
                Text { text: { langTick; return qsTr("Continue") }
                       color: "#FFFFFF"; font.pixelSize: 28; font.weight: Font.ExtraBold }
                Text { text: "→"; color: "#00E5FF"; font.pixelSize: 28; font.weight: Font.Black }
            }
        }
    }
}
