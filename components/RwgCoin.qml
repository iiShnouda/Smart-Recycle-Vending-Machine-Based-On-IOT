import QtQuick

/*
 * RwgCoin — the animated RWG reward coin.
 *
 * Animation loop (matches the spec):
 *   1. spin smoothly around the vertical axis, right → left (one full turn)
 *   2. stop a little
 *   3. bob a little upper
 *   4. then a little lower
 *   5. settle back to centre, stop a little
 *   6. repeat forever
 *
 * Both motions use `transform` (Rotation + Translate) rather than the
 * geometry/anchors, so the coin never disturbs the Row / layout it sits in.
 * The loop pauses automatically when the coin isn't visible (saves CPU on
 * the Pi when another page is on top).
 */
Image {
    id: coin

    // The only knob callers need: the on-screen diameter.
    property int size: 64

    width: size
    height: size
    source: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/rwg-coin.png"
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
    asynchronous: true

    transform: [
        Translate { id: bob; y: 0 },
        Rotation {
            id: spin
            origin.x: coin.width  / 2
            origin.y: coin.height / 2
            axis { x: 0; y: 1; z: 0 }     // vertical axis → the face turns right→left
            angle: 0
        }
    ]

    SequentialAnimation {
        running: coin.visible
        loops: Animation.Infinite

        // 1) smooth spin, right → left (one full turn)
        NumberAnimation {
            target: spin; property: "angle"
            from: 0; to: 360
            duration: 1500; easing.type: Easing.InOutSine
        }
        PauseAnimation { duration: 350 }            // 2) stop a little

        // 3) go a little upper
        NumberAnimation {
            target: bob; property: "y"
            to: -coin.size * 0.16; duration: 360; easing.type: Easing.OutQuad
        }
        // 4) then a little lower
        NumberAnimation {
            target: bob; property: "y"
            to:  coin.size * 0.12; duration: 460; easing.type: Easing.InOutQuad
        }
        // 5) settle back to centre
        NumberAnimation {
            target: bob; property: "y"
            to: 0; duration: 300; easing.type: Easing.OutQuad
        }
        PauseAnimation { duration: 450 }            // stop a little, then redo
    }
}
