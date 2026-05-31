import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

Rectangle {
    id: page
    objectName: "authChoicePage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Idle is global — no per-page timer.
    function bumpIdle() { Idle.touch() }
    Component.onCompleted: Idle.touch()
    StackView.onActivated: Idle.touch()

    // ── Python face-rec sidecar wiring ────────────────────────────────────
    // Tapping "Face Detection" launches the FaceRec_project's Python
    // pipeline (MediaPipe liveness + ArcFace match) in its own process.
    // Python opens its cv2 window for the camera + liveness prompts; this
    // overlay keeps the kiosk UI in a "busy" state until the result arrives.
    property string faceStatus: ""
    property string lastFaceError: ""

    Connections {
        target: FaceRec
        function onStatusChanged() {
            if (FaceRec.status.length > 0) page.faceStatus = FaceRec.status
        }
        function onIdentified(name, score) {
            bumpIdle()
            page.faceStatus = ""
            if (!stackView) stackView = StackView.view
            stackView.push(Qt.resolvedUrl("MainPage.qml"), { userName: name })
        }
        function onUnknown(bestScore) {
            page.faceStatus = ""
            page.lastFaceError = qsTr("Face not recognised — please register first")
        }
        function onFailed(reason) {
            page.faceStatus = ""
            page.lastFaceError = reason
        }
    }
    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: Idle.touch()
    }

    // ===== QR note overlay =====
    property bool showQrNote: false

    function openQrNote() {
        bumpIdle()
        showQrNote = true
    }

    function goToMainFromNote() {
        bumpIdle()
        showQrNote = false
        if (!stackView) stackView = StackView.view
        if (stackView) stackView.push(Qt.resolvedUrl("MainPage.qml"))
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 90
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10

        Text {
            text: "ReWinGo"
            color: "#1F2A1B"
            font.pixelSize: 80
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            text: { langTick; return qsTr("Choose how to continue") }
            color: "#5A6B52"
            font.pixelSize: 26
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Column {
        width: parent.width * 0.88
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 80

        ChoiceTile {
            width: parent.width
            height: 540
            title: { langTick; return qsTr("Face Detection") }
            subtitle: { langTick; return qsTr("Use Face Scan") }
            iconKind: "face"

            onTapped: {
                bumpIdle()
                page.lastFaceError = ""
                page.faceStatus = qsTr("Starting face recognition…")
                FaceRec.identify()
            }
        }

        ChoiceTile {
            width: parent.width
            height: 540
            title: { langTick; return qsTr("QR Code") }
            subtitle: { langTick; return qsTr("Scan QR Code") }
            iconKind: "qr"

            onTapped: {
                bumpIdle()
                openQrNote()
            }
        }
    }

    // ===== QR Important note overlay =====
    Rectangle {
        id: qrOverlay
        anchors.fill: parent
        visible: showQrNote
        z: 9999
        color: "#F2F4ED"

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: bumpIdle()
        }

        // Back button on QR notice
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 30
            anchors.leftMargin: 30
            width: 90
            height: 90
            radius: 45
            color: "#FFFFFF"
            border.width: 2
            border.color: "#D8E0CF"
            z: 10

            TapHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                onTapped: {
                    bumpIdle()
                    showQrNote = false
                }
            }

            Text {
                anchors.centerIn: parent
                text: "←"
                font.pixelSize: 36
                color: "#1F2A1B"
            }
        }

        Rectangle {
            width: parent.width * 0.86
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            radius: 32
            color: "#FFF7D6"
            border.width: 2
            border.color: "#F59E0B"
            height: qrCol.implicitHeight + 80

            Column {
                id: qrCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 36
                spacing: 18

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 18

                    Rectangle {
                        width: 64
                        height: 64
                        radius: 32
                        color: "#F59E0B"
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "!"
                            color: "#FFFFFF"
                            font.pixelSize: 44
                            font.weight: Font.Black
                        }
                    }

                    Text {
                        text: { langTick; return qsTr("Important") }
                        color: "#92400E"
                        font.pixelSize: 50
                        font.weight: Font.ExtraBold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    text: { langTick; return qsTr("To use QR login, you must have the ReWinGo mobile app and a registered email.") }
                    wrapMode: Text.WordWrap
                    width: parent.width
                    color: "#1F2A1B"
                    font.pixelSize: 28
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { height: 6; width: 1 }

                Rectangle {
                    width: 360
                    height: 96
                    radius: 28
                    color: "#1A1D1A"
                    anchors.horizontalCenter: parent.horizontalCenter

                    TapHandler {
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                        onTapped: goToMainFromNote()
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 14

                        Text {
                            text: { langTick; return qsTr("Continue") }
                            color: "#FFFFFF"
                            font.pixelSize: 32
                            font.weight: Font.ExtraBold
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "→"
                            color: "#00E5FF"
                            font.pixelSize: 32
                            font.weight: Font.Black
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }

    // ===== Face Rec busy overlay =====
    Rectangle {
        anchors.fill: parent
        visible: FaceRec.running || page.faceStatus.length > 0
        color: "#CC000000"
        z: 9998
        TapHandler { acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen }   // swallow taps

        Column {
            anchors.centerIn: parent
            spacing: 24
            BusyIndicator {
                running: parent.visible
                width: 96; height: 96
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: page.faceStatus
                color: "#FFFFFF"
                font.pixelSize: 30
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                width: page.width * 0.8
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                visible: !FaceRec.running     // only useful once Python's window is up
                text: { langTick; return qsTr("The camera window will open in a moment. Blink twice, then turn right, then left.") }
                color: "#A5F3FC"
                font.pixelSize: 20
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                width: page.width * 0.8
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ===== Face Rec error/info toast =====
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 60
        visible: page.lastFaceError.length > 0
        z: 9999
        width: errorRow.implicitWidth + 60
        height: 80
        radius: 40
        color: "#1A1D1A"
        border.width: 2
        border.color: "#F59E0B"

        Row {
            id: errorRow
            anchors.centerIn: parent
            spacing: 14
            Text {
                text: "⚠"
                color: "#F59E0B"
                font.pixelSize: 32
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: page.lastFaceError
                color: "#FFFFFF"
                font.pixelSize: 22
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: page.lastFaceError = ""
        }
        // Auto-dismiss after 6 s.
        Timer {
            running: page.lastFaceError.length > 0
            interval: 6000
            onTriggered: page.lastFaceError = ""
        }
    }

    component ChoiceTile: Item {
        id: tile
        property string title: ""
        property string subtitle: ""
        property string iconKind: "qr"
        signal tapped()

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: tile.tapped()
        }

        // Drop shadow
        Rectangle {
            anchors.fill: card
            anchors.margins: -14
            radius: 40
            color: "#000000"
            opacity: 0.06
            y: 16
            z: -1
        }

        Rectangle {
            id: card
            anchors.fill: parent
            radius: 40
            color: "#FFFFFF"
            border.width: 2
            border.color: "#D8E0CF"
            LayoutMirroring.enabled: false

            Rectangle {
                width: 8
                radius: 4
                color: "#7A8B6A"
                opacity: 0.95
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 18
                anchors.bottomMargin: 18
            }

            // Icon area (left)
            Item {
                id: iconArea
                width: 360
                anchors.left: parent.left
                anchors.leftMargin: 40
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                Item {
                    id: iconBox
                    anchors.centerIn: parent
                    width: 250
                    height: 250
                    clip: true

                    // QR-style icon
                    Item {
                        visible: tile.iconKind === "qr"
                        anchors.fill: parent

                        Rectangle { x: 0; y: 0; width: 78; height: 78; radius: 18; color: "transparent"; border.width: 6; border.color: "#0891B2" }
                        Rectangle { x: parent.width - 78; y: 0; width: 78; height: 78; radius: 18; color: "transparent"; border.width: 6; border.color: "#0891B2" }
                        Rectangle { x: 0; y: parent.height - 78; width: 78; height: 78; radius: 18; color: "transparent"; border.width: 6; border.color: "#0891B2" }

                        Repeater {
                            model: 64
                            delegate: Rectangle {
                                width: 18; height: 18
                                radius: 6
                                color: (index % 5 === 0 || index % 7 === 0) ? "#0891B2" : "#D8E0CF"
                                x: (index % 8) * 28 + 18
                                y: Math.floor(index / 8) * 28 + 18
                                opacity: 0.95
                            }
                        }
                    }

                    // Face icon
                    Item {
                        visible: tile.iconKind === "face"
                        anchors.fill: parent

                        Rectangle {
                            width: 120; height: 120
                            radius: 60
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 20
                            color: "transparent"
                            border.width: 6
                            border.color: "#0891B2"
                        }

                        Rectangle {
                            width: 190; height: 115
                            radius: 58
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 140
                            color: "transparent"
                            border.width: 6
                            border.color: "#0891B2"
                        }
                    }

                    Rectangle {
                        id: scanLine
                        width: iconBox.width - 14
                        height: 6
                        radius: 3
                        color: "#00E5FF"
                        opacity: 0.85
                        x: 7
                        y: 18

                        SequentialAnimation on y {
                            loops: Animation.Infinite
                            NumberAnimation { from: 18; to: iconBox.height - scanLine.height - 18; duration: 1200; easing.type: Easing.InOutSine }
                            NumberAnimation { from: iconBox.height - scanLine.height - 18; to: 18; duration: 1200; easing.type: Easing.InOutSine }
                        }
                    }
                }
            }

            // Text area (right) — centered horizontally and vertically
            Item {
                anchors.left: iconArea.right
                anchors.right: parent.right
                anchors.rightMargin: 30
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                Column {
                    anchors.centerIn: parent
                    spacing: 14
                    width: parent.width - 20

                    Text {
                        text: tile.title
                        color: "#1F2A1B"
                        font.pixelSize: 56
                        font.weight: Font.ExtraBold
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: tile.subtitle
                        color: "#5A6B52"
                        font.pixelSize: 24
                        wrapMode: Text.WordWrap
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: { langTick; return qsTr("Tap to continue") }
                        color: "#0891B2"
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
    Rectangle {
        width: 80; height: 80; color: "red"
        anchors.top: parent.top; anchors.right: parent.right
        TapHandler { onTapped: appManager.devTriggerAdmin() }
    }
}
