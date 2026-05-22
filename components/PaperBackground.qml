import QtQuick

/*
 *  PaperBackground — warm cream paper with a subtle gradient + grain.
 *  Drop this in as the first child of any page Rectangle so the texture
 *  shows through. The grain is drawn with a Canvas pattern so it ships
 *  with the binary (no PNG asset required).
 */
Item {
    id: root
    anchors.fill: parent

    // Soft vertical gradient — paper at top, slightly warmer at bottom.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#F5EFE3" }
            GradientStop { position: 1.0; color: "#EFE6D4" }
        }
    }

    // Decorative top-right "sun" disc — very subtle, like a magazine print
    // colour spot. Sits behind everything else.
    Rectangle {
        width: 720; height: 720; radius: width / 2
        x: parent.width - width * 0.55
        y: -height * 0.45
        color: "#E8C988"
        opacity: 0.18
    }

    // Tiny pulled-out grain layer — adds texture without screaming.
    Canvas {
        anchors.fill: parent
        opacity: 0.06
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const step = 3
            for (let y = 0; y < height; y += step) {
                for (let x = 0; x < width; x += step) {
                    if (Math.random() > 0.78) {
                        ctx.fillStyle = "#1A201A"
                        ctx.fillRect(x, y, 1, 1)
                    }
                }
            }
        }
        Component.onCompleted: requestPaint()
    }

    // Hairline frame — like the inner margin of a printed magazine page.
    Rectangle {
        anchors.fill: parent
        anchors.margins: 24
        color: "transparent"
        border.width: 1
        border.color: "#D4A574"
        opacity: 0.35
    }
}
