import QtQuick
import QtQuick.Controls
import QtMultimedia

Item {
    id: ads
    objectName: "advertisePage"

    property StackView stackView: StackView.view

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#F2F4ED" }
            GradientStop { position: 1.0; color: "#E8EEDB" }
        }
    }

    // Soft sage blob
    Rectangle {
        id: blob1
        width: 900; height: 900
        radius: width / 2
        color: "#D8E0CF"
        opacity: 0.55
        x: -320
        y: 200
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { to: 250; duration: 5200; easing.type: Easing.InOutSine }
            NumberAnimation { to: 200; duration: 5200; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        id: blob2
        width: 760; height: 760
        radius: width / 2
        color: "#A5F3FC"
        opacity: 0.20
        x: parent.width - width + 220
        y: parent.height - height + 120
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { to: parent.height - height + 80;  duration: 6000; easing.type: Easing.InOutSine }
            NumberAnimation { to: parent.height - height + 120; duration: 6000; easing.type: Easing.InOutSine }
        }
    }

    // Video / banner card
    Item {
        id: videoCardWrap
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 120
        width: parent.width * 0.90
        height: 980

        // Drop shadow
        Rectangle {
            anchors.fill: videoCard
            anchors.margins: -14
            radius: videoCard.radius + 14
            color: "#000000"
            opacity: 0.06
            y: 16
            z: -1
        }

        Rectangle {
            id: videoCard
            anchors.fill: parent
            radius: 36
            color: "#FFFFFF"
            border.width: 2
            border.color: "#D8E0CF"

            MediaPlayer {
                id: player
                source: "qrc:/qt/qml/Recycle_Vending_Machine_LCD/resources/assets/ads.jpg"
                loops: MediaPlayer.Infinite
                autoPlay: true
                onErrorOccurred: (err, msg) => console.log("[ADS] Video error:", err, msg)
            }

            VideoOutput {
                anchors.fill: parent
                anchors.margins: 12
                source: player
                fillMode: VideoOutput.PreserveAspectCrop
            }

            Text {
                visible: player.error !== MediaPlayer.NoError
                anchors.centerIn: parent
                text: { langTick; return qsTr("VIDEO MISSING / ERROR") }
                color: "#92400E"
                font.pixelSize: 28
                font.bold: true
            }
        }
    }

    // Brand + Press to start
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 140
        spacing: 18

        Text {
            text: { langTick; return qsTr("ReWinGo") }
            color: "#1F2A1B"
            font.pixelSize: 84
            font.weight: Font.Black
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Item {
            width: 820
            height: 120
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                radius: 28
                color: "#A5F3FC"
                opacity: 0.55
            }

            Text {
                id: pressText
                anchors.centerIn: parent
                text: { langTick; return qsTr("PRESS TO START") + "  •  اضغط للبدء" }
                color: "#1F2A1B"
                font.pixelSize: 44
                font.weight: Font.Black
                style: Text.Outline
                styleColor: "#00E5FF"
                opacity: 1.0
                scale: 1.0

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.55; duration: 650; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.0;  duration: 650; easing.type: Easing.InOutQuad }
                }
                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.03; duration: 650; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.00; duration: 650; easing.type: Easing.InOutQuad }
                }
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
