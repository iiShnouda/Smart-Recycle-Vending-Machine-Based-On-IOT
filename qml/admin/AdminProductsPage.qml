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

        // Scan-a-product button (top-right, left of reload)
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 130
            height: 80; radius: 40
            width: scanRow.implicitWidth + 44
            color: scanTap.pressed ? "#0E7490" : "#0891B2"
            scale: scanTap.pressed ? 0.94 : 1.0
            Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }
            TapHandler { id: scanTap; onTapped: scanDialog.beginScan() }
            Row {
                id: scanRow
                anchors.centerIn: parent
                spacing: 8
                Text { text: "📷"; font.pixelSize: 30
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("Scan product"); color: "#FFFFFF"
                       font.pixelSize: 18; font.weight: Font.ExtraBold
                       anchors.verticalCenter: parent.verticalCenter }
            }
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
                                : count > 0  ? qsTr("IN STOCK")
                                             : qsTr("OUT")
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
                        // A fresh slot defaults to Active — you're adding a
                        // product because you want customers to see it.
                        editActive.checked = isEmpty ? true : isActive
                        editInStock.checked = count > 0          // default OUT of stock
                        editDialog.nameCandidates = []
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
        width: 880
        height: 860
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: "#B0000000" }
        standardButtons: Dialog.Save | Dialog.Cancel

        // Inline product search (Open Food Facts) as the admin types the name.
        property var  nameCandidates: []
        property bool nameSearching: false
        Timer {
            id: nameSearch
            interval: 700; repeat: false
            onTriggered: {
                if (editName.text.trim().length >= 3) {
                    editDialog.nameSearching = true
                    OffClient.search(editName.text)
                }
            }
        }
        Connections {
            target: OffClient
            function onResults(query, list) {
                editDialog.nameSearching = false
                editDialog.nameCandidates = list
            }
            function onSearchFailed(q, r) {
                editDialog.nameSearching = false
                editDialog.nameCandidates = []
            }
        }

        Column {
            anchors.fill: parent
            spacing: 14

            Text { text: qsTr("Name  (type to search products)")
                   font.pixelSize: 16; color: "#5A6B52" }
            Row {
                width: parent.width
                spacing: 10
                TextField {
                    id: editName
                    width: parent.width - 170
                    placeholderText: qsTr("e.g. Coca Cola 330ml")
                    font.pixelSize: 22
                    onTextChanged: nameSearch.restart()
                }
                // Explicit lookup — fetches images + info from Open Food Facts.
                Rectangle {
                    width: 160; height: 56
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 12
                    color: editName.text.trim().length > 0 ? "#0891B2" : "#9CA3AF"
                    TapHandler {
                        enabled: editName.text.trim().length > 0
                        onTapped: { editDialog.nameSearching = true; OffClient.search(editName.text) }
                    }
                    Row {
                        anchors.centerIn: parent; spacing: 8
                        BusyIndicator { running: editDialog.nameSearching
                                        visible: editDialog.nameSearching
                                        width: 22; height: 22
                                        anchors.verticalCenter: parent.verticalCenter }
                        Text { anchors.verticalCenter: parent.verticalCenter
                               text: editDialog.nameSearching ? qsTr("Looking…") : qsTr("🔍 Look up")
                               color: "#FFFFFF"; font.pixelSize: 17; font.weight: Font.ExtraBold }
                    }
                }
            }

            // Inline suggestions with images — tap one to fill name + image.
            Rectangle {
                visible: editDialog.nameCandidates.length > 0
                width: parent.width; height: 150; radius: 12
                color: "#FAFBF6"; border.width: 1; border.color: "#D8E0CF"
                ListView {
                    anchors.fill: parent; anchors.margins: 8
                    orientation: ListView.Horizontal; spacing: 8; clip: true
                    model: editDialog.nameCandidates
                    delegate: Rectangle {
                        width: 120; height: 130; radius: 10; color: "#FFFFFF"
                        border.width: 1; border.color: "#D8E0CF"
                        Column {
                            anchors.fill: parent; anchors.margins: 6; spacing: 4
                            Image { width: parent.width; height: 78
                                    source: modelData.imageUrl
                                    fillMode: Image.PreserveAspectFit
                                    visible: modelData.imageUrl }
                            Text { text: modelData.name; width: parent.width
                                   color: "#1F2A1B"; font.pixelSize: 10
                                   elide: Text.ElideRight; maximumLineCount: 2
                                   wrapMode: Text.WordWrap }
                        }
                        TapHandler {
                            onTapped: {
                                editName.text  = modelData.name
                                editImage.text = modelData.imageUrl
                                editDialog.nameCandidates = []
                            }
                        }
                    }
                }
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

            CheckBox {
                id: editInStock
                text: qsTr("In stock (available to buy)")
                font.pixelSize: 18
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
            ProductsModel.setInStock(page.editingSlot, editInStock.checked)
            ProductsModel.reload()
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

    // ══════════════════════ CALIBRATION PROMPT ══════════════════════
    // Shown right after configuring a product whose slot isn't calibrated.
    // Calibration is the ONLY thing that makes counting automatic — so we
    // nudge the admin to do the one-time scale setup instead of asking them
    // to type stock numbers. Skipping is fine: the slot is assumed in-stock
    // until the scale is calibrated.
    Dialog {
        id: calibratePrompt
        property int    slotNo: -1
        property string prodName: ""

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        Overlay.modal: Rectangle { color: "#B0000000" }
        width: 720; height: 460
        standardButtons: Dialog.NoButton

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 18

            Text { text: "⚖"; font.pixelSize: 76
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text {
                text: qsTr("Set up automatic stock counting?")
                color: "#1F2A1B"; font.pixelSize: 28; font.weight: Font.ExtraBold
                horizontalAlignment: Text.AlignHCenter; width: parent.width
                wrapMode: Text.WordWrap
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("Calibrate the shelf scale for “%1” once, and the machine auto-counts every restock — no manual numbers. Until then this slot is shown in stock.")
                          .arg(calibratePrompt.prodName)
                color: "#5A6B52"; font.pixelSize: 17
                horizontalAlignment: Text.AlignHCenter; width: parent.width
                wrapMode: Text.WordWrap
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Row {
                spacing: 16
                anchors.horizontalCenter: parent.horizontalCenter
                Rectangle {
                    width: 300; height: 76; radius: 38; color: "#16A34A"
                    TapHandler {
                        onTapped: {
                            calibratePrompt.close()
                            // Re-open the slot editor; its calibration block
                            // auto-expands because the slot isn't calibrated.
                            page.editingSlot = calibratePrompt.slotNo
                            editDialog.open()
                        }
                    }
                    Text { anchors.centerIn: parent; text: qsTr("Calibrate now")
                           color: "#FFFFFF"; font.pixelSize: 22; font.weight: Font.ExtraBold }
                }
                Rectangle {
                    width: 260; height: 76; radius: 38
                    color: "transparent"; border.width: 2; border.color: "#9CA3AF"
                    TapHandler { onTapped: calibratePrompt.close() }
                    Text { anchors.centerIn: parent; text: qsTr("Skip for now")
                           color: "#5A6B52"; font.pixelSize: 20; font.weight: Font.Bold }
                }
            }
        }
    }

    // ══════════════════════ SCAN A PRODUCT ══════════════════════
    // Show the camera a product's barcode → Open Food Facts lookup → this
    // popup shows the name + image, suggests an empty slot, and lets the
    // admin set the points, then adds it. No URLs, no typing the name.
    Dialog {
        id: scanDialog
        property string phase: "idle"   // scanning | looking | result | none | error
        property string rName:    ""
        property string rImage:   ""
        property int    rWeight:  0
        property string rBarcode: ""
        property bool   found:    false
        property string errMsg:   ""

        function beginScan() {
            phase = "scanning"
            rName = ""; rImage = ""; rWeight = 0; rBarcode = ""; found = false; errMsg = ""
            scanName.text = ""; scanPoints.text = ""
            open()
            BarcodeScanner.scan()
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true; focus: true
        closePolicy: Popup.NoAutoClose
        Overlay.modal: Rectangle { color: "#B0000000" }
        width: 820; height: 980
        standardButtons: Dialog.NoButton

        Connections {
            target: BarcodeScanner
            function onScanned(code) {
                scanDialog.rBarcode = code
                scanDialog.phase = "looking"
                OffClient.lookupBarcode(code)
            }
            function onNothingFound() { if (scanDialog.visible) scanDialog.phase = "none" }
            function onFailed(msg) {
                if (scanDialog.visible) { scanDialog.phase = "error"; scanDialog.errMsg = msg }
            }
        }
        Connections {
            target: OffClient
            function onProductResolved(barcode, ok, product) {
                if (!scanDialog.visible) return
                scanDialog.found   = ok
                scanDialog.rName   = ok ? (product.name    || "") : ""
                scanDialog.rImage  = ok ? (product.imageUrl || "") : ""
                scanDialog.rWeight = ok ? (product.weightG  || 0)  : 0
                scanName.text = scanDialog.rName
                var s = ProductsModel.firstEmptySlot()
                scanSlot.value = s > 0 ? s : 1
                scanDialog.phase = "result"
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            // ── Busy phases ──
            Column {
                visible: scanDialog.phase === "scanning" || scanDialog.phase === "looking"
                width: parent.width; spacing: 20
                anchors.horizontalCenter: parent.horizontalCenter

                // Live camera so the admin can aim the barcode. The scanner
                // sidecar writes its frames to the same preview file the face
                // provider reads (they never run at once).
                Rectangle {
                    visible: scanDialog.phase === "scanning"
                    width: 640; height: 360; radius: 18
                    color: "#1A1D1A"; clip: true
                    anchors.horizontalCenter: parent.horizontalCenter
                    CameraPreview {
                        anchors.fill: parent
                        circular: false
                        cornerRadius: 18
                        fps: 12
                    }
                    // Aiming guide line down the middle.
                    Rectangle { anchors.centerIn: parent; width: parent.width * 0.8
                                height: 3; color: "#00E5FF"; opacity: 0.7 }
                }

                BusyIndicator { running: visible; width: 120; height: 120
                                visible: scanDialog.phase === "looking"
                                anchors.horizontalCenter: parent.horizontalCenter }
                Text {
                    text: scanDialog.phase === "scanning"
                          ? qsTr("Hold the product's barcode up to the camera…")
                          : qsTr("Looking it up…  (%1)").arg(scanDialog.rBarcode)
                    color: "#1F2A1B"; font.pixelSize: 24; font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter; width: parent.width
                    wrapMode: Text.WordWrap
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // ── No barcode / error ──
            Column {
                visible: scanDialog.phase === "none" || scanDialog.phase === "error"
                width: parent.width; spacing: 18
                Text { text: "🤔"; font.pixelSize: 80
                       anchors.horizontalCenter: parent.horizontalCenter }
                Text {
                    text: scanDialog.phase === "error"
                          ? qsTr("Scanner problem: %1").arg(scanDialog.errMsg)
                          : qsTr("Didn't catch a barcode. Try again, holding it steady and well-lit.")
                    color: "#1F2A1B"; font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter; width: parent.width
                    wrapMode: Text.WordWrap
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // ── Result ──
            Item {
                visible: scanDialog.phase === "result"
                width: parent.width; height: 720

                Column {
                    anchors.fill: parent
                    spacing: 14

                    Text {
                        text: scanDialog.found ? qsTr("Found it!")
                                               : qsTr("Not in the database — fill it in")
                        color: scanDialog.found ? "#16A34A" : "#92400E"
                        font.pixelSize: 26; font.weight: Font.ExtraBold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Row {
                        spacing: 16
                        width: parent.width
                        Rectangle {
                            width: 150; height: 150; radius: 14
                            color: "#E8EEDB"; border.width: 1; border.color: "#D8E0CF"
                            Image { anchors.fill: parent; anchors.margins: 8
                                    source: scanDialog.rImage
                                    fillMode: Image.PreserveAspectFit
                                    visible: scanDialog.rImage.length > 0 }
                            Text { anchors.centerIn: parent; text: "📷"; font.pixelSize: 48
                                   visible: scanDialog.rImage.length === 0 }
                        }
                        Column {
                            width: parent.width - 166
                            spacing: 8
                            Text { text: qsTr("Name"); color: "#5A6B52"; font.pixelSize: 14 }
                            TextField { id: scanName; width: parent.width
                                        placeholderText: qsTr("Product name"); font.pixelSize: 20 }
                            Text { text: qsTr("Barcode: %1").arg(scanDialog.rBarcode)
                                   color: "#9CA3AF"; font.pixelSize: 13 }
                            Text { visible: scanDialog.rWeight > 0
                                   text: qsTr("Weight: %1 g").arg(scanDialog.rWeight)
                                   color: "#9CA3AF"; font.pixelSize: 13 }
                        }
                    }

                    // Slot + points
                    Row {
                        spacing: 24; width: parent.width
                        Column {
                            spacing: 4
                            Text { text: qsTr("Put in slot"); color: "#5A6B52"; font.pixelSize: 14 }
                            SpinBox { id: scanSlot; from: 1; to: 8; value: 1; font.pixelSize: 22 }
                        }
                        Column {
                            spacing: 4; width: parent.width - 220
                            Text { text: qsTr("Price (points)"); color: "#5A6B52"; font.pixelSize: 14 }
                            TextField {
                                id: scanPoints; width: 200
                                placeholderText: "50"
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 0; top: 99999 }
                                font.pixelSize: 22
                            }
                        }
                    }
                    Text {
                        text: qsTr("Slot %1 is suggested (first empty). The image comes from the database; no calibration needed to start selling.")
                                  .arg(scanSlot.value)
                        color: "#5A6B52"; font.pixelSize: 13; wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }

            // ── Buttons ──
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16

                Rectangle {
                    visible: scanDialog.phase === "result"
                    width: 320; height: 76; radius: 38
                    color: scanName.text.trim().length > 0 ? "#16A34A" : "#9CA3AF"
                    TapHandler {
                        enabled: scanName.text.trim().length > 0
                        onTapped: {
                            ProductsModel.setProduct(
                                scanSlot.value, scanName.text.trim(),
                                parseInt(scanPoints.text || "0"),
                                scanDialog.rImage, true)
                            ProductsModel.reload()
                            scanDialog.close()
                        }
                    }
                    Text { anchors.centerIn: parent
                           text: qsTr("Add to slot %1").arg(scanSlot.value)
                           color: "#FFFFFF"; font.pixelSize: 20; font.weight: Font.ExtraBold }
                }
                Rectangle {
                    visible: scanDialog.phase === "none" || scanDialog.phase === "error"
                    width: 220; height: 76; radius: 38; color: "#0891B2"
                    TapHandler { onTapped: scanDialog.beginScan() }
                    Text { anchors.centerIn: parent; text: qsTr("Try again")
                           color: "#FFFFFF"; font.pixelSize: 20; font.weight: Font.ExtraBold }
                }
                Rectangle {
                    width: 200; height: 76; radius: 38
                    color: "transparent"; border.width: 2; border.color: "#9CA3AF"
                    TapHandler {
                        onTapped: { BarcodeScanner.cancel(); scanDialog.close() }
                    }
                    Text { anchors.centerIn: parent
                           text: scanDialog.phase === "result" ? qsTr("Cancel") : qsTr("Close")
                           color: "#5A6B52"; font.pixelSize: 20; font.weight: Font.Bold }
                }
            }
        }
    }
}
