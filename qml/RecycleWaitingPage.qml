import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../components"

Rectangle {
    id: waitingPage
    objectName: "recycleWaitingPage"
    color: "#F2F4ED"

    property StackView stackView: StackView.view
    property real plasticTankVolume: 0.45
    property real canTankVolume: 0.35

    // Translation refresh helper
    property int langTick: 0
    Connections {
        target: appManager
        function onLanguageChanged() { langTick++ }
    }

    // Idle timer is centralised in IdleManager (singleton "Idle").
    function bumpIdle() { Idle.touch() }

    // Arm the STM32 recycle lane on entry. When IR1 trips (EVT,ENTRY) the
    // session emits itemEntered → we move to the live counter and the STM32
    // runs the sort sequence. Guard so we navigate exactly once.
    property bool navigated: false
    Component.onCompleted: { Idle.disable(); RecycleSession.start() }
    StackView.onActivated:   Idle.disable()
    // Re-arm the idle timeout when leaving the recycle flow.
    StackView.onDeactivated: Idle.enable()

    Connections {
        target: RecycleSession
        function onItemEntered() {
            if (waitingPage.navigated) return
            waitingPage.navigated = true
            stackView.push(sessionPageComponent)
        }
    }
    Component { id: sessionPageComponent; RecycleSessionPage {} }

    // Leaving before any item entered → disarm the lane.
    function leave() {
        if (!waitingPage.navigated) RecycleSession.finish()
        stackView.pop()
    }

    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: Idle.touch()
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
        color: "#FFFFFF"
        border.width: 2
        border.color: "#D8E0CF"
        z: 10

        BounceOnPress { onTapped: waitingPage.leave() }

        Text {
            anchors.centerIn: parent
            text: "←"
            font.pixelSize: 36
            color: "#1F2A1B"
        }
    }

    // Exit button (top-right) — pop all the way home
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 30
        anchors.rightMargin: 30
        width: 90
        height: 90
        radius: 45
        color: "#FFFFFF"
        border.width: 2
        border.color: "#D8E0CF"
        z: 10

        BounceOnPress {
            onTapped: {
                if (!waitingPage.navigated) RecycleSession.finish()
                while (stackView && stackView.depth > 1) stackView.pop()
            }
        }

        Text {
            anchors.centerIn: parent
            text: "✕"
            font.pixelSize: 32
            font.weight: Font.Black
            color: "#1F2A1B"
        }
    }

    // ===== Header =====
    Column {
        id: header
        anchors.top: parent.top
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        Text {
            text: "ReWinGo"
            color: "#1F2A1B"
            font.pixelSize: 86
            font.weight: Font.Black
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: { langTick; return qsTr("Insert items to start recycling") }
            color: "#5A6B52"
            font.pixelSize: 24
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // ===== Two circle tanks =====
    Row {
        id: circlesRow
        anchors.top: header.bottom
        anchors.topMargin: 70
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 70

        // Display-only fill gauges. The real flow is hardware-driven: drop a
        // bottle/can, IR1 detects it, and we move to the live counter — there
        // are no manual "insert" test buttons.
        TankCircle {
            label: { langTick; return qsTr("Plastic Bottles") }
            iconSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/plastic-bottle.png"
            fillPercent: waitingPage.plasticTankVolume
            onInserted: Idle.touch()
        }

        TankCircle {
            label: { langTick; return qsTr("Aluminum Cans") }
            iconSource: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/soda-can.png"
            fillPercent: waitingPage.canTankVolume
            onInserted: Idle.touch()
        }
    }

    // ===== Strict notice =====
    Item {
        id: noticeWrap
        anchors.top: circlesRow.bottom
        anchors.topMargin: 70
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.86
        height: noticeCard.height

        Rectangle {
            anchors.fill: noticeCard
            anchors.margins: -10
            radius: 26
            color: "#000000"
            opacity: 0.06
            y: 12
            z: -1
        }

        Rectangle {
            id: noticeCard
            width: parent.width
            radius: 22
            color: "#FFF7D6"
            border.width: 2
            border.color: "#F59E0B"
            height: noticeRow.implicitHeight + 50

            Row {
                id: noticeRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 20

                Rectangle {
                    width: 70
                    height: 70
                    radius: 35
                    color: "#F59E0B"
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        color: "#FFFFFF"
                        font.pixelSize: 46
                        font.weight: Font.Black
                    }
                }

                Column {
                    width: parent.width - 90 - 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: { langTick; return qsTr("STRICT NOTICE") }
                        color: "#92400E"
                        font.pixelSize: 22
                        font.weight: Font.Black
                        font.letterSpacing: 1.2
                    }

                    Text {
                        text: { langTick; return qsTr("Insert ONLY empty plastic bottles or empty aluminum cans. Anything else will be rejected.") }
                        color: "#1F2A1B"
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }
        }
    }

    // ===== Fading attention message =====
    Column {
        anchors.top: noticeWrap.bottom
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8

        Text {
            id: attentionText
            text: { langTick; return qsTr("Drop your bottle or can to begin recycling") }
            color: "#0891B2"
            font.pixelSize: 32
            font.weight: Font.ExtraBold
            anchors.horizontalCenter: parent.horizontalCenter
            opacity: 1.0
            style: Text.Outline
            styleColor: "#A5F3FC"

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 0.35; duration: 1400; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.00; duration: 1400; easing.type: Easing.InOutQuad }
            }
        }

    }

    // ===== Counting page transition =====
    Component {
        id: countingPageComponent
        RecycleCountingPage {
            plasticTankVolume: waitingPage.plasticTankVolume
            canTankVolume: waitingPage.canTankVolume
        }
    }

    // ===== TankCircle component =====
    component TankCircle: Item {
        id: tank
        property string label: ""
        property string iconSource: ""
        property real fillPercent: 0.5
        signal inserted()

        // Sized for 1080-wide kiosk: two tanks × 440 + 70 spacing = 950
        // (was 480 each → tight at 1080)
        width: 440
        height: 540

        TapHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onTapped: tank.inserted()
        }

        // Drop shadow
        Rectangle {
            anchors.fill: outerRing
            anchors.margins: -10
            radius: width / 2
            color: "#000000"
            opacity: 0.07
            y: 14
            z: -1
        }

        // Outer ring (sage matte)
        Rectangle {
            id: outerRing
            width: 440
            height: 440
            radius: 220
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#FFFFFF"
            border.color: "#7A8B6A"
            border.width: 6

            // Inner ring (matte black thin border for "interior" look)
            Rectangle {
                anchors.fill: parent
                anchors.margins: 14
                radius: width / 2
                color: "#1A1D1A"

                // Liquid fill canvas (clipped to circle, fills from bottom)
                Canvas {
                    id: fillCanvas
                    anchors.fill: parent
                    anchors.margins: 4
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var w = width
                        var h = height
                        var cx = w / 2
                        var cy = h / 2
                        var r = Math.min(w, h) / 2 - 2
                        var fp = Math.max(0, Math.min(1, tank.fillPercent))
                        var fillTop = h - h * fp

                        // Black matte interior background fill (full circle)
                        ctx.save()
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                        ctx.clip()

                        ctx.fillStyle = "#1A1D1A"
                        ctx.fillRect(0, 0, w, h)

                        // Ice-blue LED-style fill rising from bottom
                        var grad = ctx.createLinearGradient(0, fillTop, 0, h)
                        grad.addColorStop(0.0, "rgba(0, 229, 255, 0.55)")
                        grad.addColorStop(0.5, "rgba(0, 229, 255, 0.75)")
                        grad.addColorStop(1.0, "rgba(165, 243, 252, 0.85)")
                        ctx.fillStyle = grad
                        ctx.fillRect(0, fillTop, w, h - fillTop)

                        // Bright top-of-water line (LED rim)
                        ctx.strokeStyle = "rgba(165, 243, 252, 0.95)"
                        ctx.lineWidth = 4
                        ctx.beginPath()
                        ctx.moveTo(0, fillTop)
                        ctx.lineTo(w, fillTop)
                        ctx.stroke()

                        ctx.restore()

                        // Subtle outer LED glow ring inside black interior
                        ctx.strokeStyle = "rgba(0, 229, 255, 0.35)"
                        ctx.lineWidth = 2
                        ctx.beginPath()
                        ctx.arc(cx, cy, r - 2, 0, 2 * Math.PI)
                        ctx.stroke()
                    }
                }

                // Bottle / can image in center
                Image {
                    anchors.centerIn: parent
                    width: 180
                    height: 250
                    source: tank.iconSource
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    z: 10
                }
            }
        }

        // Repaint canvas whenever fill changes
        onFillPercentChanged: fillCanvas.requestPaint()
        Component.onCompleted: fillCanvas.requestPaint()

        // Label + percentage below the circle
        Column {
            anchors.top: outerRing.bottom
            anchors.topMargin: 20
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 2

            Text {
                text: tank.label
                color: "#1F2A1B"
                font.pixelSize: 32
                font.weight: Font.ExtraBold
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: Math.round(tank.fillPercent * 100) + "% " + qsTr("full")
                color: "#0891B2"
                font.pixelSize: 24
                font.weight: Font.DemiBold
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
