import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * AdminMachinePage — "Deploy machine". The admin sets this machine's NAME +
 * LOCATION and toggles Deployed (show in the app) / In service. Saving
 * publishes the retained machine state, so the phone "Find machine" page shows
 * it by name + place (not the raw id) with its live status + bins + products.
 */
Rectangle {
    id: page
    objectName: "adminMachinePage"
    color: "#EFF3EA"
    property StackView stackView: StackView.view

    property bool deployed:  appManager.machineDeployed()
    property bool inService: appManager.machineInService()

    Component.onCompleted: Idle.disable()
    StackView.onActivated:  Idle.disable()

    // Header
    Rectangle {
        id: headerBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 160; color: "#1F2A1B"
        Rectangle {
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 30; width: 80; height: 80; radius: 40; color: "#FFFFFF"
            TapHandler { onTapped: stackView.pop() }
            Text { anchors.centerIn: parent; text: "←"; font.pixelSize: 36; color: "#1F2A1B" }
        }
        Column {
            anchors.centerIn: parent; spacing: 4
            Text { text: qsTr("Deploy machine"); color: "#FFFFFF"
                   font.pixelSize: 50; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Name, location & status shown in the phone app")
                   color: "#A5F3FC"; font.pixelSize: 18
                   anchors.horizontalCenter: parent.horizontalCenter }
        }
    }

    Column {
        anchors.top: headerBar.bottom; anchors.topMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        width: 760; spacing: 20

        Text { text: qsTr("Machine name"); color: "#1F2A1B"
               font.pixelSize: 22; font.weight: Font.DemiBold }
        Rectangle {
            width: parent.width; height: 88; radius: 16; color: "#FFFFFF"
            border.width: 2; border.color: nameField.activeFocus ? "#0891B2" : "#D8E0CF"
            TextField {
                id: nameField; anchors.fill: parent; anchors.margins: 6
                font.pixelSize: 28; leftPadding: 16
                verticalAlignment: TextInput.AlignVCenter
                placeholderText: qsTr("e.g. ReWinGo — Cafeteria"); background: Item {}
                text: appManager.machineName()
            }
        }

        Text { text: qsTr("Location"); color: "#1F2A1B"
               font.pixelSize: 22; font.weight: Font.DemiBold }
        Rectangle {
            width: parent.width; height: 88; radius: 16; color: "#FFFFFF"
            border.width: 2; border.color: locField.activeFocus ? "#0891B2" : "#D8E0CF"
            TextField {
                id: locField; anchors.fill: parent; anchors.margins: 6
                font.pixelSize: 28; leftPadding: 16
                verticalAlignment: TextInput.AlignVCenter
                placeholderText: qsTr("e.g. Building A, Ground Floor"); background: Item {}
                text: appManager.machineLocation()
            }
        }

        Row {
            width: parent.width; spacing: 16
            Column {
                width: parent.width - 110
                Text { text: qsTr("Show in the app (Deployed)"); color: "#1F2A1B"
                       font.pixelSize: 22; font.weight: Font.DemiBold }
                Text { text: qsTr("When on, this machine appears on the Find Machine page.")
                       color: "#5A6B52"; font.pixelSize: 15
                       wrapMode: Text.WordWrap; width: parent.width }
            }
            Switch { checked: page.deployed; onToggled: page.deployed = checked
                     anchors.verticalCenter: parent.verticalCenter }
        }

        Row {
            width: parent.width; spacing: 16
            Column {
                width: parent.width - 110
                Text { text: qsTr("In service"); color: "#1F2A1B"
                       font.pixelSize: 22; font.weight: Font.DemiBold }
                Text { text: qsTr("Turn off to mark the machine OUT OF SERVICE in the app.")
                       color: "#5A6B52"; font.pixelSize: 15
                       wrapMode: Text.WordWrap; width: parent.width }
            }
            Switch { checked: page.inService; onToggled: page.inService = checked
                     anchors.verticalCenter: parent.verticalCenter }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 340; height: 92; radius: 46; color: "#16A34A"
            TapHandler {
                onTapped: {
                    appManager.setMachineInfo(nameField.text.trim(),
                                              locField.text.trim(),
                                              page.deployed, page.inService)
                    saved.visible = true
                }
            }
            Text { anchors.centerIn: parent; text: qsTr("Save & publish")
                   color: "#FFFFFF"; font.pixelSize: 26; font.weight: Font.ExtraBold }
        }
        Text { id: saved; visible: false
               text: qsTr("Saved ✓ — now live in the app")
               color: "#16A34A"; font.pixelSize: 18
               anchors.horizontalCenter: parent.horizontalCenter }
    }
}
