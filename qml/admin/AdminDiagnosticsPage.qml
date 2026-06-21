import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../../components"   // BounceOnPress (tactile press feedback)

/*
 * AdminDiagnosticsPage — hardware self-test. Every actuator and sensor on the
 * STM32 board (plus the recycle Arduino) has a button here; results come
 * back as OK/FAIL chips.
 *
 * Each row's `cmd` MUST equal the exact wire string DiagnosticsRunner sends,
 * because results are keyed by that string (see DiagnosticsRunner::record).
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
            BounceOnPress { onTapped: stackView.pop() }
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
            Text { text: qsTr("Test every motor, relay, sensor and load cell")
                   color: "#A5F3FC"
                   font.pixelSize: 18
                   anchors.horizontalCenter: parent.horizontalCenter }
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 30
            spacing: 10

            // Arduino link status — separate board, separate connection,
            // so it gets its own indicator rather than implying it's part
            // of the STM32 link.
            Rectangle {
                width: 200; height: 80; radius: 40
                color: "transparent"
                border.width: 2
                border.color: AppManager.arduinoConnected ? "#16A34A" : "#DC2626"
                Row {
                    anchors.centerIn: parent
                    spacing: 10
                    Rectangle {
                        width: 14; height: 14; radius: 7
                        anchors.verticalCenter: parent.verticalCenter
                        color: AppManager.arduinoConnected ? "#16A34A" : "#DC2626"
                    }
                    Text {
                        text: AppManager.arduinoConnected
                              ? qsTr("Arduino linked")
                              : qsTr("Arduino offline")
                        color: "#FFFFFF"
                        font.pixelSize: 16; font.weight: Font.ExtraBold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Rectangle {
                width: 80; height: 80; radius: 40
                color: "transparent"
                border.width: 2; border.color: "#A5F3FC"
                BounceOnPress { onTapped: Diagnostics.clearResults() }
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

            // ── Reusable command row (chip keyed on `cmd`) ──
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
                        width: 240
                        anchors.verticalCenter: parent.verticalCenter
                    }
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
                        width: parent.width - 520
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        width: 140; height: 56; radius: 28
                        color: buttonColor
                        anchors.verticalCenter: parent.verticalCenter
                        BounceOnPress { onTapped: triggered() }
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
                    Rectangle { width: 6; height: 36; radius: 3; color: "#7A8B6A"
                                anchors.verticalCenter: parent.verticalCenter }
                    Text { text: title; color: "#1F2A1B"
                           font.pixelSize: 26; font.weight: Font.ExtraBold
                           anchors.verticalCenter: parent.verticalCenter }
                    Text { text: note; color: "#92400E"; font.pixelSize: 14
                           anchors.verticalCenter: parent.verticalCenter }
                }
            }

            // ── Link ──
            SectionHeader { title: qsTr("Link") }
            TestRow { label: "Ping"; cmd: "PING"; onTriggered: Diagnostics.testPing() }
            TestRow { label: qsTr("Admin door"); cmd: "DOOR";
                      onTriggered: Diagnostics.testDoor() }

            // ── IR sensors (one command returns a 5-bit mask) ──
            SectionHeader { title: qsTr("IR sensors")
                            note: (AppManager.arduinoConnected ? qsTr("F1–F5 inlet / lane beams")
                                                               : qsTr("Arduino offline — check the GPIO UART link")) }
            Rectangle {
                id: irCard
                width: parent.width; height: 120; radius: 18
                color: "#FFFFFF"; border.width: 1; border.color: "#D8E0CF"
                property var entry: Diagnostics.results["IR"] || null
                // Parse "OK IR:0x1F" → 0x1F. Bit i set = sensor F(i+1) detects something.
                property int mask: {
                    if (!entry || !entry.ok) return -1
                    var m = (entry.reply || "").match(/0x([0-9A-Fa-f]+)/)
                    return m ? parseInt(m[1], 16) : -1
                }
                Column {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    Row {
                        width: parent.width
                        Text { text: qsTr("Hold an object in front, then tap Read")
                               color: "#5A6B52"; font.pixelSize: 16
                               anchors.verticalCenter: parent.verticalCenter }
                        Item { width: parent.width - 360; height: 1 }
                        Rectangle {
                            width: 140; height: 52; radius: 26; color: "#0891B2"
                            anchors.verticalCenter: parent.verticalCenter
                            BounceOnPress { onTapped: Diagnostics.testIr() }
                            Text { anchors.centerIn: parent; text: qsTr("Read")
                                   color: "#FFFFFF"; font.pixelSize: 18; font.weight: Font.ExtraBold }
                        }
                    }
                    Row {
                        spacing: 14
                        Repeater {
                            model: 5
                            delegate: Column {
                                required property int index
                                spacing: 4
                                Rectangle {
                                    width: 54; height: 36; radius: 10
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: irCard.mask < 0 ? "#E5E7EB"
                                          : ((irCard.mask & (1 << index)) ? "#16A34A" : "#CBD5C0")
                                }
                                Text { text: "F" + (index + 1)
                                       anchors.horizontalCenter: parent.horizontalCenter
                                       color: "#1F2A1B"; font.pixelSize: 16; font.weight: Font.Bold }
                            }
                        }
                    }
                }
            }

            // ── Steppers (one revolution each, via the 595 mux) ──
            SectionHeader { title: qsTr("Vending motors")
                            note: qsTr("⚠ 12 V motor PSU on — one full turn") }
            Repeater {
                model: 8
                delegate: TestRow {
                    label: qsTr("Motor ") + (index + 1)
                    cmd: Diagnostics.motorCmd(index + 1)   // STEP:51200:1:<n>
                    buttonText: qsTr("Spin")
                    buttonColor: "#7A8B6A"
                    onTriggered: Diagnostics.testMotor(index + 1)
                }
            }

            // ── Conveyor (recycle belt) ──
            SectionHeader { title: qsTr("Recycle conveyor")
                            note: (AppManager.arduinoConnected ? qsTr("⚠ 12 V belt PSU on")
                                                               : qsTr("Arduino offline — check the GPIO UART link")) }
            TestRow {
                label: qsTr("Belt")
                cmd: "CONVEYOR"
                buttonText: qsTr("Run")
                buttonColor: "#7A8B6A"
                onTriggered: Diagnostics.testConveyor()
            }

            // ── Servo diverter ──
            SectionHeader { title: qsTr("Sorting servo") }
            TestRow { label: qsTr("Bottle side (30°)");  cmd: "ANGLE:30";
                      buttonColor: "#0891B2"; onTriggered: Diagnostics.testServo(30) }
            TestRow { label: qsTr("Centre (90°)");       cmd: "ANGLE:90";
                      buttonColor: "#0891B2"; onTriggered: Diagnostics.testServo(90) }
            TestRow { label: qsTr("Can side (150°)");    cmd: "ANGLE:150";
                      buttonColor: "#0891B2"; onTriggered: Diagnostics.testServo(150) }

            // ── Relays (lights) — On/Off per channel ──
            SectionHeader { title: qsTr("Relays / lights") }
            Repeater {
                model: [ { lbl: qsTr("Relay 1 (PA9)"),     idx: 1 },
                         { lbl: qsTr("Bottom LED (PA10)"), idx: 2 },
                         { lbl: qsTr("Vending (PB8)"),     idx: 3 } ]
                delegate: Rectangle {
                    required property var modelData
                    width: parent.width; height: 88; radius: 18
                    color: "#FFFFFF"; border.width: 1; border.color: "#D8E0CF"
                    property var onE:  Diagnostics.results[Diagnostics.relayCmd(modelData.idx, true)]  || null
                    property var offE: Diagnostics.results[Diagnostics.relayCmd(modelData.idx, false)] || null
                    property var entry: (onE && offE) ? (onE.time >= offE.time ? onE : offE)
                                                      : (onE || offE)
                    Row {
                        anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 18
                        spacing: 16
                        Text { text: modelData.lbl; color: "#1F2A1B"
                               font.pixelSize: 22; font.weight: Font.DemiBold; width: 240
                               anchors.verticalCenter: parent.verticalCenter }
                        Rectangle {
                            width: 90; height: 36; radius: 18
                            color: entry === null ? "#E5E7EB" : (entry.ok ? "#16A34A" : "#DC2626")
                            anchors.verticalCenter: parent.verticalCenter
                            Text { anchors.centerIn: parent
                                   text: entry === null ? "—" : (entry.ok ? qsTr("OK") : qsTr("FAIL"))
                                   color: entry === null ? "#5A6B52" : "#FFFFFF"
                                   font.pixelSize: 16; font.weight: Font.ExtraBold }
                        }
                        Item { width: parent.width - 520; height: 1 }
                        Rectangle {
                            width: 100; height: 56; radius: 28; color: "#16A34A"
                            anchors.verticalCenter: parent.verticalCenter
                            BounceOnPress { onTapped: Diagnostics.testRelay(modelData.idx, true) }
                            Text { anchors.centerIn: parent; text: qsTr("On")
                                   color: "#FFFFFF"; font.pixelSize: 18; font.weight: Font.ExtraBold }
                        }
                        Rectangle {
                            width: 100; height: 56; radius: 28; color: "#6B7280"
                            anchors.verticalCenter: parent.verticalCenter
                            BounceOnPress { onTapped: Diagnostics.testRelay(modelData.idx, false) }
                            Text { anchors.centerIn: parent; text: qsTr("Off")
                                   color: "#FFFFFF"; font.pixelSize: 18; font.weight: Font.ExtraBold }
                        }
                    }
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