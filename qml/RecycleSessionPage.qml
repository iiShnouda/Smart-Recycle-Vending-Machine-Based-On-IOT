import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../components"

/*
 * RecycleSessionPage — the live, animated counter shown while the user is
 * recycling. Bound to the RecycleSession singleton (fed by the STM32's
 * EVT lines). Each accepted item pops the RWG coin; rejects flash red.
 */
Rectangle {
    id: page
    objectName: "recycleSessionPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    Component.onCompleted: { Idle.disable(); RecycleSession.start() }

    function endSession() {
        var total = RecycleSession.finish()
        stackView.push(summaryComp, {
            plasticCount: RecycleSession.bottles,
            canCount: RecycleSession.cans,
            rejectedCount: RecycleSession.rejected,
            totalPoints: total
        })
    }

    // ── React to item events with animations ───────────────────────────
    Connections {
        target: RecycleSession
        function onItemAccepted(type, points) { coinPop.restart(); acceptFlash.restart() }
        function onItemRejected(reason)        { rejectFlash.restart() }
    }

    // Full-screen flashes
    Rectangle { id: acceptOverlay; anchors.fill: parent; color: "#16A34A"; opacity: 0
        SequentialAnimation { id: acceptFlash; NumberAnimation { target: acceptOverlay; property: "opacity"; to: 0.18; duration: 120 }
                              NumberAnimation { target: acceptOverlay; property: "opacity"; to: 0.0; duration: 380 } } }
    Rectangle { id: rejectOverlay; anchors.fill: parent; color: "#DC2626"; opacity: 0
        SequentialAnimation { id: rejectFlash; NumberAnimation { target: rejectOverlay; property: "opacity"; to: 0.22; duration: 120 }
                              NumberAnimation { target: rejectOverlay; property: "opacity"; to: 0.0; duration: 480 } } }

    // ── Header ──
    Text {
        id: title
        anchors.top: parent.top; anchors.topMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("Recycling…")
        color: "#1F2A1B"; font.pixelSize: 64; font.weight: Font.Black
    }

    // ── Big total + spinning coin ──
    Row {
        id: totalRow
        anchors.top: title.bottom; anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 18
        RwgCoin {
            id: coin
            anchors.verticalCenter: parent.verticalCenter
            size: 110
            // Pop bigger on each accept, settle back.
            SequentialAnimation on scale { id: coinPop; running: false
                NumberAnimation { from: 1.0; to: 1.45; duration: 140; easing.type: Easing.OutBack }
                NumberAnimation { to: 1.0; duration: 260; easing.type: Easing.OutQuad } }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: RecycleSession.totalPoints
            color: "#0891B2"; font.pixelSize: 130; font.weight: Font.Black
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.bottomMargin: 12
            text: qsTr("pts")
            color: "#5A6B52"; font.pixelSize: 36; font.weight: Font.DemiBold
        }
    }

    // ── Three counters ──
    Row {
        id: counters
        anchors.top: totalRow.bottom; anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 30

        component Counter: Rectangle {
            width: 300; height: 280; radius: 28
            color: "#FFFFFF"; border.width: 2; border.color: borderColor
            property string label: ""
            property int    value: 0
            property string icon: ""
            property color  accent: "#0891B2"
            property color  borderColor: "#D8E0CF"
            Column {
                anchors.centerIn: parent; spacing: 10
                Text { text: icon; font.pixelSize: 70; color: accent
                       anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: value; color: "#1F2A1B"; font.pixelSize: 96; font.weight: Font.Black
                       anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: label; color: "#5A6B52"; font.pixelSize: 24; font.weight: Font.DemiBold
                       anchors.horizontalCenter: parent.horizontalCenter }
            }
        }

        Counter { label: qsTr("Bottles"); value: RecycleSession.bottles; icon: "🍼"; accent: "#0891B2" }
        Counter { label: qsTr("Cans");    value: RecycleSession.cans;    icon: "🥫"; accent: "#7A8B6A" }
        Counter { label: qsTr("Rejected"); value: RecycleSession.rejected; icon: "✕"; accent: "#DC2626"; borderColor: "#FCA5A5" }
    }

    // ── Last-event banner ──
    Rectangle {
        id: banner
        anchors.top: counters.bottom; anchors.topMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.8; height: 90; radius: 45
        color: "#1A1D1A"
        Text {
            anchors.centerIn: parent
            text: RecycleSession.lastEvent
            color: "#FFFFFF"; font.pixelSize: 30; font.weight: Font.DemiBold
        }
    }

    // ── Finish button ──
    Rectangle {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        width: 460; height: 110; radius: 55; color: "#16A34A"
        scale: finishTap.pressed ? 0.95 : 1.0
        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
        TapHandler { id: finishTap; onTapped: page.endSession() }
        Text { anchors.centerIn: parent
               text: qsTr("Finish & get points")
               color: "#FFFFFF"; font.pixelSize: 34; font.weight: Font.ExtraBold }
    }

    Component { id: summaryComp; RecycleSummaryPage {} }
}
