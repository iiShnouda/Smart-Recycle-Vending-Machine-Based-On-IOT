import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * AdminInventoryPage — live, read-only view of what each shelf currently
 * holds, and a feed of the last few "something changed" events.
 *
 * Why this page exists: to make it obvious that the kiosk auto-tracks
 * inventory. Admin walks up, sees the counts updating in real time, sees
 * the restock history with "Slot 3: +20 [admin]" entries — no calibration
 * dance required for normal restocks.
 *
 * Layout:
 *   ┌────────────────────────────────────────────┐
 *   │ [←]  Inventory               [Rescan now]  │
 *   ├────────────────────────────────────────────┤
 *   │ ╔════════════════════╗  ╔════════════════╗ │
 *   │ ║ Slot 1 — Cola      ║  ║ Slot 2 — Chips ║ │
 *   │ ║ 12 items           ║  ║ Not calibrated ║ │
 *   │ ║ 216 g  (raw 18432) ║  ║ raw 41200       ║ │
 *   │ ║ ✓ Calibrated       ║  ║ ⚠ Set up scale ║ │
 *   │ ║ Last: +5 [admin]   ║  ║                ║ │
 *   │ ╚════════════════════╝  ╚════════════════╝ │
 *   │  (six more)                                 │
 *   ├────────────────────────────────────────────┤
 *   │ Recent activity (newest first)              │
 *   │  • 14:32  Slot 3  +20  [admin]              │
 *   │  • 12:11  Slot 1  -1   [dispense]           │
 *   │  • 11:48  Slot 5  +8   [boot]               │
 *   └────────────────────────────────────────────┘
 */
Rectangle {
    id: page
    objectName: "adminInventoryPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view
    property var       latestBySlot: []     // [{slot, ts, delta, source}, ...]
    property var       events:       []     // recent activity feed

    function refresh() {
        latestBySlot = appManager.latestRestockBySlot()
        events       = appManager.restockEvents(50)
    }
    function lastForSlot(slot) {
        for (let i = 0; i < latestBySlot.length; ++i)
            if (latestBySlot[i].slot === slot) return latestBySlot[i]
        return null
    }

    Component.onCompleted: { Idle.disable(); refresh() }
    StackView.onActivated: { Idle.disable(); refresh() }

    // Every time a slot's data changes we re-pull (cheap — the DB is local).
    Connections {
        target: ProductsModel
        function onDataChanged() { page.refresh() }
    }

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

        Column {
            anchors.centerIn: parent
            spacing: 2
            Text { text: qsTr("Inventory")
                   color: "#FFFFFF"
                   font.pixelSize: 36; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Auto-tracked by the load cells")
                   color: "#A5F3FC"
                   font.pixelSize: 14
                   anchors.horizontalCenter: parent.horizontalCenter }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 20
            width: 160; height: 60; radius: 30
            color: rescanTap.pressed ? "#0E7490" : "#0891B2"
            scale: rescanTap.pressed ? 0.93 : 1.0
            Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }
            TapHandler {
                id: rescanTap
                onTapped: { appManager.rescanInventoryNow(); page.refresh() }
            }
            Text { anchors.centerIn: parent; text: qsTr("Rescan now")
                   color: "#FFFFFF"; font.pixelSize: 17; font.weight: Font.ExtraBold }
        }
    }

    // ============ Slot card grid ============
    Grid {
        id: slotGrid
        anchors.top: topBar.bottom
        anchors.topMargin: 22
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.94
        columns: 2
        spacing: 14

        Repeater {
            model: 8
            delegate: Rectangle {
                id: card
                width: (slotGrid.width - slotGrid.spacing) / 2
                height: 230
                radius: 18
                color: "#FFFFFF"

                // Pull this slot's data straight from ProductsModel.
                property int    slotIdx: index
                property string pname: ""
                property int    pcount: 0
                property int    pweight: 0
                property int    plastRaw: 0
                property int    pemptyShelf: 0
                property int    pUnitRaw: 0
                property bool   pCalibrated: false
                property bool   pActive: false

                function refreshRow() {
                    const idx = ProductsModel.index(slotIdx, 0)
                    if (!idx.valid) return
                    pname        = ProductsModel.data(idx, Qt.UserRole + 2)
                    pactive      = ProductsModel.data(idx, Qt.UserRole + 5)
                    pcount       = ProductsModel.data(idx, Qt.UserRole + 6)
                    pweight      = ProductsModel.data(idx, Qt.UserRole + 7)
                    pemptyShelf  = ProductsModel.data(idx, Qt.UserRole + 8)
                    pUnitRaw     = ProductsModel.data(idx, Qt.UserRole + 9)
                    plastRaw     = ProductsModel.data(idx, Qt.UserRole + 10)
                    pCalibrated  = ProductsModel.data(idx, Qt.UserRole + 11)
                }
                Component.onCompleted: refreshRow()
                Connections {
                    target: ProductsModel
                    function onDataChanged() { card.refreshRow() }
                    function onModelReset()  { card.refreshRow() }
                }

                border.width: 2
                border.color: card.pCalibrated ? "#D8E0CF" : "#FBBF24"

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    Row {
                        width: parent.width
                        spacing: 8
                        Rectangle {
                            width: 50; height: 30; radius: 15
                            color: "#1A1D1A"
                            anchors.verticalCenter: parent.verticalCenter
                            Text { anchors.centerIn: parent
                                   text: "#" + (card.slotIdx + 1)
                                   color: "#FFFFFF"
                                   font.pixelSize: 13; font.weight: Font.ExtraBold }
                        }
                        Text {
                            text: card.pname.length > 0 ? card.pname : qsTr("(empty slot)")
                            color: card.pname.length > 0 ? "#1F2A1B" : "#9CA3AF"
                            font.pixelSize: 20; font.weight: Font.ExtraBold
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 60
                        }
                    }

                    // Big count
                    Row {
                        spacing: 12
                        anchors.horizontalCenter: parent.horizontalCenter
                        Text {
                            text: card.pCalibrated ? card.pcount : "?"
                            color: card.pCalibrated ? "#0891B2" : "#9CA3AF"
                            font.pixelSize: 56; font.weight: Font.Black
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0
                            Text { text: card.pCalibrated
                                         ? qsTr("items")
                                         : qsTr("calibrate")
                                   color: "#5A6B52"
                                   font.pixelSize: 16; font.weight: Font.Bold }
                            Text { text: card.pCalibrated
                                         ? card.pweight + " g"
                                         : qsTr("first")
                                   color: "#5A6B52"
                                   font.pixelSize: 12 }
                        }
                    }

                    // Status strip
                    Row {
                        spacing: 8
                        width: parent.width
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            width: 22; height: 22; radius: 11
                            color: card.pCalibrated ? "#16A34A" : "#FBBF24"
                            anchors.verticalCenter: parent.verticalCenter
                            Text { anchors.centerIn: parent
                                   text: card.pCalibrated ? "✓" : "!"
                                   color: "#FFFFFF"
                                   font.pixelSize: 13; font.weight: Font.Black }
                        }
                        Text {
                            text: card.pCalibrated
                                  ? qsTr("Auto-counting")
                                  : qsTr("Calibrate to enable count")
                            color: card.pCalibrated ? "#16A34A" : "#92400E"
                            font.pixelSize: 12; font.weight: Font.Bold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Item { width: parent.width - 200; height: 1 }
                        Text {
                            text: qsTr("raw ") + card.plastRaw
                            color: "#9CA3AF"
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Last event line
                    Text {
                        property var lastEvent: page.lastForSlot(card.slotIdx + 1)
                        text: lastEvent
                              ? qsTr("Last change: %1 %2 (%3) [%4]")
                                    .arg(lastEvent.delta > 0 ? "+" : "")
                                    .arg(lastEvent.delta)
                                    .arg(lastEvent.ts ? lastEvent.ts.toString().slice(0,16).replace("T"," ") : "")
                                    .arg(lastEvent.source)
                              : qsTr("No changes recorded yet")
                        color: "#5A6B52"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }
            }
        }
    }

    // ============ Recent activity feed ============
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
                Text { text: qsTr("Recent activity")
                       color: "#1F2A1B"
                       font.pixelSize: 22; font.weight: Font.ExtraBold }
                Item { width: parent.width - 400; height: 1 }
                Text { text: page.events.length + " " + qsTr("events")
                       color: "#5A6B52"
                       font.pixelSize: 14 }
            }
            Rectangle { width: parent.width; height: 2; color: "#D8E0CF" }

            ListView {
                width: parent.width
                height: parent.height - 60
                model: page.events
                clip: true
                spacing: 4
                delegate: Rectangle {
                    width: ListView.view ? ListView.view.width : 0
                    height: 56
                    radius: 10
                    color: "#FBFCFA"
                    border.width: 1; border.color: "#E5E7EB"

                    Row {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 14

                        // Source badge (admin / dispense / boot / scan)
                        Rectangle {
                            width: 100; height: 32; radius: 16
                            color: modelData.source === "admin"    ? "#16A34A"
                                 : modelData.source === "dispense" ? "#0891B2"
                                 : modelData.source === "boot"     ? "#92400E"
                                                                   : "#6B7280"
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                anchors.centerIn: parent
                                text: modelData.source
                                color: "#FFFFFF"
                                font.pixelSize: 13
                                font.weight: Font.ExtraBold
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: qsTr("Slot ") + modelData.slot
                                   color: "#1F2A1B"
                                   font.pixelSize: 16; font.weight: Font.Bold }
                            Text { text: modelData.ts ? modelData.ts.toString().slice(0,19).replace("T"," ") : ""
                                   color: "#5A6B52"
                                   font.pixelSize: 11 }
                        }

                        Item { width: 20; height: 1 }

                        Text {
                            text: modelData.prevCount + " → " + modelData.newCount
                            color: "#1F2A1B"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Rectangle {
                            width: 80; height: 36; radius: 18
                            color: modelData.delta > 0 ? "#DCFCE7"
                                 : modelData.delta < 0 ? "#FEE2E2"
                                                       : "#F3F4F6"
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                anchors.centerIn: parent
                                text: (modelData.delta > 0 ? "+" : "") + modelData.delta
                                color: modelData.delta > 0 ? "#16A34A"
                                     : modelData.delta < 0 ? "#DC2626"
                                                           : "#6B7280"
                                font.pixelSize: 18; font.weight: Font.Black
                            }
                        }
                    }
                }

                // Empty state
                Item {
                    anchors.centerIn: parent
                    visible: page.events.length === 0
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("No inventory changes yet — restock something and watch.")
                        color: "#5A6B52"
                        font.pixelSize: 16
                    }
                }
            }
        }
    }
}
