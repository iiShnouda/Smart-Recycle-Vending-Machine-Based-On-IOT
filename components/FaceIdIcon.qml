import QtQuick

/*
 * FaceIdIcon — iPhone Face ID-style icon, drawn entirely in QML.
 *
 *   ┌─┐                ┌─┐
 *      ╭─────╮        ◄── rounded outer brackets
 *      │ 👀 │
 *      │  ⌣  │         ◄── smile + eyes
 *      ╰─────╯
 *   └─┘                └─┘
 *
 * Properties:
 *   color       — line/face color (sage / blue / etc.)
 *   bgColor     — background color (matches your dark/light card)
 *   scanLine    — true → animated scan line passes top to bottom
 */
Item {
    id: root
    property color color:     "#7A8B6A"
    property color bgColor:   "transparent"
    property bool  scanLine:  false

    // Square aspect
    implicitWidth: 180
    implicitHeight: 180

    Rectangle {
        anchors.fill: parent
        radius: width * 0.22
        color: root.bgColor
    }

    // ── Four corner brackets ────────────────────────────────────────
    Repeater {
        model: [
            { dx: 0,           dy: 0,           rot:   0 },   // top-left
            { dx: 1,           dy: 0,           rot:  90 },   // top-right
            { dx: 1,           dy: 1,           rot: 180 },   // bottom-right
            { dx: 0,           dy: 1,           rot: 270 }    // bottom-left
        ]
        Item {
            width: root.width * 0.22
            height: root.height * 0.22
            x: modelData.dx * (root.width  - width)
            y: modelData.dy * (root.height - height)
            rotation: modelData.rot
            Rectangle {
                // horizontal arm
                width: parent.width
                height: parent.height * 0.16
                color: root.color
                radius: height / 2
                anchors.top: parent.top
                anchors.left: parent.left
            }
            Rectangle {
                // vertical arm
                width: parent.width * 0.16
                height: parent.height
                color: root.color
                radius: width / 2
                anchors.top: parent.top
                anchors.left: parent.left
            }
        }
    }

    // ── Face inside (eyes + smile) ─────────────────────────────────
    Item {
        anchors.centerIn: parent
        width: root.width * 0.45
        height: root.height * 0.45

        // Left eye
        Rectangle {
            width: parent.width * 0.10
            height: parent.height * 0.22
            radius: width
            color: root.color
            x: parent.width * 0.22
            y: parent.height * 0.18
        }
        // Right eye
        Rectangle {
            width: parent.width * 0.10
            height: parent.height * 0.22
            radius: width
            color: root.color
            x: parent.width * 0.68
            y: parent.height * 0.18
        }

        // Nose (small vertical line)
        Rectangle {
            width: parent.width * 0.04
            height: parent.height * 0.18
            radius: width / 2
            color: root.color
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.42
        }

        // Smile (Canvas arc — most reliable shape primitive)
        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = root.color
                ctx.lineWidth   = Math.max(2, width * 0.04)
                ctx.lineCap     = "round"
                ctx.beginPath()
                ctx.arc(width/2, height * 0.55,
                        width * 0.30,
                        Math.PI * 0.20, Math.PI - Math.PI * 0.20)
                ctx.stroke()
            }
            Component.onCompleted: requestPaint()
        }
    }

    // ── Optional animated scan line ────────────────────────────────
    Rectangle {
        visible: root.scanLine
        width: parent.width * 0.85
        height: 4
        radius: 2
        color: "#00E5FF"
        opacity: 0.85
        x: parent.width * 0.075
        SequentialAnimation on y {
            running: root.scanLine
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.15; to: root.height * 0.80
                              duration: 1300; easing.type: Easing.InOutSine }
            NumberAnimation { from: root.height * 0.80; to: root.height * 0.15
                              duration: 1300; easing.type: Easing.InOutSine }
        }
    }
}
