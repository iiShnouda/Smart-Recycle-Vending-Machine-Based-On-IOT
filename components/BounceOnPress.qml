import QtQuick

/*
 * BounceOnPress — drop-in replacement for a bare TapHandler that also makes
 * the button it sits in shrink ("bounce") while pressed, then spring back
 * on release. Gives every button a tactile clicked feel with one line.
 *
 * Usage — replace:
 *     TapHandler {
 *         acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
 *         onTapped: doThing()
 *     }
 * with:
 *     BounceOnPress { onTapped: doThing() }
 *
 * It fills its parent, drives parent.scale via a state/transition, and
 * forwards the tap. acceptedDevices + enabled are exposed as aliases.
 */
Item {
    id: root
    anchors.fill: parent
    z: 5                                   // above sibling visuals, below nothing important

    property real pressedScale: 0.93
    property int  springMs: 120
    property alias enabled: th.enabled
    property alias acceptedDevices: th.acceptedDevices
    property alias pressed: th.pressed
    signal tapped()

    TapHandler {
        id: th
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
        onTapped: root.tapped()
    }

    states: State {
        name: "down"; when: th.pressed
        PropertyChanges { target: root.parent; scale: root.pressedScale }
    }
    transitions: Transition {
        NumberAnimation {
            target: root.parent; property: "scale"
            duration: root.springMs; easing.type: Easing.OutBack
        }
    }
}
