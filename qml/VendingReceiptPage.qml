import QtQuick
import QtQuick.Controls
import "../components"

/*
 * VendingReceiptPage — shown after a vend session finishes dispensing.
 * Lists what dropped (photo + name + price), the points spent, the new
 * balance, and a thanks line.
 *
 * Pushed with:
 *   { items: [{name, price, imagePath}], totalUsed: int, newBalance: int }
 */
Rectangle {
    id: page
    objectName: "vendingReceiptPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view

    property var items: []
    property int totalUsed: 0
    property int newBalance: 0

    property int langTick: 0
    Connections { target: appManager; function onLanguageChanged() { langTick++ } }
    Component.onCompleted: Idle.touch()

    // Return to the user's Main page (menu) without logging out.
    function goMain() {
        var mp = stackView.find(function(item) { return item.objectName === "mainPage" })
        if (mp) stackView.pop(mp)
        else while (stackView && stackView.depth > 1) stackView.pop()
    }

    // ── Header ──
    Column {
        id: header
        anchors.top: parent.top; anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
        Text { text: { langTick; return qsTr("Thank You!") }
               color: "#1F2A1B"; font.pixelSize: 80; font.weight: Font.Black
               anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: { langTick; return qsTr("Please collect your items below") }
               color: "#5A6B52"; font.pixelSize: 22
               anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: { langTick; return qsTr("Thanks for using ReWinGo 💙") }
               color: "#0891B2"; font.pixelSize: 24; font.weight: Font.DemiBold
               anchors.horizontalCenter: parent.horizontalCenter; topPadding: 6 }
    }

    // ── Receipt card ──
    Rectangle {
        id: card
        anchors.top: header.bottom; anchors.topMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.86
        height: parent.height - header.height - 320
        radius: 32; color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"

        ListView {
            id: list
            anchors.fill: parent; anchors.margins: 24
            clip: true; spacing: 12
            model: page.items
            delegate: Rectangle {
                width: list.width; height: 110; radius: 18
                color: "#F8FAF4"; border.width: 1; border.color: "#E3E9D8"
                Row {
                    anchors.fill: parent; anchors.margins: 14; spacing: 18
                    Rectangle {
                        width: 82; height: 82; radius: 14; color: "#FFFFFF"
                        border.width: 1; border.color: "#D8E0CF"; clip: true
                        anchors.verticalCenter: parent.verticalCenter
                        Image {
                            anchors.fill: parent; anchors.margins: 6
                            source: modelData.imagePath || ""
                            fillMode: Image.PreserveAspectFit; smooth: true
                            visible: source != ""
                        }
                        Text { anchors.centerIn: parent; text: "📦"
                               font.pixelSize: 36; visible: !modelData.imagePath }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: list.width - 230
                        text: modelData.name; color: "#1F2A1B"
                        font.pixelSize: 30; font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "-" + modelData.price + " " + qsTr("pts")
                        color: "#DC2626"; font.pixelSize: 28; font.weight: Font.Bold
                    }
                }
            }
        }
    }

    // ── Totals + Done ──
    Column {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 18; width: parent.width * 0.86

        Row {
            anchors.horizontalCenter: parent.horizontalCenter; spacing: 60
            Column { spacing: 2
                Text { text: { langTick; return qsTr("Points used") }
                       color: "#5A6B52"; font.pixelSize: 20
                       anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: page.totalUsed; color: "#DC2626"
                       font.pixelSize: 56; font.weight: Font.Black
                       anchors.horizontalCenter: parent.horizontalCenter } }
            Rectangle { width: 2; height: 90; color: "#D8E0CF" }
            Column { spacing: 2
                Text { text: { langTick; return qsTr("New balance") }
                       color: "#5A6B52"; font.pixelSize: 20
                       anchors.horizontalCenter: parent.horizontalCenter }
                Row { spacing: 10; anchors.horizontalCenter: parent.horizontalCenter
                    RwgCoin { size: 48; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: page.newBalance; color: "#0891B2"
                           font.pixelSize: 56; font.weight: Font.Black
                           anchors.verticalCenter: parent.verticalCenter } } }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 24

            // Back to Main Menu (stay logged in)
            Rectangle {
                width: 320; height: 100; radius: 50; color: "#0891B2"
                scale: mainTap.pressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
                TapHandler { id: mainTap; onTapped: page.goMain() }
                Row {
                    anchors.centerIn: parent; spacing: 10
                    Text { text: "⌂"; color: "#FFFFFF"; font.pixelSize: 32; font.weight: Font.Black
                           anchors.verticalCenter: parent.verticalCenter }
                    Text { text: { langTick; return qsTr("Main Menu") }
                           color: "#FFFFFF"; font.pixelSize: 30; font.weight: Font.ExtraBold
                           anchors.verticalCenter: parent.verticalCenter }
                }
            }

            // Done → home (logout to start screen)
            Rectangle {
                width: 320; height: 100; radius: 50; color: "#1A1D1A"
                scale: doneTap.pressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
                TapHandler { id: doneTap
                    onTapped: { while (stackView && stackView.depth > 1) stackView.pop() } }
                Text { anchors.centerIn: parent; text: { langTick; return qsTr("Done") }
                       color: "#FFFFFF"; font.pixelSize: 34; font.weight: Font.ExtraBold }
            }
        }
    }
}
