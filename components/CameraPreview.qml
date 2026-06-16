import QtQuick

/*
 * CameraPreview — flicker-free circular live preview.
 *
 * Reads the face/recognition sidecar's frame via the C++ image provider
 * (image://facepreview/<n>) and DOUBLE-BUFFERS it: a timer loads the next
 * frame into the hidden image; only when it's fully decoded does it crossfade
 * to the front. The previous frame stays visible until then, so there's never
 * a blank gap (the single-Image swap flickered at ~11 fps). Decode happens on
 * the image-loading thread (asynchronous), so the GUI stays smooth.
 *
 * The square camera frame is masked into a circle with a Canvas "donut"
 * (clip:true on a rounded rect renders blank on the Pi GPU).
 */
Item {
    id: root
    property color maskColor: "#F2F4ED"   // page background, painted over the corners
    property int   fps: 12
    property bool  circular: true         // false → rectangular (e.g. barcode)
    property int   cornerRadius: 18       // used only when !circular

    property int tick: 0
    property Item frontImg: imgA

    // Dark placeholder shown until the first frame is ready.
    Rectangle {
        anchors.fill: parent
        radius: root.circular ? width / 2 : root.cornerRadius
        color: "#1A1D1A"
    }

    Image {
        id: imgA
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        cache: false; asynchronous: true
        opacity: root.frontImg === imgA ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 80 } }
        onStatusChanged: if (status === Image.Ready) root.frontImg = imgA
    }
    Image {
        id: imgB
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        cache: false; asynchronous: true
        opacity: root.frontImg === imgB ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 80 } }
        onStatusChanged: if (status === Image.Ready) root.frontImg = imgB
    }

    Timer {
        interval: Math.max(40, 1000 / root.fps); running: true; repeat: true
        onTriggered: {
            var back = (root.frontImg === imgA) ? imgB : imgA
            back.source = "image://facepreview/" + (++root.tick)
        }
    }

    // Mask the square frame into a circle (rect minus circle, even-odd).
    // Skipped entirely for the rectangular (barcode) variant.
    Canvas {
        anchors.fill: parent
        visible: root.circular
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.circular) return
            ctx.fillStyle = root.maskColor
            ctx.beginPath()
            ctx.rect(0, 0, width, height)
            ctx.arc(width / 2, height / 2, width / 2, 0, 2 * Math.PI, true)
            ctx.fill("evenodd")
        }
    }
}
