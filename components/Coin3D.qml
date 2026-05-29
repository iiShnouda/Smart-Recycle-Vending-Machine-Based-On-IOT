import QtQuick
import QtQuick3D

/*
 * Coin3D — the real 3D RWG coin (Meshy model), same motion as RwgCoin:
 *   1. spin smoothly around the vertical axis, right → left (one full turn)
 *   2. stop a little
 *   3. bob a little upper
 *   4. then a little lower
 *   5. settle, stop a little, repeat
 *
 * The mesh is a disc of radius ~1 lying in the XY plane (it faces +Z), so a
 * camera on +Z sees it face-on and a Y-axis spin reads as a coin flip.
 *
 * Background: transparent by default so it sits cleanly on whatever pill /
 * surface is behind it. To give it a solid backdrop instead:
 *     Coin3D { transparentBg: false; bgColor: "#0891B2" }
 *
 * NOTE: this is the full 364k-face Meshy mesh — fine on a desktop GPU,
 * heavy on the Pi 4. For the Pi, decimate in Blender (Decimate modifier
 * keeps UVs) down to ~30k faces and re-run balsam.
 */
Item {
    id: root

    property int   size: 96
    property bool  transparentBg: true
    property color bgColor: "#000000"

    width: size
    height: size

    View3D {
        anchors.fill: parent
        // Offscreen render mode → composits with alpha over the QML behind it.
        renderMode: View3D.Offscreen

        environment: SceneEnvironment {
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            backgroundMode: root.transparentBg ? SceneEnvironment.Transparent
                                               : SceneEnvironment.Color
            clearColor: root.bgColor
        }

        PerspectiveCamera {
            id: cam
            z: 4.0
            fieldOfView: 40
            clipNear: 0.1
            clipFar: 100
        }

        // Key light (warm, upper-right) + softer fill so the gold reads well
        // even without an HDRI environment.
        DirectionalLight {
            eulerRotation.x: -25
            eulerRotation.y: -35
            brightness: 1.4
            color: "#FFF6E0"
        }
        DirectionalLight {
            eulerRotation.x: 25
            eulerRotation.y: 150
            brightness: 0.7
            color: "#FFE9B8"
        }

        // Pivot carries the up/down bob; the coin model carries the spin.
        Node {
            id: pivot
            Model {
                id: coin
                source: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/coin3d/coin.mesh"
                materials: PrincipledMaterial {
                    baseColorMap: Texture {
                        source: "qrc:/Recycle_Vending_Machine_LCD/resources/assets/coin3d/coin.png"
                        generateMipmaps: true
                        mipFilter: Texture.Linear
                    }
                    // Keep metalness low: with no HDRI to reflect, a fully
                    // metallic material renders dark. The texture already
                    // carries the gold; a touch of specular adds sheen.
                    metalness: 0.15
                    roughness: 0.35
                    specularAmount: 0.7
                }
                eulerRotation.y: 0
            }
        }

        SequentialAnimation {
            running: root.visible
            loops: Animation.Infinite

            // 1) smooth spin, right → left (one full turn)
            NumberAnimation {
                target: coin; property: "eulerRotation.y"
                from: 0; to: 360
                duration: 1500; easing.type: Easing.InOutSine
            }
            PauseAnimation { duration: 350 }            // 2) stop a little
            // 3) up
            NumberAnimation {
                target: pivot; property: "y"
                to: 0.30; duration: 360; easing.type: Easing.OutQuad
            }
            // 4) down
            NumberAnimation {
                target: pivot; property: "y"
                to: -0.25; duration: 460; easing.type: Easing.InOutQuad
            }
            // 5) settle
            NumberAnimation {
                target: pivot; property: "y"
                to: 0; duration: 300; easing.type: Easing.OutQuad
            }
            PauseAnimation { duration: 450 }
        }
    }
}
