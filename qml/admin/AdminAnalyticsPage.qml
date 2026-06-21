import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

Rectangle {
    id: page
    color: "#EFF3EA"
    property StackView stackView: StackView.view

    Component.onCompleted: { Analytics.refresh(); Idle.disable() }
    StackView.onActivated: { Idle.disable(); Analytics.refresh() }

    // Auto-refresh while the page is open — no need to tap the ↻ button.
    Timer { interval: 8000; repeat: true; running: true; onTriggered: Analytics.refresh() }

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
            Text { text: qsTr("Analytics")
                   color: "#FFFFFF"
                   font.pixelSize: 50; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Live data from the kiosk database")
                   color: "#A5F3FC"
                   font.pixelSize: 18
                   anchors.horizontalCenter: parent.horizontalCenter }
        }
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 30
            width: 80; height: 80; radius: 40
            color: refreshTap.pressed ? "#0E7490" : "transparent"
            border.width: 2; border.color: "#A5F3FC"
            scale: refreshTap.pressed ? 0.9 : 1.0
            Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }
            TapHandler { id: refreshTap; onTapped: { spinAnim.restart(); Analytics.refresh() } }
            Text { id: refreshIcon; anchors.centerIn: parent; text: "↻"
                   color: "#A5F3FC"; font.pixelSize: 36; font.weight: Font.Black
                   RotationAnimation { id: spinAnim; target: refreshIcon; from: 0; to: 360
                                       duration: 500; easing.type: Easing.OutCubic; running: false } }
        }
    }

    // ════════════ KPI STRIP ════════════
    Row {
        id: kpis
        anchors.top: headerBar.bottom
        anchors.topMargin: 30
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        component Kpi : Rectangle {
            property string label
            property string value
            property color  colour: "#0891B2"
            // Sized for 1080-wide kiosk: 4 × 220 + 3 × 14 spacing = 922
            width: 220; height: 150; radius: 24
            color: "#FFFFFF"
            border.width: 2; border.color: "#D8E0CF"
            Column {
                anchors.centerIn: parent
                spacing: 8
                Text { text: parent.parent.value
                       color: parent.parent.colour
                       font.pixelSize: 50; font.weight: Font.Black
                       anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: parent.parent.label
                       color: "#5A6B52"; font.pixelSize: 15
                       font.weight: Font.DemiBold
                       anchors.horizontalCenter: parent.horizontalCenter }
            }
        }

        Kpi { label: qsTr("Recycles today");  value: Analytics.recyclesToday;
              colour: "#16A34A" }
        Kpi { label: qsTr("Vendings today");  value: Analytics.vendingsToday;
              colour: "#0891B2" }
        Kpi { label: qsTr("Total users");     value: Analytics.totalUsers;
              colour: "#1F2A1B" }
        Kpi { label: qsTr("Points spent");    value: Analytics.pointsSpentToday;
              colour: "#92400E" }
    }

    // ════════════ RECENT TX TABLE ════════════
    Rectangle {
        anchors.top: kpis.bottom
        anchors.topMargin: 30
        anchors.bottom: parent.bottom; anchors.bottomMargin: 30
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 48
        radius: 24
        color: "#FFFFFF"
        border.width: 2; border.color: "#D8E0CF"

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10

            Row {
                spacing: 14; width: parent.width
                Text { text: qsTr("Recent transactions")
                       color: "#1F2A1B"
                       font.pixelSize: 22; font.weight: Font.ExtraBold }
            }

            // Column headers
            Row {
                width: parent.width
                spacing: 14
                Text { text: qsTr("Time");  color: "#5A6B52"
                       width: 160; font.pixelSize: 14; font.weight: Font.DemiBold }
                Text { text: qsTr("Kind");  color: "#5A6B52"
                       width: 110; font.pixelSize: 14; font.weight: Font.DemiBold }
                Text { text: qsTr("Slot");  color: "#5A6B52"
                       width: 60;  font.pixelSize: 14; font.weight: Font.DemiBold }
                Text { text: qsTr("Amount"); color: "#5A6B52"
                       width: 90;  font.pixelSize: 14; font.weight: Font.DemiBold }
                Text { text: qsTr("User");  color: "#5A6B52"
                       width: 220; font.pixelSize: 14; font.weight: Font.DemiBold }
            }
            Rectangle { width: parent.width; height: 1; color: "#D8E0CF" }

            Flickable {
                width: parent.width
                height: parent.height - 90
                contentWidth: width
                contentHeight: rowsCol.height
                clip: true
                Column {
                    id: rowsCol
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: Analytics.recent
                        delegate: Rectangle {
                            width: rowsCol.width
                            height: 36
                            radius: 8
                            color: index % 2 ? "#F7F9F3" : "transparent"
                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                spacing: 14
                                Text { text: modelData.ts.substring(11, 19)
                                       color: "#1F2A1B"; font.pixelSize: 14
                                       anchors.verticalCenter: parent.verticalCenter
                                       width: 160 }
                                Rectangle {
                                    width: 90; height: 22; radius: 11
                                    color: modelData.kind === "recycle" ? "#16A34A" : "#0891B2"
                                    anchors.verticalCenter: parent.verticalCenter
                                    Text { anchors.centerIn: parent
                                           text: modelData.kind
                                           color: "#FFFFFF"; font.pixelSize: 11
                                           font.weight: Font.ExtraBold }
                                }
                                Item { width: 16; height: 1 }   // spacer to align
                                Text { text: modelData.slot
                                       color: "#1F2A1B"; font.pixelSize: 14
                                       anchors.verticalCenter: parent.verticalCenter
                                       width: 60 }
                                Text { text: modelData.amount
                                       color: modelData.amount > 0 ? "#16A34A" : "#DC2626"
                                       font.pixelSize: 14; font.weight: Font.ExtraBold
                                       anchors.verticalCenter: parent.verticalCenter
                                       width: 90 }
                                Text { text: modelData.user_id
                                       color: "#5A6B52"; font.pixelSize: 12
                                       anchors.verticalCenter: parent.verticalCenter
                                       width: 220
                                       elide: Text.ElideRight }
                            }
                        }
                    }
                }
            }
        }
    }
}
