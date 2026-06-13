import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

// Translation refresh helper

/*
 * ConsentPage — shows the privacy summary + "I agree / Use QR instead".
 * Pushed onto the stack when the user picks "Sign up with face".
 */
Rectangle {
    id: page
    objectName: "consentPage"
    color: "#F2F4ED"
    property StackView stackView: StackView.view
    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Back button
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left
        anchors.topMargin: 30; anchors.leftMargin: 30
        width: 90; height: 90; radius: 45
        color: "#FFFFFF"; border.width: 2; border.color: "#D8E0CF"
        TapHandler { onTapped: stackView.pop() }
        Text { anchors.centerIn: parent; text: "←"; font.pixelSize: 36; color: "#1F2A1B" }
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
        Text {
            text: { langTick; return qsTr("Welcome to ReWinGo") }
            color: "#1F2A1B"
            font.pixelSize: 56
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: { langTick; return qsTr("Quick consent before we start") }
            color: "#5A6B52"
            font.pixelSize: 20
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Flickable {
        id: flick
        anchors.top: parent.top
        anchors.topMargin: 170
        anchors.bottom: btnRow.top
        anchors.bottomMargin: 30
        width: parent.width * 0.86
        anchors.horizontalCenter: parent.horizontalCenter
        contentWidth: width
        contentHeight: col.height
        clip: true

        Column {
            id: col
            width: parent.width
            spacing: 22

            component Bullet : Row {
                spacing: 14
                property string icon
                property string title
                property string body
                Text { text: icon; font.pixelSize: 40 }
                Column {
                    spacing: 4
                    width: flick.width - 60
                    Text { text: title; color: "#1F2A1B"; font.pixelSize: 22;
                           font.weight: Font.ExtraBold }
                    Text { text: body;  color: "#5A6B52"; font.pixelSize: 16;
                           wrapMode: Text.WordWrap; width: parent.width }
                }
            }

            Bullet {
                icon:  "📸"; title: { langTick; return qsTr("Your face") }
                body:  { langTick; return qsTr("We turn it into a list of numbers (a fingerprint) so we can recognise you next visit. We DO NOT keep your photo.") }
            }
            Bullet {
                icon:  "👤"; title: { langTick; return qsTr("Your name") }
                body:  { langTick; return qsTr("So we can greet you and link points to you.") }
            }
            Bullet {
                icon:  "♻️"; title: { langTick; return qsTr("Recycling & vending activity") }
                body:  { langTick; return qsTr("Items recycled, products bought, points earned & spent.") }
            }
            Bullet {
                icon:  "🗓"; title: { langTick; return qsTr("Kept for up to 12 months") }
                body:  { langTick; return qsTr("After your last visit, your data is deleted automatically.") }
            }
            Bullet {
                icon:  "🗑"; title: { langTick; return qsTr("You can delete everything any time") }
                body:  { langTick; return qsTr("Tap \"Delete my data\" in the user menu.") }
            }
        }
    }

    Row {
        id: btnRow
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 24

        // Use QR instead
        Rectangle {
            width: 260; height: 90; radius: 45
            color: "transparent"
            border.width: 2; border.color: "#5A6B52"
            TapHandler {
                onTapped: stackView.replace(
                    "qrc:/Recycle_Vending_Machine_LCD/qml/QrLoginPage.qml")
            }
            Text { anchors.centerIn: parent
                   text: { langTick; return qsTr("Use QR instead") }
                   color: "#1F2A1B"
                   font.pixelSize: 20; font.weight: Font.DemiBold }
        }
        // I agree
        Rectangle {
            width: 260; height: 90; radius: 45
            color: "#1A1D1A"
            TapHandler {
                onTapped: stackView.replace(
                    "qrc:/Recycle_Vending_Machine_LCD/qml/registration/RegistrationDetailsPage.qml")
            }
            Row {
                anchors.centerIn: parent; spacing: 10
                Text { text: { langTick; return qsTr("I agree") }
                       color: "#FFFFFF"
                       font.pixelSize: 24; font.weight: Font.ExtraBold
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: "→"; color: "#00E5FF"
                       font.pixelSize: 24; font.weight: Font.Black
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }
}
