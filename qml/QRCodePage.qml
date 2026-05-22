import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

Rectangle {
    id: qrPage
    objectName: "qrCodePage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view
    property real plasticTankVolume: 0.45
    property real canTankVolume: 0.35
    property int plasticCount: 0
    property int canCount: 0
    property int totalPoints: 0

    // Translation refresh helper
    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Idle is centralised in IdleManager — no local timer here.
    Component.onCompleted: Idle.touch()
    StackView.onActivated: Idle.touch()
    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: Idle.touch()
    }

    // Back button
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 30
        anchors.leftMargin: 30
        width: 90
        height: 90
        radius: 45
        color: "#FFFFFF"
        border.width: 2
        border.color: "#D8E0CF"
        z: 10

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: {
                stackView.pop()
            }
        }

        Text {
            anchors.centerIn: parent
            text: "←"
            font.pixelSize: 36
            color: "#1F2A1B"
        }
    }

    // Exit button
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 30
        anchors.rightMargin: 30
        width: 90
        height: 90
        radius: 45
        color: "#FFFFFF"
        border.width: 2
        border.color: "#D8E0CF"
        z: 10

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: {
                while (stackView && stackView.depth > 1) stackView.pop()
            }
        }

        Text {
            anchors.centerIn: parent
            text: "✕"
            font.pixelSize: 32
            font.weight: Font.Black
            color: "#1F2A1B"
        }
    }

    // Header
    Column {
        id: header
        anchors.top: parent.top
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        Text {
            text: { langTick; return qsTr("Your Recycling Summary") }
            color: "#1F2A1B"
            font.pixelSize: 76
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: { langTick; return qsTr("Scan the QR code to claim your points") }
            color: "#5A6B52"
            font.pixelSize: 22
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // Main content row
    Row {
        anchors.top: header.bottom
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 40

        // QR card
        Item {
            width: 380
            height: 480

            Rectangle {
                anchors.fill: qrCard
                anchors.margins: -14
                radius: 40
                color: "#000000"
                opacity: 0.06
                y: 16
                z: -1
            }

            Rectangle {
                id: qrCard
                anchors.fill: parent
                radius: 40
                color: "#FFFFFF"
                border.width: 2
                border.color: "#D8E0CF"

                Column {
                    anchors.centerIn: parent
                    spacing: 18

                    Text {
                        text: { langTick; return qsTr("Scan to Claim Points") }
                        color: "#0891B2"
                        font.pixelSize: 22
                        font.weight: Font.ExtraBold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Rectangle {
                        width: 280
                        height: 280
                        radius: 16
                        color: "#FFFFFF"
                        border.width: 3
                        border.color: "#7A8B6A"
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            anchors.centerIn: parent
                            width: 240
                            height: 240
                            color: "#E8EEDB"

                            Text {
                                anchors.centerIn: parent
                                text: "QR\nCODE"
                                font.pixelSize: 42
                                font.weight: Font.Black
                                color: "#1F2A1B"
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }
            }
        }

        // Summary card
        Item {
            width: 460
            height: 480

            Rectangle {
                anchors.fill: summaryCard
                anchors.margins: -14
                radius: 40
                color: "#000000"
                opacity: 0.06
                y: 16
                z: -1
            }

            Rectangle {
                id: summaryCard
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

                Column {
                    anchors.centerIn: parent
                    spacing: 22

                    Text {
                        text: { langTick; return qsTr("Items Recycled") }
                        color: "#5A6B52"
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Row {
                        spacing: 50
                        anchors.horizontalCenter: parent.horizontalCenter

                        Column {
                            spacing: 4
                            Text {
                                text: qrPage.plasticCount
                                color: "#1F2A1B"
                                font.pixelSize: 64
                                font.weight: Font.Black
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: { langTick; return qsTr("Plastic Bottles") }
                                color: "#5A6B52"
                                font.pixelSize: 18
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        Rectangle {
                            width: 2
                            height: 100
                            color: "#D8E0CF"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            spacing: 4
                            Text {
                                text: qrPage.canCount
                                color: "#1F2A1B"
                                font.pixelSize: 64
                                font.weight: Font.Black
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: { langTick; return qsTr("Aluminum Cans") }
                                color: "#5A6B52"
                                font.pixelSize: 18
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    Rectangle {
                        width: 320
                        height: 2
                        color: "#D8E0CF"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: { langTick; return qsTr("Total Points Earned") }
                        color: "#5A6B52"
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: qrPage.totalPoints
                        color: "#0891B2"
                        font.pixelSize: 72
                        font.weight: Font.Black
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }

    // Back to main button
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        anchors.horizontalCenter: parent.horizontalCenter
        width: 360
        height: 96
        radius: 28
        color: "#1A1D1A"

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: {
                while (stackView && stackView.depth > 1) stackView.pop()
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: 14

            Text {
                text: { langTick; return qsTr("Back to Home") }
                color: "#FFFFFF"
                font.pixelSize: 30
                font.weight: Font.ExtraBold
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "→"
                color: "#00E5FF"
                font.pixelSize: 30
                font.weight: Font.Black
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
