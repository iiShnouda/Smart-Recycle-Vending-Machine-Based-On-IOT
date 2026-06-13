import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../components"

Rectangle {
    id: mainRect
    objectName: "mainPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view

    // User info — wire from your auth flow
    property string userName: "Shnouda"
    property int userPoints: 0

    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Idle handled by global IdleManager — no local timer.
    function bumpIdle() { Idle.touch() }

    Timer {
        id: navigationTimer
        interval: 240
        property var pageToLoad
        onTriggered: {
            if (mainRect.stackView) mainRect.stackView.push(pageToLoad)
        }
    }

    function resetState() {
        Idle.touch()
        navigationTimer.stop()
    }

    function exitToHome() {
        while (stackView && stackView.depth > 1) stackView.pop()
    }

    StackView.onActivated: resetState()
    Component.onCompleted: resetState()

    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: Idle.touch()
    }

    // ===== Top bar =====
    Item {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 24
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        height: 110

        // Points pill (top-left)
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            height: 100
            radius: 50
            width: pointsRow.implicitWidth + 50
            color: "#0891B2"

            Row {
                id: pointsRow
                anchors.centerIn: parent
                spacing: 12

                // Animated 3D RWG reward coin (transparent → dark pill shows
                // through). For a solid backdrop: transparentBg:false; bgColor:"#..."
                Coin3D {
                    anchors.verticalCenter: parent.verticalCenter
                    size: 92
                    transparentBg: true
                }

                Text {
                    text: mainRect.userPoints
                    color: "#FFFFFF"
                    font.pixelSize: 40
                    font.weight: Font.Black
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: { langTick; return qsTr("pts") }
                    color: "#FFFFFF"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Right side row: Admin button + Exit button.
        // The Admin button is the manual entry path while the STM32 reed
        // switch isn't wired up — once it is, this button can stay as a
        // backup for operator-on-site service.
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14

            // Admin button (red — distinct from the customer-facing controls).
            Rectangle {
                id: adminBtn
                width: 90
                height: 90
                radius: 45
                color: "#DC2626"

                // Tactile press: shrink with a little overshoot on release.
                scale: adminTap.pressed ? 0.90 : 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }

                TapHandler {
                    id: adminTap
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                    onTapped: {
                        bumpIdle()
                        appManager.devTriggerAdmin()
                    }
                }

                // White flash overlay while held — the "ink" feel.
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "#FFFFFF"
                    opacity: adminTap.pressed ? 0.22 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }

                Text {
                    anchors.centerIn: parent
                    text: { langTick; return qsTr("ADMIN") }
                    font.pixelSize: 18
                    font.weight: Font.Black
                    color: "#FFFFFF"
                }
            }

            // Exit button.
            Rectangle {
                id: exitBtn
                width: 90
                height: 90
                radius: 45
                color: exitTap.pressed ? "#EEF1E8" : "#FFFFFF"
                border.width: 2
                border.color: exitTap.pressed ? "#0891B2" : "#D8E0CF"

                scale: exitTap.pressed ? 0.90 : 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
                Behavior on color { ColorAnimation { duration: 120 } }
                Behavior on border.color { ColorAnimation { duration: 120 } }

                TapHandler {
                    id: exitTap
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                    onTapped: exitToHome()
                }

                Text {
                    anchors.centerIn: parent
                    text: { langTick; return qsTr("EXIT") }
                    font.pixelSize: 22
                    font.weight: Font.Black
                    color: "#1F2A1B"
                }
            }
        }
    }

    // ===== Brand header (welcome — bigger, centered) =====
    Column {
        id: header
        anchors.top: topBar.bottom
        anchors.topMargin: 28
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8

        Text {
            text: "ReWinGo"
            color: "#1F2A1B"
            font.pixelSize: 84
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
        }

        // Welcome line — bigger, centered.
        Text {
            text: { langTick; return qsTr("Welcome, ") + mainRect.userName }
            color: "#0891B2"
            font.pixelSize: 42
            font.weight: Font.ExtraBold
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            text: { langTick; return qsTr("Select a mode") }
            color: "#5A6B52"
            font.pixelSize: 22
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Column {
        width: parent.width * 0.86
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: header.bottom
        anchors.topMargin: 60
        spacing: 70

        KioskTile {
            width: parent.width
            height: 540
            title: { langTick; return qsTr("Vending") }
            subtitle: { langTick; return qsTr("Buy products using your earned points") }
            gifSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/Vending1.gif"
            iconSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/vending.png"

            onTapped: {
                bumpIdle()
                navigationTimer.pageToLoad = vendingPageComponent
                navigationTimer.start()
            }
        }

        KioskTile {
            width: parent.width
            height: 540
            title: { langTick; return qsTr("Recycle") }
            subtitle: { langTick; return qsTr("Plastic bottles and aluminum cans only") }
            gifSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/Recycle1.gif"
            iconSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/Recycle.png"

            onTapped: openRecycleNotice()
        }
    }

    Component { id: recyclePageComponent; RecycleWaitingPage {} }
    Component {
        id: vendingPageComponent
        VendingPage {
            userName: mainRect.userName
            userPoints: mainRect.userPoints
        }
    }

    // ===== Recycle confirmation modal =====
    property bool showRecycleNotice: false

    function openRecycleNotice() {
        bumpIdle()
        showRecycleNotice = true
    }

    function confirmRecycle() {
        Idle.touch()
        showRecycleNotice = false
        navigationTimer.pageToLoad = recyclePageComponent
        navigationTimer.start()
    }

    Rectangle {
        id: recycleNoticeOverlay
        anchors.fill: parent
        visible: showRecycleNotice
        z: 9999
        color: "#F2F4ED"

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: bumpIdle()
        }

        // Back button
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 30
            anchors.leftMargin: 30
            width: 90
            height: 90
            radius: 45
            color: backTap.pressed ? "#EEF1E8" : "#FFFFFF"
            border.width: 2
            border.color: backTap.pressed ? "#0891B2" : "#D8E0CF"
            z: 10

            scale: backTap.pressed ? 0.90 : 1.0
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            TapHandler {
                id: backTap
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                onTapped: {
                    bumpIdle()
                    showRecycleNotice = false
                }
            }

            Text {
                anchors.centerIn: parent
                text: "←"
                font.pixelSize: 36
                color: "#1F2A1B"
            }
        }

        Item {
            width: parent.width * 0.86
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: noticeCard.height

            Rectangle {
                anchors.fill: noticeCard
                anchors.margins: -14
                radius: 32
                color: "#000000"
                opacity: 0.08
                y: 16
                z: -1
            }

            Rectangle {
                id: noticeCard
                width: parent.width
                radius: 32
                color: "#FFF7D6"
                border.width: 2
                border.color: "#F59E0B"
                height: noticeColumn.implicitHeight + 80

                Column {
                    id: noticeColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 40
                    spacing: 22

                    Row {
                        spacing: 18
                        anchors.horizontalCenter: parent.horizontalCenter

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
                            text: { langTick; return qsTr("Important Notice") }
                            color: "#92400E"
                            font.pixelSize: 50
                            font.weight: Font.ExtraBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        text: { langTick; return qsTr("Please insert ONLY empty plastic bottles or empty aluminum cans.") }
                        color: "#1F2A1B"
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 10

                        Text {
                            text: { langTick; return "•  " + qsTr("Items must be empty and free of liquid") }
                            color: "#1F2A1B"
                            font.pixelSize: 22
                            wrapMode: Text.WordWrap
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            text: { langTick; return "•  " + qsTr("No glass, paper, food waste, or other materials") }
                            color: "#1F2A1B"
                            font.pixelSize: 22
                            wrapMode: Text.WordWrap
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            text: { langTick; return "•  " + qsTr("Crushed or contaminated items will be rejected") }
                            color: "#1F2A1B"
                            font.pixelSize: 22
                            wrapMode: Text.WordWrap
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    Item { height: 8; width: 1 }

                    Rectangle {
                        width: 360
                        height: 96
                        radius: 28
                        color: "#1A1D1A"
                        anchors.horizontalCenter: parent.horizontalCenter

                        scale: continueTap.pressed ? 0.95 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }

                        TapHandler {
                            id: continueTap
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
                            onTapped: confirmRecycle()
                        }

                        // Press flash overlay.
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: "#FFFFFF"
                            opacity: continueTap.pressed ? 0.16 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
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
    }

    // ===== Kiosk Tile =====
    component KioskTile: Item {
        id: tile
        property string title: ""
        property string subtitle: ""
        property string gifSource: ""
        property string iconSource: ""
        signal tapped()

        // Press feedback: the whole card dips slightly and its border
        // lights up in the ice-blue accent while held.
        scale: tileTap.pressed ? 0.975 : 1.0
        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutBack } }

        TapHandler {
            id: tileTap
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: {
                mainRect.bumpIdle()
                tile.tapped()
            }
        }

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
            border.width: tileTap.pressed ? 4 : 2
            border.color: tileTap.pressed ? "#0891B2" : "#D8E0CF"
            LayoutMirroring.enabled: false
            Behavior on border.color { ColorAnimation { duration: 130 } }
            Behavior on border.width { NumberAnimation { duration: 130 } }

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

            Item {
                id: tileIconArea
                width: 380
                anchors.left: parent.left
                anchors.leftMargin: 40
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                AnimatedImage {
                    id: gif
                    anchors.centerIn: parent
                    width: parent.width - 30
                    height: parent.height - 60
                    source: tile.gifSource
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: tile.gifSource !== "" && status === Image.Ready
                }

                Image {
                    anchors.centerIn: parent
                    width: parent.width - 30
                    height: parent.height - 60
                    source: tile.iconSource
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: !gif.visible
                }
            }

            Item {
                anchors.left: tileIconArea.right
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
                        font.pixelSize: 60
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
                        text: { mainRect.langTick; return qsTr("Tap to open") }
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
}
