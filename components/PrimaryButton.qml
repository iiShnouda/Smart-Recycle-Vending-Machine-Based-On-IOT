import QtQuick
import QtQuick.Controls

Button {
    id: b
    font.pixelSize: 26
    padding: 18

    background: Rectangle {
        radius: 18
        color: b.down ? "#35FFFFFF" : "#22FFFFFF"
        border.color: b.hovered ? Theme.accent : Theme.glassBorder
        border.width: 1

        // glow-ish accent line
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
            radius: 3
            color: Theme.accent
            opacity: b.hovered ? 0.9 : 0.55
        }
    }

    contentItem: Text {
        text: b.text
        color: Theme.text
        font.pixelSize: b.font.pixelSize
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    // premium press animation
    scale: b.down ? 0.98 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.fast } }
}
