import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

Rectangle {
    id: countingPage
    objectName: "recycleCountingPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view
    property real plasticTankVolume: 0.45
    property real canTankVolume: 0.35
    property int plasticCount: 0
    property int canCount: 0
    property int totalPoints: plasticCount * 10 + canCount * 15

    // Translation refresh helper
    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // This page has its OWN behavior: if user is silent for 60 s, jump to
    // the summary page (NOT pop). Kept as a local Timer because the action
    // is different from the global idle timeout.
    Timer {
        id: autoSummaryTimer
        interval: 60000
        repeat: false
        running: false
        onTriggered: stackView.push(summaryPageComponent)
    }

    onPlasticCountChanged: { Idle.touch(); autoSummaryTimer.restart() }
    onCanCountChanged:     { Idle.touch(); autoSummaryTimer.restart() }

    Component.onCompleted: { Idle.touch(); autoSummaryTimer.start() }
    StackView.onActivated: { Idle.touch(); autoSummaryTimer.restart() }
    Component.onDestruction: autoSummaryTimer.stop()

    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: { Idle.touch(); autoSummaryTimer.restart() }
    }

    // Header
    Column {
        id: header
        anchors.top: parent.top
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        Text {
            text: "ReWinGo"
            color: "#1F2A1B"
            font.pixelSize: 86
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: { langTick; return qsTr("Items detected — keep adding!") }
            color: "#5A6B52"
            font.pixelSize: 24
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // Content
    Column {
        width: parent.width * 0.86
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: header.bottom
        anchors.topMargin: 50
        spacing: 34

        CountCard {
            width: parent.width
            height: 360
            title: { langTick; return qsTr("Plastic Bottles") }
            pointsEach: 10
            count: countingPage.plasticCount
            iconSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/plastic-bottle.png"
            onIncrement: countingPage.plasticCount++
            onDecrement: if (countingPage.plasticCount > 0) countingPage.plasticCount--
        }

        CountCard {
            width: parent.width
            height: 360
            title: { langTick; return qsTr("Aluminum Cans") }
            pointsEach: 15
            count: countingPage.canCount
            iconSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/soda-can.png"
            onIncrement: countingPage.canCount++
            onDecrement: if (countingPage.canCount > 0) countingPage.canCount--
        }

        // Points + Done row
        Row {
            width: parent.width
            height: 110
            spacing: 30

            // Points display
            Item {
                width: (parent.width - 30) * 0.56
                height: parent.height

                Rectangle {
                    anchors.fill: pointsBox
                    anchors.margins: -8
                    radius: 28
                    color: "#000000"
                    opacity: 0.05
                    y: 10
                    z: -1
                }

                Rectangle {
                    id: pointsBox
                    anchors.fill: parent
                    radius: 28
                    color: "#FFFFFF"
                    border.width: 2
                    border.color: "#D8E0CF"

                    Row {
                        anchors.centerIn: parent
                        spacing: 14

                        Text {
                            text: { langTick; return qsTr("Total Points:") }
                            color: "#5A6B52"
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: countingPage.totalPoints
                            color: "#0891B2"
                            font.pixelSize: 50
                            font.weight: Font.Black
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // Done button
            Item {
                width: (parent.width - 30) * 0.44
                height: parent.height

                Rectangle {
                    anchors.fill: doneBtn
                    anchors.margins: -8
                    radius: 28
                    color: "#000000"
                    opacity: 0.16
                    y: 10
                    z: -1
                }

                Rectangle {
                    id: doneBtn
                    anchors.fill: parent
                    radius: 28
                    color: "#1A1D1A"

                    TapHandler {
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                        onTapped: {
                            autoSummaryTimer.stop()
                            stackView.push(summaryPageComponent)
                        }
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            text: { langTick; return qsTr("Done") }
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
        }
    }

    Component {
        id: summaryPageComponent
        RecycleSummaryPage {
            plasticCount: countingPage.plasticCount
            canCount: countingPage.canCount
            totalPoints: countingPage.totalPoints
        }
    }

    // ===== CountCard component =====
    component CountCard: Item {
        id: countCard
        property string title: ""
        property int count: 0
        property int pointsEach: 10
        property string iconSource: ""
        signal increment()
        signal decrement()

        // Drop shadow
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

            Row {
                anchors.fill: parent
                anchors.leftMargin: 50
                anchors.rightMargin: 30
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                spacing: 30

                // Icon
                Item {
                    width: 200
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        anchors.centerIn: parent
                        width: 200
                        height: 200
                        radius: 100
                        color: "#E8EEDB"
                        border.color: "#7A8B6A"
                        border.width: 2

                        Image {
                            anchors.centerIn: parent
                            width: 110
                            height: 160
                            source: countCard.iconSource
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }
                    }
                }

                // Label + pts
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    width: 280

                    Text {
                        text: countCard.title
                        color: "#1F2A1B"
                        font.pixelSize: 40
                        font.weight: Font.ExtraBold
                    }

                    Text {
                        text: countCard.pointsEach + " " + qsTr("pts each")
                        color: "#5A6B52"
                        font.pixelSize: 20
                    }
                }

                // − / count / +
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 24

                    Rectangle {
                        width: 80
                        height: 80
                        radius: 40
                        color: "#E8EEDB"
                        border.width: 2
                        border.color: "#7A8B6A"
                        anchors.verticalCenter: parent.verticalCenter

                        TapHandler {
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                            onTapped: countCard.decrement()
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "−"
                            font.pixelSize: 42
                            font.weight: Font.Bold
                            color: "#4A5A3F"
                        }
                    }

                    Text {
                        text: countCard.count
                        color: "#1F2A1B"
                        font.pixelSize: 86
                        font.weight: Font.Black
                        width: 130
                        horizontalAlignment: Text.AlignHCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 80
                        height: 80
                        radius: 40
                        color: "#1A1D1A"
                        anchors.verticalCenter: parent.verticalCenter

                        TapHandler {
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                            onTapped: countCard.increment()
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "+"
                            font.pixelSize: 42
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                        }
                    }
                }
            }
        }
    }
}
