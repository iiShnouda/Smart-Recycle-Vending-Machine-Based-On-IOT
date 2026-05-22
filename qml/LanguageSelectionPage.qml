import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

Rectangle {
    id: languagePage
    objectName: "languageSelectionPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Idle is global — no per-page timer.
    function bumpIdle() { Idle.touch() }
    Component.onCompleted: {
        appManager.selectLanguage("en")
        Idle.touch()
    }
    StackView.onActivated: Idle.touch()
    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: Idle.touch()
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 110
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12

        Text {
            text: "ReWinGo"
            color: "#1F2A1B"
            font.pixelSize: 88
            font.weight: Font.Black
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: "Choose your language  •  اختر لغتك"
            color: "#5A6B52"
            font.pixelSize: 26
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Column {
        width: parent.width * 0.88
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 80

        LangTile {
            width: parent.width
            height: 380
            emoji: "EN"
            label: "English"
            tapText: "Tap to continue"
            onTapped: {
                bumpIdle()
                appManager.selectLanguage("en")
                if (!stackView) stackView = StackView.view
                stackView.push(Qt.resolvedUrl("AuthChoicePage.qml"))
            }
        }

        LangTile {
            width: parent.width
            height: 380
            emoji: "AR"
            label: "العربية"
            tapText: "انقر للمتابعة"
            onTapped: {
                bumpIdle()
                appManager.selectLanguage("ar")
                if (!stackView) stackView = StackView.view
                stackView.push(Qt.resolvedUrl("AuthChoicePage.qml"))
            }
        }
    }

    component LangTile: Item {
        id: tile
        property string emoji: ""
        property string label: ""
        property string tapText: ""
        signal tapped()

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: tile.tapped()
        }

        Rectangle {
            anchors.fill: card
            anchors.margins: -14
            radius: 40
            color: "#000000"
            opacity: 0.06
            y: 16
            z: -1
        }

        Rectangle {
            id: card
            anchors.fill: parent
            radius: 40
            color: "#FFFFFF"
            border.width: 2
            border.color: "#D8E0CF"

            Rectangle {
                width: 8
                radius: 4
                color: "#7A8B6A"
                opacity: 0.95
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 18
                anchors.bottomMargin: 18
            }

            Row {
                anchors.fill: parent
                anchors.margins: 30
                anchors.leftMargin: 50
                spacing: 30
                LayoutMirroring.enabled: false

                Rectangle {
                    width: 220
                    height: 220
                    radius: 36
                    color: "#E8EEDB"
                    border.width: 2
                    border.color: "#7A8B6A"
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: tile.emoji
                        font.pixelSize: 80
                        font.weight: Font.Black
                        color: "#1F2A1B"
                    }
                }

                Item {
                    width: parent.width - 220 - 30 - 50
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter

                    Column {
                        anchors.centerIn: parent
                        spacing: 14

                        Text {
                            text: tile.label
                            color: "#1F2A1B"
                            font.pixelSize: 64
                            font.weight: Font.ExtraBold
                            anchors.horizontalCenter: parent.horizontalCenter
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            text: tile.tapText
                            color: "#0891B2"
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                            anchors.horizontalCenter: parent.horizontalCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
