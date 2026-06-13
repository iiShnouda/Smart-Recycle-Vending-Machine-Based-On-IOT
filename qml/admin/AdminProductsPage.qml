import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD

/*
 * AdminProductsPage — manage the 8 vending slots.
 *
 * Visual design:
 *   - 2-column grid that fills the screen width.
 *   - "Configured" slots (have a name) → white card with full product info.
 *   - "Empty" slots (no name yet)      → dashed-border placeholder with "+
 *     Tap to configure" CTA — invites the admin to fill it in.
 *   - Active/inactive shown via a switch-style badge in the corner.
 */
Rectangle {
    id: page
    objectName: "adminProductsPage"
    color: "#EFF3EA"            // slightly warmer sage background

    property StackView stackView: StackView.view
    property int editingSlot: -1

    Component.onCompleted: Idle.disable()
    StackView.onActivated: Idle.disable()

    // ══════════════════════════ HEADER ══════════════════════════
    Rectangle {
        id: headerBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 160
        color: "#1F2A1B"          // dark sage banner

        // Back button (top-left)
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 30
            width: 80; height: 80; radius: 40
            color: "#FFFFFF"
            TapHandler { onTapped: stackView.pop() }
            Text { anchors.centerIn: parent; text: "←"
                   font.pixelSize: 36; color: "#1F2A1B" }
        }

        Column {
            anchors.centerIn: parent
            spacing: 4
            Text { text: qsTr("Products")
                   color: "#FFFFFF"
                   font.pixelSize: 50; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Configure your 8 vending slots")
                   color: "#A5F3FC"
                   font.pixelSize: 18
                   anchors.horizontalCenter: parent.horizontalCenter }
        }

        // Reload button (top-right)
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 30
            width: 80; height: 80; radius: 40
            color: prodReloadTap.pressed ? "#0E7490" : "transparent"
            border.width: 2; border.color: "#A5F3FC"
            scale: prodReloadTap.pressed ? 0.9 : 1.0
            Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }
            TapHandler { id: prodReloadTap; onTapped: { prodSpin.restart(); ProductsModel.reload() } }
            Text { id: prodReloadIcon; anchors.centerIn: parent; text: "↻"
                   color: "#A5F3FC"; font.pixelSize: 36; font.weight: Font.Black
                   RotationAnimation { id: prodSpin; target: prodReloadIcon; from: 0; to: 360
                                       duration: 500; easing.type: Easing.OutCubic; running: false } }
        }
    }

    // ══════════════════════════ GRID ══════════════════════════
    GridView {
        id: grid
        enabled: !editDialog.opened && !imagePicker.opened
        opacity:  enabled ? 1.0 : 0.35

        anchors.top: headerBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        cellWidth: width / 2
        cellHeight: (height) / 4    // 4 rows × 2 cols = 8 slots, fill screen
        clip: true
        model: ProductsModel

        delegate: Item {
            width: grid.cellWidth - 16
            height: grid.cellHeight - 16
            x: 8; y: 8

            // Empty if no name has been set
            readonly property bool isEmpty: name === "" || name.startsWith("Slot ")
            readonly property bool isActive: active === true || active === 1

            // ── Shadow ──
            Rectangle {
                anchors.fill: card
                anchors.margins: -8
                radius: 28
                color: "#000000"; opacity: 0.05
                y: 8; z: -1
            }

            // ── Card ──
            Rectangle {
                id: card
                anchors.fill: parent
                anchors.margins: 8
                radius: 24
                color: isEmpty ? "transparent" : "#FFFFFF"
                border.width: isEmpty ? 3 : 2
                border.color: isEmpty   ? "#7A8B6A"
                            : !isActive ? "#DC2626"
                                        : "#7A8B6A"
                // dashed border for empty (simulated with antialiased line)

                // ── Empty state ──
                Column {
                    visible: isEmpty
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        width: 70; height: 70; radius: 35
                        color: "#7A8B6A"
                        anchors.horizontalCenter: parent.horizontalCenter
                        Text { anchors.centerIn: parent; text: "+"
                               color: "#FFFFFF"
                               font.pixelSize: 48; font.weight: Font.Black }
                    }
                    Text { text: qsTr("Slot %1").arg(slot)
                           color: "#1F2A1B"
                           font.pixelSize: 28; font.weight: Font.ExtraBold
                           anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: qsTr("Tap to configure")
                           color: "#5A6B52"; font.pixelSize: 16
                           anchors.horizontalCenter: parent.horizontalCenter }
                }

                // ── Configured state ──
                Item {
                    visible: !isEmpty
                    anchors.fill: parent

                    // Slot pill (top-left)
                    Rectangle {
                        width: 64; height: 32; radius: 16
                        color: "#1A1D1A"
                        anchors.top: parent.top; anchors.left: parent.left
                        anchors.margins: 14
                        Text { anchors.centerIn: parent
                               text: "#" + slot
                               color: "#FFFFFF"
                               font.pixelSize: 15; font.weight: Font.ExtraBold }
                    }

                    // Status badge (top-right)
                    Rectangle {
                        width: 100; height: 32; radius: 16
                        color: !isActive ? "#DC2626"
                             : count > 0  ? "#16A34A"
                                          : "#9CA3AF"
                        anchors.top: parent.top; anchors.right: parent.right
                        anchors.margins: 14
                        Text {
                            anchors.centerIn: parent
                            text: !isActive ? qsTr("OFF")
                                : count > 0  ? "✓ " + count
                                             : qsTr("EMPTY")
                            color: "#FFFFFF"
                            font.pixelSize: 14; font.weight: Font.ExtraBold
                        }
                    }

                    // Image
                    Rectangle {
                        id: thumb
                        anchors.top: parent.top; anchors.topMargin: 60
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 130; height: 130; radius: 16
                        color: "#F2F4ED"
                        Image {
                            anchors.fill: parent; anchors.margins: 8
                            source: imagePath
                            fillMode: Image.PreserveAspectFit
                            visible: imagePath.length > 0
                        }
                        Text { anchors.centerIn: parent; text: "□"
                               font.pixelSize: 60; color: "#9CA3AF"
                               visible: imagePath.length === 0 }
                    }

                    // Name + price
                    Column {
                        anchors.top: thumb.bottom; anchors.topMargin: 14
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 2
                        width: parent.width - 30
                        Text { text: name
                               color: "#1F2A1B"
                               font.pixelSize: 22; font.weight: Font.ExtraBold
                               elide: Text.ElideRight
                               anchors.horizontalCenter: parent.horizontalCenter
                               horizontalAlignment: Text.AlignHCenter
                               width: parent.width }
                        Text { text: pricePoints + " " + qsTr("pts")
                               color: "#0891B2"
                               font.pixelSize: 22; font.weight: Font.Black
                               anchors.horizontalCenter: parent.horizontalCenter }
                    }
                }

                TapHandler {
                    onTapped: {
                        page.editingSlot   = slot
                        editName.text      = isEmpty ? "" : name
                        editPrice.text     = pricePoints
                        editImage.text     = imagePath
                        editActive.checked = isActive
                        editDialog.open()
                    }
                }
            }
        }
    }

    // ══════════════════════════ EDIT DIALOG ══════════════════════════
    Dialog {
        id: editDialog
        title: qsTr("Slot ") + page.editingSlot
        parent: Overlay.overlay
        anchors.centerIn: parent
        // Sized for 1080-wide kiosk; tall enough for calibration block.
        width: 880
        height: 1200
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: "#B0000000" }
        standardButtons: Dialog.Save | Dialog.Cancel

        // Refresh the cached calibration whenever the model fires a change,
        // so the "Live reading" row updates in real time as scans roll in.
        property int liveRaw:      page.editingSlot >= 1 ? ProductsModel.lastRaw(page.editingSlot) : 0
        property int emptyShelf:   page.editingSlot >= 1 ? ProductsModel.emptyShelfRaw(page.editingSlot) : 0
        property int unitWeight:   page.editingSlot >= 1 ? ProductsModel.unitWeightRaw(page.editingSlot) : 0

        Connections {
            target: ProductsModel
            function onDataChanged() {
                if (page.editingSlot < 1) return
                editDialog.liveRaw    = ProductsModel.lastRaw(page.editingSlot)
                editDialog.emptyShelf = ProductsModel.emptyShelfRaw(page.editingSlot)
                editDialog.unitWeight = ProductsModel.unitWeightRaw(page.editingSlot)
            }
        }

        Column {
            anchors.fill: parent
            spacing: 14

            // ── Pick-from-catalog shortcut ───────────────────────────────
            // This is the new "configure-once, reuse-everywhere" entry
            // point. Tap → catalog opens → pick → fields below auto-fill,
            // including the per-item weight if the catalog entry has one.
            Rectangle {
                width: parent.width; height: 80; radius: 16
                color: "#0891B2"
                TapHandler { onTapped: catalogPicker.open() }
                Row {
                    anchors.centerIn: parent
                    spacing: 12
                    Text { text: "📚"; font.pixelSize: 30
                           anchors.verticalCenter: parent.verticalCenter }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0
                        Text { text: qsTr("Pick from catalog")
                               color: "#FFFFFF"
                               font.pixelSize: 20; font.weight: Font.ExtraBold }
                        Text { text: qsTr("Reuse a product you've already configured")
                               color: "#A5F3FC"
                               font.pixelSize: 13 }
                    }
                }
            }

            Text { text: qsTr("Name"); font.pixelSize: 16; color: "#5A6B52" }
            TextField {
                id: editName
                width: parent.width
                placeholderText: qsTr("e.g. Cola 330ml")
                font.pixelSize: 22
            }

            Text { text: qsTr("Price (points)"); font.pixelSize: 16; color: "#5A6B52" }
            TextField {
                id: editPrice
                width: parent.width
                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhFormattedNumbersOnly
                validator: IntValidator { bottom: 0; top: 99999 }
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }

            Text { text: qsTr("Image"); font.pixelSize: 16; color: "#5A6B52" }
            Row {
                spacing: 10
                width: parent.width
                Rectangle {
                    width: 100; height: 100; radius: 10
                    color: "#E8EEDB"
                    border.width: 1; border.color: "#D8E0CF"
                    Image { anchors.fill: parent; anchors.margins: 6
                            source: editImage.text
                            fillMode: Image.PreserveAspectFit
                            visible: editImage.text.length > 0 }
                    Text { anchors.centerIn: parent; text: "📷"
                           font.pixelSize: 44
                           visible: editImage.text.length === 0 }
                }
                Button {
                    text: qsTr("Choose image…")
                    height: 50
                    onClicked: imagePicker.open()
                }
                TextField { id: editImage; visible: false }
            }

            CheckBox {
                id: editActive
                text: qsTr("Active (visible to customers)")
                font.pixelSize: 18
            }

            // ── Calibration block ─────────────────────────────────────
            // IMPORTANT UX note: this block is ONE-TIME per product. Normal
            // restocks need no buttons here — open the door, add items, close
            // the door, the auto-scanner handles the count. We only set
            // these two numbers when (a) configuring a slot for the first
            // time or (b) swapping to a different product that has a
            // different per-item weight.
            //
            // When the slot is already calibrated we collapse to a small
            // status row + an "Update calibration" disclosure that reveals
            // the full controls. Avoids making restock look like work.
            Rectangle {
                width: parent.width
                height: showCalControls ? 420 : 110
                Behavior on height { NumberAnimation { duration: 150 } }
                radius: 16
                color: "#FAFBF6"
                border.width: 2; border.color: "#D8E0CF"

                // Disclosure state: open automatically for uncalibrated slots,
                // collapsed by default once everything's set.
                property bool showCalControls: editDialog.unitWeight <= 0

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    // ── Status row (always visible) ──────────────────────
                    Row {
                        width: parent.width
                        spacing: 14

                        Rectangle {
                            width: 60; height: 60; radius: 30
                            color: editDialog.unitWeight > 0 ? "#16A34A" : "#DC2626"
                            anchors.verticalCenter: parent.verticalCenter
                            Text { anchors.centerIn: parent
                                   text: editDialog.unitWeight > 0 ? "⚖" : "!"
                                   color: "#FFFFFF"
                                   font.pixelSize: 30; font.weight: Font.Black }
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 60 - 200 - 28
                            spacing: 2
                            Text { text: editDialog.unitWeight > 0
                                         ? qsTr("Scale is calibrated for this product")
                                         : qsTr("Scale not calibrated yet")
                                   color: "#1F2A1B"
                                   font.pixelSize: 18; font.weight: Font.ExtraBold }
                            Text {
                                text: editDialog.unitWeight > 0
                                      ? qsTr("Auto-counts on every restock. Live reading: %1 raw  →  %2 items")
                                          .arg(editDialog.liveRaw).arg(
                                              Math.max(0, Math.round(
                                                  (editDialog.liveRaw - editDialog.emptyShelf)
                                                  / editDialog.unitWeight)))
                                      : qsTr("Run this once. After that, just open the door, add items, close it.")
                                color: "#5A6B52"
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }
                        Rectangle {
                            width: 200; height: 56; radius: 28
                            anchors.verticalCenter: parent.verticalCenter
                            color: parent.parent.parent.showCalControls ? "#9CA3AF" : "#0891B2"
                            TapHandler {
                                onTapped: parent.parent.parent.parent.showCalControls =
                                              !parent.parent.parent.parent.showCalControls
                            }
                            Text { anchors.centerIn: parent
                                   text: parent.parent.parent.parent.showCalControls
                                         ? qsTr("Hide") : qsTr("Update calibration")
                                   color: "#FFFFFF"
                                   font.pixelSize: 15; font.weight: Font.ExtraBold }
                        }
                    }

                    // ── Detail panel (only shown when expanded) ──────────
                    Rectangle {
                        visible: parent.parent.showCalControls
                        width: parent.width; height: 1; color: "#E5E7EB"
                    }

                    Column {
                        visible: parent.parent.showCalControls
                        width: parent.width
                        spacing: 12

                        // Big info banner — only shown when calibrating
                        Rectangle {
                            width: parent.width; height: 56; radius: 12
                            color: "#FEF3C7"
                            border.width: 1; border.color: "#FBBF24"
                            Text {
                                anchors.centerIn: parent
                                anchors.margins: 12
                                width: parent.width - 24
                                text: qsTr("Only do this when setting up a new product. Restocks are automatic.")
                                color: "#92400E"
                                font.pixelSize: 13; font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                        }

                        // Live readings strip
                        Row {
                            spacing: 24
                            width: parent.width
                            Column {
                                spacing: 2
                                Text { text: qsTr("Live"); color: "#5A6B52"; font.pixelSize: 11 }
                                Text { text: editDialog.liveRaw
                                       color: "#1F2A1B"
                                       font.pixelSize: 20; font.weight: Font.Black }
                            }
                            Column {
                                spacing: 2
                                Text { text: qsTr("Empty tare"); color: "#5A6B52"; font.pixelSize: 11 }
                                Text { text: editDialog.emptyShelf
                                       color: "#1F2A1B"
                                       font.pixelSize: 20; font.weight: Font.Black }
                            }
                            Column {
                                spacing: 2
                                Text { text: qsTr("Per item"); color: "#5A6B52"; font.pixelSize: 11 }
                                Text { text: editDialog.unitWeight
                                       color: "#1F2A1B"
                                       font.pixelSize: 20; font.weight: Font.Black }
                            }
                        }

                        // Action 1: tare empty shelf
                        Row {
                            spacing: 14
                            width: parent.width
                            Column {
                                width: parent.width - 220
                                spacing: 2
                                Text { text: qsTr("A.  Empty the shelf, then tap →")
                                       color: "#1F2A1B"
                                       font.pixelSize: 15; font.weight: Font.Bold }
                                Text { text: qsTr("Captures the zero reading for this slot.")
                                       color: "#5A6B52"
                                       font.pixelSize: 11
                                       wrapMode: Text.WordWrap; width: parent.width }
                            }
                            Rectangle {
                                width: 200; height: 50; radius: 25
                                color: "#0891B2"
                                TapHandler {
                                    onTapped: ProductsModel.calibrateEmptyShelf(
                                                  page.editingSlot, editDialog.liveRaw)
                                }
                                Text { anchors.centerIn: parent; text: qsTr("Tare = 0")
                                       color: "#FFFFFF"
                                       font.pixelSize: 17; font.weight: Font.ExtraBold }
                            }
                        }

                        // Action 2: per-item weight
                        Row {
                            spacing: 14
                            width: parent.width
                            Column {
                                width: parent.width - 220 - 110 - 28
                                spacing: 2
                                Text { text: qsTr("B.  Place N items, type N, tap Set →")
                                       color: "#1F2A1B"
                                       font.pixelSize: 15; font.weight: Font.Bold }
                                Text { text: qsTr("Tells the system how heavy one item is.")
                                       color: "#5A6B52"
                                       font.pixelSize: 11
                                       wrapMode: Text.WordWrap; width: parent.width }
                            }
                            TextField {
                                id: knownCountField
                                width: 110; height: 50
                                placeholderText: "N"
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 1; top: 9999 }
                                font.pixelSize: 20
                                horizontalAlignment: TextInput.AlignHCenter
                            }
                            Rectangle {
                                width: 200; height: 50; radius: 25
                                color: knownCountField.acceptableInput ? "#16A34A" : "#9CA3AF"
                                TapHandler {
                                    enabled: knownCountField.acceptableInput
                                    onTapped: {
                                        const n = parseInt(knownCountField.text || "0")
                                        if (n > 0) {
                                            ProductsModel.calibrateUnitWeight(
                                                page.editingSlot,
                                                editDialog.liveRaw, n)
                                            knownCountField.text = ""
                                        }
                                    }
                                }
                                Text { anchors.centerIn: parent; text: qsTr("Set per-item")
                                       color: "#FFFFFF"
                                       font.pixelSize: 17; font.weight: Font.ExtraBold }
                            }
                        }
                    }
                }
            }
        }

        onAccepted: {
            ProductsModel.setProduct(
                page.editingSlot,
                editName.text,
                parseInt(editPrice.text || "0"),
                editImage.text,
                editActive.checked
            )
        }
    }

    // ══════════════════════════ IMAGE PICKER ══════════════════════════
    Dialog {
        id: imagePicker
        title: qsTr("Choose product image")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: "#B0000000" }
        // Sized for 1080-wide kiosk with margins.
        width: 920
        height: 1000
        standardButtons: Dialog.Close

        Component.onCompleted: ProductImages.refresh()

        Column {
            anchors.fill: parent
            spacing: 12

            Text { text: qsTr("Tap an image to use it")
                   color: "#5A6B52"; font.pixelSize: 14 }

            // Add URL row
            Row {
                width: parent.width
                spacing: 8
                TextField {
                    id: urlField
                    width: parent.width - 230
                    placeholderText: qsTr("Paste image URL (https://...png)")
                    font.pixelSize: 14
                }
                TextField { id: urlName; width: 110
                            placeholderText: qsTr("Name"); font.pixelSize: 14 }
                Button {
                    text: qsTr("+ Add URL")
                    enabled: ProductImages.isValidImageUrl(urlField.text)
                    onClicked: {
                        ProductImages.addUrl(urlField.text, urlName.text)
                        urlField.clear(); urlName.clear()
                    }
                }
            }

            Rectangle {
                width: parent.width; height: 36; radius: 8
                color: "#FFF7D6"
                border.width: 1; border.color: "#F59E0B"
                Text {
                    anchors.centerIn: parent
                    text: qsTr("📱 Don't see it? Upload from the ReWinGo mobile app")
                    font.pixelSize: 13; color: "#92400E"
                }
            }

            GridView {
                id: picker
                width: parent.width
                height: parent.height - 140
                cellWidth: 150
                cellHeight: 170
                clip: true
                model: ProductImages.images

                delegate: Rectangle {
                    width: picker.cellWidth - 10
                    height: picker.cellHeight - 10
                    radius: 14
                    color: editImage.text === modelData.path ? "#0891B2" : "#FFFFFF"
                    border.width: 2
                    border.color: editImage.text === modelData.path
                                  ? "#0891B2" : "#D8E0CF"

                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        Image {
                            width: 110; height: 110
                            anchors.horizontalCenter: parent.horizontalCenter
                            source: modelData.path
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }
                        Text {
                            text: modelData.name
                            font.pixelSize: 13
                            color: editImage.text === modelData.path ? "#FFFFFF" : "#1F2A1B"
                            anchors.horizontalCenter: parent.horizontalCenter
                            elide: Text.ElideRight
                            width: 130
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    TapHandler {
                        onTapped: {
                            editImage.text = modelData.path
                            imagePicker.close()
                        }
                    }
                }
            }
        }
    }

    // ══════════════════════════ CATALOG PICKER ══════════════════════════
    // Opened from inside editDialog. On pick:
    //   - copies catalog name/image/price into the slot's edit fields
    //   - assigns the slot → catalog link in SQLite
    //   - seeds the per-item calibration from typicalWeightG using a
    //     global RAW_PER_GRAM constant. Admin can still re-calibrate
    //     precisely later via the existing "Tare + Set per-item" controls.
    CatalogPickerDialog {
        id: catalogPicker
        onPicked: (id, name, imagePath, price, weightG) => {
            editName.text  = name
            editPrice.text = price
            editImage.text = imagePath
            editActive.checked = true

            // Link the slot to the catalog row.
            ProductsModel; // touch
            CatalogModel.assignToSlot(page.editingSlot, id)

            // Seed unit_weight_raw from typical grams. Hardcoded
            // RAW_PER_GRAM = 22 matches the constant in products_model.cpp
            // — keep these two in sync if you re-tune the cell scale.
            if (weightG > 0) {
                const seedRaw = weightG * 22
                // Only seed if the slot isn't already calibrated, so we
                // don't clobber a hand-tuned value from a previous setup.
                if (ProductsModel.unitWeightRaw(page.editingSlot) <= 0) {
                    ProductsModel.calibrateUnitWeight(
                        page.editingSlot,
                        // Synthetic "current_raw" = tare + N items × raw/g.
                        // We assume 1 item is on the shelf for the seed —
                        // admin will refine with the Tare button next.
                        ProductsModel.emptyShelfRaw(page.editingSlot) + seedRaw,
                        1)
                }
            }
        }
    }
}
