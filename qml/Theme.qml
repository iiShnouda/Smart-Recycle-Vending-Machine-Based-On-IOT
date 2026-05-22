pragma Singleton
import QtQuick

QtObject {
    // ===== ReWinGo brand palette =====
    // Sage matte exterior, black matte interior, ice-blue LED accents.

    // Surface
    readonly property color bg:           "#F2F4ED"   // warm off-white, sage tint
    readonly property color bgAlt:        "#E8EEDB"   // light sage tint background
    readonly property color card:         "#FFFFFF"
    readonly property color cardBorder:   "#D8E0CF"   // subtle sage border
    readonly property color overlay:      "#FAFCF7"

    // Text
    readonly property color textPrimary:  "#1F2A1B"
    readonly property color textMuted:    "#5A6B52"
    readonly property color textSubtle:   "#8A9B82"
    readonly property color textOnInk:    "#FFFFFF"

    // Sage greens (exterior matte)
    readonly property color sage:         "#7A8B6A"
    readonly property color sageDeep:     "#4A5A3F"
    readonly property color sageSoft:     "#B5C4A0"

    // Matte black (interior)
    readonly property color ink:          "#1A1D1A"
    readonly property color inkSoft:      "#2D332E"

    // Ice blue LED accent
    readonly property color ice:          "#00E5FF"
    readonly property color iceLight:     "#A5F3FC"
    readonly property color iceDeep:      "#0891B2"

    // Warning (for important notices)
    readonly property color warnBg:       "#FFF7D6"
    readonly property color warnBorder:   "#F59E0B"
    readonly property color warnText:     "#92400E"

    // Motion
    readonly property int fast:   140
    readonly property int normal: 220
    readonly property int slow:   420

    // Radii
    readonly property int rTile: 40
    readonly property int rPanel: 28
    readonly property int rChip: 16
}
