import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * CatalogPickerDialog — admin picks a product from the shared catalog.
 *
 * Workflow:
 *   - Search box filters by name
 *   - Tap a card → emits picked(catalogId, name, imagePath, price, weightG)
 *   - "+ Add new product" → opens NewProductDialog inline; on save the new
 *     row appears in the grid automatically (CatalogModel.reload() fires
 *     a model reset).
 *
 * This dialog DOES NOT touch the slot directly — it just emits the chosen
 * id. The caller decides what to do with it (typically: assignToSlot +
 * fill the slot edit form's fields).
 */
Dialog {
    id: dlg

    signal picked(string id, string name, string imagePath,
                  int price, int weightG)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 940
    height: 1300
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "#B0000000" }
    standardButtons: Dialog.Close
    title: qsTr("Pick from catalog")

    // Refresh on open — captures any rows synced from other kiosks.
    onOpened: { CatalogModel.reload(); searchField.text = "" }

    Column {
        anchors.fill: parent
        spacing: 14

        // ── Search + add row ───────────────────────────────────────────
        Row {
            width: parent.width
            spacing: 10

            TextField {
                id: searchField
                width: parent.width - 280
                height: 62
                placeholderText: qsTr("🔍  Search products…")
                font.pixelSize: 20
                onTextChanged: CatalogModel.reload(text)
            }

            Rectangle {
                width: 270; height: 62; radius: 31
                color: "#16A34A"
                TapHandler { onTapped: newProductDialog.open() }
                Text { anchors.centerIn: parent
                       text: qsTr("+ Add new product")
                       color: "#FFFFFF"
                       font.pixelSize: 18; font.weight: Font.ExtraBold }
            }
        }

        Rectangle { width: parent.width; height: 2; color: "#D8E0CF" }

        // ── Catalog grid ──────────────────────────────────────────────
        ScrollView {
            width: parent.width
            height: parent.height - 110
            clip: true

            GridView {
                id: gridView
                width: parent.width
                cellWidth:  parent.width / 3
                cellHeight: 280
                model: CatalogModel
                interactive: true

                // Empty-state hint. GridView exposes a `count` of its own.
                Item {
                    anchors.centerIn: parent
                    visible: gridView.count === 0
                    Column {
                        spacing: 10
                        anchors.centerIn: parent
                        Text { text: qsTr("Catalog is empty")
                               color: "#1F2A1B"
                               font.pixelSize: 22; font.weight: Font.ExtraBold
                               anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: qsTr("Tap “+ Add new product” to create your first item.")
                               color: "#5A6B52"
                               font.pixelSize: 14
                               anchors.horizontalCenter: parent.horizontalCenter }
                    }
                }

                delegate: Item {
                    width: GridView.view.cellWidth - 12
                    height: GridView.view.cellHeight - 12

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 6
                        radius: 18
                        color: "#FFFFFF"
                        border.width: 2
                        border.color: "#D8E0CF"

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            Rectangle {
                                width: parent.width; height: 130; radius: 12
                                color: "#E8EEDB"
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    source: imagePath || ""
                                    fillMode: Image.PreserveAspectFit
                                    visible: imagePath !== ""
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "📦"
                                    font.pixelSize: 48
                                    visible: imagePath === ""
                                }
                            }
                            Text {
                                text: name
                                color: "#1F2A1B"
                                font.pixelSize: 16; font.weight: Font.ExtraBold
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 8
                                Text {
                                    text: defaultPrice + qsTr(" pts")
                                    color: "#0891B2"
                                    font.pixelSize: 14; font.weight: Font.Bold
                                }
                                Text { text: "·"; color: "#9CA3AF" }
                                Text {
                                    text: typicalWeightG > 0 ? (typicalWeightG + "g") : qsTr("?g")
                                    color: "#5A6B52"
                                    font.pixelSize: 14
                                }
                            }
                            Rectangle {
                                width: parent.width; height: 32; radius: 16
                                color: "#0891B2"
                                anchors.horizontalCenter: parent.horizontalCenter
                                TapHandler {
                                    onTapped: {
                                        dlg.picked(id, name, imagePath,
                                                   defaultPrice, typicalWeightG)
                                        dlg.close()
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("Use this")
                                    color: "#FFFFFF"
                                    font.pixelSize: 13; font.weight: Font.ExtraBold
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // The nested "create new" dialog lives inside the picker so the user
    // can keep typing in the search box behind it.
    NewProductDialog {
        id: newProductDialog
        onCreated: (id) => { CatalogModel.reload() }
    }
}
