import QtQuick
import QtQuick.Controls

Item {
    id: startPage
    objectName: "startPage"

    property StackView stackView: StackView.view

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Solid sage backdrop
    Rectangle {
        anchors.fill: parent
        color: "#F2F4ED"
    }

    Column {
        anchors.centerIn: parent
        spacing: 24

        Text {
            text: "ReWinGo"
            color: "#1F2A1B"
            font.pixelSize: 110
            font.weight: Font.Black
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: "Recycle. Win. Go."
            color: "#5A6B52"
            font.pixelSize: 30
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Item { width: 1; height: 40 }

        Text {
            id: pressText
            text: "PRESS TO START  •  اضغط للبدء"
            color: "#0891B2"
            font.pixelSize: 56
            font.weight: Font.Black
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            opacity: 1.0

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 0.35; duration: 2200; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.00; duration: 2200; easing.type: Easing.InOutQuad }
            }
        }
    }
    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: {
            if (!stackView) stackView = StackView.view
            if (stackView) stackView.push(Qt.resolvedUrl("LanguageSelectionPage.qml"))
        }
    }
}
