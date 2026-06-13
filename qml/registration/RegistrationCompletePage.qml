import QtQuick
import QtQuick.Controls

Rectangle {
    id: page
    color: "#F2F4ED"
    property StackView stackView: StackView.view
    property string userId: ""
    property string userName: ""
    property string userMobile: ""

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    Column {
        anchors.centerIn: parent
        spacing: 20
        Text { text: "✓"; font.pixelSize: 200; color: "#16A34A"
               anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: { langTick; return qsTr("All done!") }
               color: "#1F2A1B"
               font.pixelSize: 56; font.weight: Font.Black
               anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: { langTick; return qsTr("You can now use face login on your next visit.") }
               color: "#5A6B52"; font.pixelSize: 22
               anchors.horizontalCenter: parent.horizontalCenter }
    }

    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 70
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 18

        // OPTIONAL: connect the phone app now (shows a claim QR). Not required.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 460; height: 96; radius: 48
            color: "#0891B2"
            scale: connectTap.pressed ? 0.96 : 1.0
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
            TapHandler {
                id: connectTap
                onTapped: stackView.push(
                    "qrc:/Recycle_Vending_Machine_LCD/qml/registration/ClaimQrPage.qml",
                    { userName: page.userName, userMobile: page.userMobile })
            }
            Row {
                anchors.centerIn: parent; spacing: 12
                Text { text: "📱"; font.pixelSize: 32
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: { langTick; return qsTr("Connect the mobile app") }
                       color: "#FFFFFF"; font.pixelSize: 26; font.weight: Font.ExtraBold
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // Finish without connecting — totally fine.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 320; height: 84; radius: 42
            color: "#1A1D1A"
            scale: doneTap.pressed ? 0.96 : 1.0
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
            TapHandler {
                id: doneTap
                onTapped: { while (stackView && stackView.depth > 1) stackView.pop() }
            }
            Text { anchors.centerIn: parent
                   text: { langTick; return qsTr("Maybe later") + " →" }
                   color: "#FFFFFF"; font.pixelSize: 26; font.weight: Font.ExtraBold }
        }
    }
}
