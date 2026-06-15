import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

Rectangle {
    id: summaryPage
    objectName: "recycleSummaryPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view
    property int plasticCount: 0
    property int canCount: 0
    property int rejectedCount: 0
    property int totalPoints: 0

    // Translation refresh helper
    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Return to the user's Main page (the menu) WITHOUT logging out — pops back
    // to the existing MainPage in the stack, keeping the session. Falls back to
    // the start screen if MainPage isn't on the stack for some reason.
    function goMain() {
        var mp = stackView.find(function(item) { return item.objectName === "mainPage" })
        if (mp) stackView.pop(mp)
        else while (stackView && stackView.depth > 1) stackView.pop()
    }

    // Header
    Column {
        id: header
        anchors.top: parent.top
        anchors.topMargin: 60
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        Text {
            text: { langTick; return qsTr("Well Done!") }
            color: "#1F2A1B"
            font.pixelSize: 86
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: { langTick; return qsTr("Here's a summary of your recycling session") }
            color: "#5A6B52"
            font.pixelSize: 22
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: { langTick; return qsTr("Thank you for recycling with ReWinGo 💚") }
            color: "#0891B2"
            font.pixelSize: 24
            font.weight: Font.DemiBold
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: 6
        }
    }

    Column {
        width: parent.width * 0.86
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: header.bottom
        anchors.topMargin: 60
        spacing: 34

        // Summary card
        Item {
            width: parent.width
            height: 480

            // Drop shadow
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

                // Sage accent line
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
                    spacing: 40

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 80

                        Column {
                            spacing: 8
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                text: summaryPage.plasticCount
                                color: "#1F2A1B"
                                font.pixelSize: 86
                                font.weight: Font.Black
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: { langTick; return qsTr("Plastic Bottles") }
                                color: "#5A6B52"
                                font.pixelSize: 22
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                // Bottles mix small (1 pt) + large (2 pts); derive the
                                // bottle contribution from the accurate session total.
                                text: (summaryPage.totalPoints
                                       - summaryPage.canCount * RecycleSession.canPoints)
                                      + " " + qsTr("pts")
                                color: "#0891B2"
                                font.pixelSize: 20
                                font.weight: Font.DemiBold
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        Rectangle {
                            width: 2
                            height: 150
                            color: "#D8E0CF"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            spacing: 8
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                text: summaryPage.canCount
                                color: "#1F2A1B"
                                font.pixelSize: 86
                                font.weight: Font.Black
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: { langTick; return qsTr("Aluminum Cans") }
                                color: "#5A6B52"
                                font.pixelSize: 22
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: summaryPage.canCount * RecycleSession.canPoints + " " + qsTr("pts")
                                color: "#0891B2"
                                font.pixelSize: 20
                                font.weight: Font.DemiBold
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    Rectangle {
                        width: 500
                        height: 2
                        color: "#D8E0CF"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 4

                        Text {
                            text: { langTick; return qsTr("Total Points Earned") }
                            color: "#5A6B52"
                            font.pixelSize: 22
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: summaryPage.totalPoints
                            color: "#1F2A1B"
                            font.pixelSize: 86
                            font.weight: Font.Black
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // EGP equivalent — 1 point = 0.4 EGP.
                        Text {
                            text: { langTick; return "≈ " +
                                    (summaryPage.totalPoints * RecycleSession.pointValueEGP).toFixed(2)
                                    + " " + qsTr("EGP") }
                            color: "#16A34A"
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }

        // Action buttons
        Row {
            width: parent.width
            height: 120
            spacing: 30

            // Finish
            Item {
                width: (parent.width - 30) / 2
                height: parent.height

                Rectangle {
                    anchors.fill: finishCard
                    anchors.margins: -10
                    radius: 32
                    color: "#000000"
                    opacity: 0.05
                    y: 10
                    z: -1
                }

                Rectangle {
                    id: finishCard
                    anchors.fill: parent
                    radius: 28
                    color: "#FFFFFF"
                    border.width: 2
                    border.color: "#7A8B6A"

                    scale: finishTap.pressed ? 0.95 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }

                    TapHandler {
                        id: finishTap
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                        onTapped: {
                            while (stackView && stackView.depth > 1) stackView.pop()
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            text: { langTick; return qsTr("Finish") }
                            color: "#1F2A1B"
                            font.pixelSize: 34
                            font.weight: Font.ExtraBold
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: { langTick; return qsTr("Return to home") }
                            color: "#5A6B52"
                            font.pixelSize: 18
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }

            // Buy from Vending
            Item {
                width: (parent.width - 30) / 2
                height: parent.height

                Rectangle {
                    anchors.fill: vendingCard
                    anchors.margins: -10
                    radius: 32
                    color: "#000000"
                    opacity: 0.18
                    y: 10
                    z: -1
                }

                Rectangle {
                    id: vendingCard
                    anchors.fill: parent
                    radius: 28
                    color: "#1A1D1A"

                    scale: vendingTap.pressed ? 0.95 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }

                    TapHandler {
                        id: vendingTap
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                        onTapped: stackView.push(Qt.resolvedUrl("VendingPage.qml"))
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            text: { langTick; return qsTr("Buy from Vending") }
                            color: "#FFFFFF"
                            font.pixelSize: 34
                            font.weight: Font.ExtraBold
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: { langTick; return qsTr("Spend your") + " " + summaryPage.totalPoints + " " + qsTr("points") }
                            color: "#00E5FF"
                            font.pixelSize: 18
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }

        // Back to the Main menu (stay logged in for another action)
        Rectangle {
            width: parent.width
            height: 104
            radius: 28
            color: "#0891B2"
            scale: mainTap.pressed ? 0.96 : 1.0
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
            TapHandler {
                id: mainTap
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                onTapped: summaryPage.goMain()
            }
            Row {
                anchors.centerIn: parent; spacing: 12
                Text { text: "⌂"; color: "#FFFFFF"; font.pixelSize: 34; font.weight: Font.Black
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: { langTick; return qsTr("Back to Main Menu") }
                       color: "#FFFFFF"; font.pixelSize: 32; font.weight: Font.ExtraBold
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }
}
