import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * NewProductDialog — create a catalog entry.
 *
 * Flow:
 *   1. Admin types a name (e.g. "Coca Cola 330ml").
 *   2. Taps "🔍 Look up online" → OffClient hits Open Food Facts.
 *   3. Candidate cards appear — admin picks one → image URL + weight
 *      auto-fill below. Admin can still edit any field.
 *   4. Sets a price (always manual — depends on the kiosk's business).
 *   5. Save → CatalogModel.addOrUpdate() → row added, dialog closes,
 *      `created(id)` fires so the picker refreshes.
 *
 * If the network is dead or the product isn't on OFF, the admin just
 * fills in everything by hand — same dialog, no extra UI to learn.
 */
Dialog {
    id: dlg

    signal created(string id)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 820
    height: 1100
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "#C0000000" }
    standardButtons: Dialog.Save | Dialog.Cancel
    title: qsTr("New product")

    // Form state
    property string  pickedImageUrl: ""
    property string  pickedBarcode:  ""
    property var     candidates:     []
    property bool    searching:      false

    onOpened: {
        nameField.text   = ""
        priceField.text  = ""
        weightField.text = ""
        pickedImageUrl   = ""
        pickedBarcode    = ""
        candidates       = []
        previewImage.source = ""
    }

    // Auto-search: a short pause after the admin stops typing fires the
    // lookup automatically, so they "just type the name and images appear"
    // — no button press, no URLs. The manual button stays as a fallback.
    Timer {
        id: autoSearch
        interval: 700; repeat: false
        onTriggered: {
            if (nameField.text.trim().length >= 3 && !dlg.searching) {
                dlg.searching  = true
                dlg.candidates = []
                OffClient.search(nameField.text)
            }
        }
    }

    // ── OFF lookup hookup ─────────────────────────────────────────────────
    Connections {
        target: OffClient
        function onResults(query, list) {
            dlg.searching  = false
            dlg.candidates = list
        }
        function onSearchFailed(query, reason) {
            dlg.searching  = false
            dlg.candidates = []
            console.warn("OFF search failed:", reason)
        }
    }

    Column {
        anchors.fill: parent
        spacing: 12

        // ── Name + lookup ─────────────────────────────────────────────
        Text { text: qsTr("Product name"); color: "#5A6B52"; font.pixelSize: 14 }
        Row {
            width: parent.width
            spacing: 10
            TextField {
                id: nameField
                width: parent.width - 220
                placeholderText: qsTr("e.g. Coca Cola 330ml")
                font.pixelSize: 20
                onTextChanged: autoSearch.restart()   // images appear as you type
            }
            Rectangle {
                width: 210; height: 56; radius: 28
                color: nameField.text.trim().length > 0 ? "#0891B2" : "#9CA3AF"
                TapHandler {
                    enabled: nameField.text.trim().length > 0 && !dlg.searching
                    onTapped: {
                        dlg.searching  = true
                        dlg.candidates = []
                        OffClient.search(nameField.text)
                    }
                }
                Row {
                    anchors.centerIn: parent
                    spacing: 8
                    BusyIndicator {
                        running: dlg.searching
                        visible: dlg.searching
                        width: 24; height: 24
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: dlg.searching ? qsTr("Looking…")
                                            : qsTr("🔍 Look up online")
                        color: "#FFFFFF"
                        font.pixelSize: 16; font.weight: Font.ExtraBold
                    }
                }
            }
        }

        // ── Candidate strip (only when we have results) ───────────────
        Rectangle {
            visible: dlg.candidates.length > 0
            width: parent.width; height: 200; radius: 14
            color: "#FAFBF6"
            border.width: 1; border.color: "#D8E0CF"

            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Text {
                    text: qsTr("Found %1 match(es) on Open Food Facts — tap to use:")
                              .arg(dlg.candidates.length)
                    color: "#5A6B52"; font.pixelSize: 12
                }
                ListView {
                    width: parent.width
                    height: parent.height - 30
                    orientation: ListView.Horizontal
                    spacing: 10
                    clip: true
                    model: dlg.candidates
                    delegate: Rectangle {
                        width: 200; height: 160; radius: 12
                        color: "#FFFFFF"
                        border.width: 1; border.color: "#D8E0CF"
                        TapHandler {
                            onTapped: {
                                nameField.text   = modelData.name
                                weightField.text = modelData.weightG > 0
                                                   ? modelData.weightG : ""
                                dlg.pickedImageUrl = modelData.imageUrl
                                dlg.pickedBarcode  = modelData.barcode
                                previewImage.source = modelData.imageUrl
                            }
                        }
                        Column {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 4
                            Rectangle {
                                width: parent.width; height: 80; radius: 8
                                color: "#E8EEDB"
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    source: modelData.imageUrl
                                    fillMode: Image.PreserveAspectFit
                                    visible: modelData.imageUrl
                                }
                            }
                            Text {
                                text: modelData.name
                                color: "#1F2A1B"
                                font.pixelSize: 11; font.weight: Font.Bold
                                width: parent.width
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: (modelData.weightG > 0
                                       ? modelData.weightG + "g" : "?g")
                                      + "  ·  " + (modelData.brand || qsTr("(no brand)"))
                                color: "#5A6B52"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                width: parent.width
                            }
                        }
                    }
                }
            }
        }

        // ── Image preview + price + weight ────────────────────────────
        Row {
            width: parent.width
            spacing: 14

            Rectangle {
                width: 140; height: 140; radius: 12
                color: "#E8EEDB"
                border.width: 1; border.color: "#D8E0CF"
                Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.margins: 8
                    fillMode: Image.PreserveAspectFit
                    visible: source != ""
                }
                Text {
                    anchors.centerIn: parent
                    text: "📷"
                    font.pixelSize: 48
                    visible: previewImage.source == ""
                }
            }
            Column {
                width: parent.width - 160
                spacing: 10

                Text { text: qsTr("Price (points)")
                       color: "#5A6B52"; font.pixelSize: 14 }
                TextField {
                    id: priceField
                    width: parent.width
                    placeholderText: qsTr("50")
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 99999 }
                    font.pixelSize: 22
                }

                Text { text: qsTr("Typical weight (grams, per item)")
                       color: "#5A6B52"; font.pixelSize: 14 }
                TextField {
                    id: weightField
                    width: parent.width
                    placeholderText: qsTr("330")
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 99999 }
                    font.pixelSize: 22
                }
            }
        }

        Text {
            text: qsTr("The typical weight helps seed the per-item calibration. " +
                       "You can still tare each shelf later for exact counts.")
            color: "#5A6B52"; font.pixelSize: 12
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }

    onAccepted: {
        if (nameField.text.trim().length === 0) return
        const id = CatalogModel.addOrUpdate({
            name:           nameField.text.trim(),
            defaultPrice:   parseInt(priceField.text  || "0"),
            typicalWeightG: parseInt(weightField.text || "0"),
            imageUrl:       dlg.pickedImageUrl,
            imagePath:      dlg.pickedImageUrl ? "" : "", // imagePath gets filled by cache
            barcode:        dlg.pickedBarcode,
            source:         dlg.pickedImageUrl ? "lookup" : "manual"
        })
        if (id.length > 0) {
            dlg.created(id)
        }
    }
}
