import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * AdminFaultsPage — history of every DISPENSE that didn't complete cleanly.
 *
 * Layout:
 *   ┌─────────────────────────────────────────────┐
 *   │ [←]   Dispense Faults    [Refresh] [Clear]  │
 *   ├─────────────────────────────────────────────┤
 *   │  Per-slot summary cards (8 of them)         │
 *   │  ┌────────┐ ┌────────┐ ┌────────┐ …         │
 *   │  │ Slot 1 │ │ Slot 2 │ │ Slot 3 │           │
 *   │  │  3✕    │ │  0     │ │  1✕    │           │
 *   │  │ [Enable]│ │       │ │ [Enable]│          │
 *   │  └────────┘ └────────┘ └────────┘           │
 *   ├─────────────────────────────────────────────┤
 *   │  Recent events                              │
 *   │  ╔══════════════════════════════════════╗   │
 *   │  ║ 2026-05-23 12:04  Slot 3  STALL      ║   │
 *   │  ║   before=120g  after=120g  drop=0g   ║   │
 *   │  ╚══════════════════════════════════════╝   │
 *   │  …                                          │
 *   └─────────────────────────────────────────────┘
 */
Rectangle {
    id: page
    objectName: "adminFaultsPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view

    // Loaded on entry + after Clear.
    property var faults:        []
    property var perSlot:       []

    function refresh() {
        faults  = appManager.dispenseFaults(200)
        perSlot = appManager.dispenseFaultsBySlot()
    }

    function countForSlot(slot) {
        for (let i = 0; i < perSlot.length; ++i)
            if (perSlot[i].slot === slot) return perSlot[i].count
        return 0
    }

    Component.onCompleted: { Idle.disable(); refresh() }
    StackView.onActivated: { Idle.disable(); refresh() }

    // Auto-refresh while the page is open — no need to tap Refresh.
    Timer { interval: 4000; repeat: true; running: true; onTriggered: page.refresh() }

    // ============ Top bar ============
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 110
        color: "#1F2A1B"

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            width: 70; height: 70; radius: 35
            color: "#FFFFFF"
            TapHandler { onTapped: stackView.pop() }
            Text { anchors.centerIn: parent; text: "←"
                   font.pixelSize: 32; color: "#1F2A1B" }
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("Dispense Faults")
            color: "#FFFFFF"
            font.pixelSize: 36; font.weight: Font.Black
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 20
            spacing: 12

            Rectangle {
                width: 130; height: 60; radius: 30
                color: faultRefreshTap.pressed ? "#0E7490" : "#0891B2"
                scale: faultRefreshTap.pressed ? 0.93 : 1.0
                Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }
                TapHandler { id: faultRefreshTap; onTapped: refresh() }
                Text { anchors.centerIn: parent; text: qsTr("Refresh")
                       color: "#FFFFFF"; font.pixelSize: 18; font.weight: Font.Bold }
            }
            Rectangle {
                width: 130; height: 60; radius: 30
                color: faultClearTap.pressed ? "#991B1B" : "#DC2626"
                scale: faultClearTap.pressed ? 0.93 : 1.0
                Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }
                TapHandler {
                    id: faultClearTap
                    onTapped: {
                        const n = appManager.clearDispenseFaults()
                        console.log("Cleared", n, "fault rows")
                        refresh()
                    }
                }
                Text { anchors.centerIn: parent; text: qsTr("Clear")
                       color: "#FFFFFF"; font.pixelSize: 18; font.weight: Font.Bold }
            }
        }
    }

    // ============ Per-slot summary grid ============
    Grid {
        id: slotGrid
        anchors.top: topBar.bottom
        anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.94
        columns: 4
        spacing: 14

        Repeater {
            model: 8
            delegate: Rectangle {
                width: (slotGrid.width - slotGrid.spacing * 3) / 4
                height: 180
                radius: 18
                color: "#FFFFFF"
                border.width: 2
                border.color: page.countForSlot(index + 1) > 0 ? "#DC2626" : "#D8E0CF"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text { text: qsTr("Slot ") + (index + 1)
                           color: "#1F2A1B"
                           font.pixelSize: 22; font.weight: Font.ExtraBold
                           anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: page.countForSlot(index + 1) + (page.countForSlot(index+1) === 1 ? qsTr(" fault") : qsTr(" faults"))
                           color: page.countForSlot(index + 1) > 0 ? "#DC2626" : "#5A6B52"
                           font.pixelSize: 16; font.weight: Font.Bold
                           anchors.horizontalCenter: parent.horizontalCenter }

                    Item { width: 1; height: 8 }

                    Rectangle {
                        visible: page.countForSlot(index + 1) > 0
                        width: parent.width; height: 48; radius: 24
                        color: "#16A34A"
                        anchors.horizontalCenter: parent.horizontalCenter
                        TapHandler {
                            onTapped: {
                                ProductsModel.setActive(index + 1, true)
                                refresh()
                            }
                        }
                        Text { anchors.centerIn: parent; text: qsTr("Re-enable")
                               color: "#FFFFFF"
                               font.pixelSize: 16; font.weight: Font.ExtraBold }
                    }
                }
            }
        }
    }

    // ============ Recent events list ============
    Rectangle {
        anchors.top: slotGrid.bottom
        anchors.topMargin: 22
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.94
        radius: 18
        color: "#FFFFFF"
        border.width: 2; border.color: "#D8E0CF"

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Row {
                width: parent.width
                Text { text: qsTr("Recent events")
                       color: "#1F2A1B"
                       font.pixelSize: 22; font.weight: Font.ExtraBold }
                Item { width: parent.width - 400; height: 1 }
                Text { text: page.faults.length + " " + qsTr("rows")
                       color: "#5A6B52"
                       font.pixelSize: 16 }
            }
            Rectangle { width: parent.width; height: 2; color: "#D8E0CF" }

            ListView {
                width: parent.width
                height: parent.height - 60
                model: page.faults
                clip: true
                spacing: 6
                delegate: Rectangle {
                    width: ListView.view ? ListView.view.width : 0
                    height: 76
                    radius: 12
                    color: "#FBFCFA"
                    border.width: 1; border.color: "#E5E7EB"

                    Row {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 16

                        // Reason badge
                        Rectangle {
                            width: 110; height: 36; radius: 18
                            color: modelData.reason === "STALL"     ? "#DC2626"
                                 : modelData.reason === "NO_DROP"   ? "#92400E"
                                 : modelData.reason === "STEP_LOSS" ? "#7C3AED"
                                 : modelData.reason === "TIMEOUT"   ? "#0891B2"
                                                                    : "#6B7280"
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                anchors.centerIn: parent
                                text: modelData.reason
                                color: "#FFFFFF"
                                font.pixelSize: 14
                                font.weight: Font.ExtraBold
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: qsTr("Slot ") + modelData.slot
                                   color: "#1F2A1B"
                                   font.pixelSize: 18; font.weight: Font.Bold }
                            Text { text: modelData.ts
                                   color: "#5A6B52"
                                   font.pixelSize: 12 }
                        }

                        Item { width: 30; height: 1 }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: qsTr("before / after")
                                   color: "#5A6B52"; font.pixelSize: 11 }
                            Text { text: modelData.before + "g → " + modelData.after + "g"
                                   color: "#1F2A1B"
                                   font.pixelSize: 16; font.weight: Font.Bold }
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: qsTr("drop")
                                   color: "#5A6B52"; font.pixelSize: 11 }
                            Text { text: modelData.drop + "g"
                                   color: modelData.drop > 0 ? "#16A34A" : "#DC2626"
                                   font.pixelSize: 16; font.weight: Font.Bold }
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: qsTr("index pulses")
                                   color: "#5A6B52"; font.pixelSize: 11 }
                            Text { text: modelData.index
                                   color: "#1F2A1B"
                                   font.pixelSize: 16; font.weight: Font.Bold }
                        }
                    }
                }

                // Empty state
                Item {
                    anchors.centerIn: parent
                    visible: page.faults.length === 0
                    width: parent.width; height: 100
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("No faults recorded — every dispense has worked.")
                        color: "#5A6B52"
                        font.pixelSize: 18
                    }
                }
            }
        }
    }
}
