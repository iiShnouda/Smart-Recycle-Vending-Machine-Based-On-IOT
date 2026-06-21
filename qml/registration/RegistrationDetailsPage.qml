import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * RegistrationDetailsPage — collects the new user's name + mobile number
 * AFTER the face capture (name-after-face). The face is already enrolled with
 * a placeholder name; here we write the real name/mobile onto that DB row via
 * FaceRec.finalizeUser(userId, …) so login greets the real person
 * ("Welcome <name>") and the mobile keys the temp account the phone app claims.
 * Uses the on-screen virtual keyboard.
 */
Rectangle {
    id: page
    objectName: "registrationDetailsPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    // The user_id returned by the just-completed face enrollment.
    property string newUserId: ""
    // Held while we wait for finalizeUser to write the row.
    property string pendingName: ""
    property string pendingMobile: ""

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // finalizeUser always answers (finalized on success, failed on error). In
    // either case the face is already enrolled, so we continue to the success
    // page — at worst the stored name stays the placeholder.
    Connections {
        target: FaceRec
        function onFinalized(userId) { page.goComplete() }
        function onFailed(reason)    { page.goComplete() }
    }

    function goComplete() {
        stackView.replace(
            "qrc:/Recycle_Vending_Machine_LCD/qml/registration/RegistrationCompletePage.qml",
            { userId: page.newUserId, userName: page.pendingName,
              userMobile: page.pendingMobile })
    }

    function proceed() {
        // Mobile-only registration — the phone number IS the account key (the
        // real name comes from the ReWinGo app when the account is claimed).
        const mob = mobileField.text.trim()
        if (mob.length < 6)  { err.text = qsTr("Please enter a valid mobile number"); return }
        page.pendingName = mob        // identify the row by mobile until the app claims it
        page.pendingMobile = mob
        err.text = ""
        saving.running = true
        FaceRec.finalizeUser(parseInt(page.newUserId), mob, mob)
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
            text: { langTick; return qsTr("Your face is saved — enter your mobile number to finish") }
            color: "#5A6B52"; font.pixelSize: 22
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
