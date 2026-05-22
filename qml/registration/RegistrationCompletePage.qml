import QtQuick
import QtQuick.Controls

Rectangle {
    id: page
    color: "#F2F4ED"
    property StackView stackView: StackView.view
    property string userId: ""

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

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 70
        anchors.horizontalCenter: parent.horizontalCenter
        width: 320; height: 96; radius: 48
        color: "#1A1D1A"
        TapHandler {
            onTapped: {
                while (stackView && stackView.depth > 1) stackView.pop()
            }
        }
        Text { anchors.centerIn: parent
               text: { langTick; return qsTr("Continue") + " →" }
               color: "#FFFFFF"; font.pixelSize: 28; font.weight: Font.ExtraBold }
    }
}
