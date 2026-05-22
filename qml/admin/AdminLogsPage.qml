import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

Rectangle {
    id: page
    color: "#EFF3EA"
    property StackView stackView: StackView.view

    Component.onCompleted: { LogsViewer.refresh(); Idle.disable() }
    StackView.onActivated: Idle.disable()

    // ════════════ HEADER ════════════
    Rectangle {
        id: headerBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 160
        color: "#1F2A1B"

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 30
            width: 80; height: 80; radius: 40
            color: "#FFFFFF"
            TapHandler { onTapped: stackView.pop() }
            Text { anchors.centerIn: parent; text: "←"
                   font.pixelSize: 36; color: "#1F2A1B" }
        }
        Column {
            anchors.centerIn: parent
            spacing: 4
            Text { text: qsTr("Logs")
                   color: "#FFFFFF"
                   font.pixelSize: 50; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("System events and maintenance")
                   color: "#A5F3FC"
                   font.pixelSize: 18
                   anchors.horizontalCenter: parent.horizontalCenter }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 30
            spacing: 10
            Rectangle {
                width: 80; height: 80; radius: 40
                color: "transparent"
                border.width: 2; border.color: "#A5F3FC"
                TapHandler { onTapped: LogsViewer.refresh() }
                Text { anchors.centerIn: parent; text: "↻"
                       color: "#A5F3FC"; font.pixelSize: 36; font.weight: Font.Black }
            }
        }
    }

    // ════════════ ACTIONS ROW ════════════
    Row {
        id: actions
        anchors.top: headerBar.bottom
        anchors.topMargin: 18
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        Rectangle {
            width: 240; height: 64; radius: 32
            color: "#92400E"
            TapHandler { onTapped: LogsViewer.deleteOlderThan(30) }
            Text { anchors.centerIn: parent
                   text: qsTr("Delete > 30 d")
                   color: "#FFFFFF"
                   font.pixelSize: 18; font.weight: Font.ExtraBold }
        }
        Rectangle {
            width: 240; height: 64; radius: 32
            color: "#DC2626"
            TapHandler { onTapped: confirmWipe.open() }
            Text { anchors.centerIn: parent
                   text: qsTr("Wipe all")
                   color: "#FFFFFF"
                   font.pixelSize: 18; font.weight: Font.ExtraBold }
        }
    }

    // ════════════ LOG TABLE ════════════
    Rectangle {
        anchors.top: actions.bottom
        anchors.topMargin: 20
        anchors.bottom: parent.bottom; anchors.bottomMargin: 30
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 48
        radius: 24
        color: "#FFFFFF"
        border.width: 2; border.color: "#D8E0CF"

        ScrollView {
            anchors.fill: parent
            anchors.margins: 16
            clip: true
            ListView {
                model: LogsViewer.lines
                spacing: 2
                delegate: Text {
                    text: modelData
                    color: modelData.indexOf("[ERROR]") >= 0 ? "#DC2626" :
                           modelData.indexOf("[WARN]")  >= 0 ? "#92400E" :
                           modelData.indexOf("[AUDIT]") >= 0 ? "#0891B2" : "#1F2A1B"
                    font.pixelSize: 13
                    font.family: "Courier"
                    wrapMode: Text.NoWrap
                }
            }
        }
    }

    Dialog {
        id: confirmWipe
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        Overlay.modal: Rectangle { color: "#A0000000" }
        title: qsTr("Wipe ALL local data?")
        standardButtons: Dialog.Yes | Dialog.No
        Text {
            text: qsTr("This deletes local logs and SQLite rows. " +
                       "Data already synced to the cloud is NOT affected.")
            wrapMode: Text.WordWrap
            width: 460
            color: "#1F2A1B"
            font.pixelSize: 15
        }
        onAccepted: LogsViewer.wipeAll()
    }
}
