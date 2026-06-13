import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * AdminDiagnosticsPage — hardware self-test. Now matches the
 * dark-banner + clean-card style of AdminProductsPage.
 */
Rectangle {
    id: page
    color: "#EFF3EA"
    property StackView stackView: StackView.view

    Component.onCompleted: Idle.disable()
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
            Text { text: qsTr("Diagnostics")
                   color: "#FFFFFF"
                   font.pixelSize: 50; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Test motors, load cells and sensors")
                   color: "#A5F3FC"
                   font.pixelSize: 18
                   anchors.horizontalCenter: parent.horizontalCenter }
        }
        // Run-all + clear buttons (top-right)
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 30
            spacing: 10
            Rectangle {
                width: 160; height: 80; radius: 40
                color: runAllTap.pressed ? "#0E7490" : "#0891B2"
                scale: runAllTap.pressed ? 0.93 : 1.0
                Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }
                TapHandler { id: runAllTap; onTapped: Diagnostics.testRunAll() }
                Row {
                    anchors.centerIn: parent; spacing: 8
                    Text { text: "▶"; color: "#FFFFFF"
                           font.pixelSize: 24; font.weight: Font.Black
                           anchors.verticalCenter: parent.verticalCenter }
                    Text { text: qsTr("Run all"); color: "#FFFFFF"
                           font.pixelSize: 20; font.weight: Font.ExtraBold
                           anchors.verticalCenter: parent.verticalCenter }
                }
            }
            Rectangle {
                width: 80; height: 80; radius: 40
                color: "transparent"
                border.width: 2; border.color: "#A5F3FC"
                TapHandler { onTapped: Diagnostics.clearResults() }
                Text { anchors.centerIn: parent; text: "↻"
                       color: "#A5F3FC"; font.pixelSize: 36; font.weight: Font.Black }
            }
        }
    }

    // ════════════ TEST SECTIONS ════════════
    Flickable {
        anchors.top: headerBar.bottom
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 24
        width: parent.width - 48
        contentWidth: width
        contentHeight: sections.height
        clip: true

        Column {
            id: sections
            width: parent.width
            spacing: 22

            // ── Reusable test row ──
            component TestRow : Rectangle {
                property string label
                property string cmd
                property string buttonText: qsTr("Test")
                property color  buttonColor: "#1A1D1A"
                signal triggered()
                width: parent.width
                height: 88
                radius: 18
                color: "#FFFFFF"
                border.width: 1; border.color: "#D8E0CF"

                property var entry: Diagnostics.results[cmd] || null

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 18
                    spacing: 16

                    Text {
                        text: label
                        color: "#1F2A1B"
                        font.pixelSize: 22; font.weight: Font.DemiBold
                        width: 260
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Result chip
                    Rectangle {
                        width: 90; height: 36; radius: 18
                        color: entry === null ? "#E5E7EB"
                                              : (entry.ok ? "#16A34A" : "#DC2626")
                        anchors.verticalCenter: parent.verticalCenter
                        Text { anchors.centerIn: parent
                               text: entry === null ? "—"
                                                    : (entry.ok ? qsTr("OK") : qsTr("FAIL"))
                               color: entry === null ? "#5A6B52" : "#FFFFFF"
                               font.pixelSize: 16; font.weight: Font.ExtraBold }
                    }

                    Text {
                        text: entry ? (entry.reply || "") + "  @ " + entry.time : ""
                        color: "#5A6B52"
                        font.pixelSize: 14
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 540
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        width: 140; height: 56; radius: 28
                        color: buttonColor
                        anchors.verticalCenter: parent.verticalCenter
                        TapHandler { onTapped: triggered() }
                        Text { anchors.centerIn: parent
                               text: buttonText
                               color: "#FFFFFF"
                               font.pixelSize: 18; font.weight: Font.ExtraBold }
                    }
                }
            }

            // ── Section header ──
            component SectionHeader : Item {
                property string title
                property string note
                width: parent.width; height: 50
                Row {
                    spacing: 14
                    Rectangle {
                        width: 6; height: 36; radius: 3
                        color: "#7A8B6A"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: title
                        color: "#1F2A1B"
                        font.pixelSize: 26; font.weight: Font.ExtraBold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: note
                        color: "#92400E"
                        font.pixelSize: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── Link ──
            SectionHeader { title: qsTr("Link") }
            TestRow { label: "PING";       cmd: "PING";
                      onTriggered: Diagnostics.testPing() }
            TestRow { label: "IR sensors"; cmd: "IR";
                      onTriggered: Diagnostics.testStatus() }

            // ── Steppers ──
            SectionHeader {
                title: qsTr("Steppers")
                note: qsTr("⚠ requires 12 V motor PSU on")
            }
            Repeater {
                model: 8
                delegate: TestRow {
                    label: qsTr("Stepper ") + (index + 1)
                    cmd: "DISPENSE " + index            // 0-based slot, matches the runner
                    buttonText: qsTr("Spin")
                    buttonColor: "#7A8B6A"
                    onTriggered: Diagnostics.testMotor(index + 1)
                }
            }

            // ── Load cells ──
            SectionHeader { title: qsTr("Load cells") }
            Repeater {
                model: 8
                delegate: TestRow {
                    label: qsTr("Cell ") + (index + 1)
                    cmd: "WEIGH:" + (index + 1)
                    buttonColor: "#0891B2"
                    onTriggered: Diagnostics.testCell(index + 1)
                }
            }

            Item { width: 1; height: 30 }   // bottom padding
        }
    }
}
