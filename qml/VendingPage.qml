import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../components"

/*
 * VendingPage — all 8 slots, 4 per page, swipeable with page dots.
 *
 * Layout: 2×2 grid per page → 2 pages total. SwipeView at the centre,
 * PageIndicator at the bottom. No scrolling — everything fits on one
 * "page" the user can swipe between.
 */
Rectangle {
    id: vendingPage
    objectName: "vendingPage"
    color: "#EFF3EA"

    property StackView stackView: StackView.view
    property string userName: "Guest"
    property int    userPoints: 0

    // Cart
    property var cart: []
    function cartTotal() {
        let sum = 0
        for (const item of cart) sum += item.price
        return sum
    }
    function addToCart(slot, name, price, imagePath) {
        const updated = cart.slice()
        updated.push({ slot, name, price, imagePath })
        cart = updated
        Idle.touch()
    }
    function removeFromCart(index) {
        const updated = cart.slice()
        updated.splice(index, 1)
        cart = updated
    }
    function clearCart() { cart = [] }

    // ───────── Dispense state ─────────
    // We dispense items one at a time, deducting points only on success.
    // A fault disables the product and shows a sorry dialog; the cart
    // continues with the remaining items.
    property int    dispensingIndex: -1            // -1 = idle, else cart[idx]
    property string dispensingStatus: ""
    readonly property bool dispensing: dispensingIndex >= 0
    property var    dispensedItems: []             // what actually dropped
    property int    pointsUsed: 0

    function startDispenseSequence() {
        if (cart.length === 0 || dispensing) return
        checkoutDialog.close()
        dispensedItems = []
        pointsUsed = 0
        dispensingIndex = 0
        sendNextDispense()
    }

    function sendNextDispense() {
        if (dispensingIndex >= cart.length) {
            // All items processed — show the receipt, then reset.
            dispensingIndex = -1
            dispensingStatus = ""
            clearCart()
            ProductsModel.reload()   // refresh in case any were disabled
            stackView.push(receiptComponent, {
                items: vendingPage.dispensedItems,
                totalUsed: vendingPage.pointsUsed,
                newBalance: vendingPage.userPoints
            })
            return
        }
        const item = cart[dispensingIndex]
        dispensingStatus = qsTr("Dispensing ") + item.name + "…"
        // 10 s timeout: 1 rev ≈ 2 s + weighing settle (~0.4 s) + slack.
        appManager.sendSerial("DISPENSE:" + item.slot, 10000)
    }

    Connections {
        target: appManager
        function onSerialCommandSucceeded(cmd, reply) {
            if (!vendingPage.dispensing) return
            if (!cmd.startsWith("DISPENSE:")) return
            const item = cart[dispensingIndex]
            // Real refund happens on the Pi (audit + DB) — for the UI we
            // only need to mirror the deduction here.
            vendingPage.userPoints -= item.price
            vendingPage.pointsUsed += item.price
            vendingPage.dispensedItems = vendingPage.dispensedItems.concat([item])
            dispensingIndex++
            sendNextDispense()
        }
        function onSerialCommandFailed(cmd, reason) {
            if (!vendingPage.dispensing) return
            if (!cmd.startsWith("DISPENSE:")) return
            const item = cart[dispensingIndex]
            // No deduction → effectively refunded. Disable the slot so no
            // one buys a broken auger before the admin services it.
            ProductsModel.setActive(item.slot, false)
            sorryDialog.failedItem = item
            sorryDialog.failedReason = reason
            sorryDialog.open()
            dispensingIndex++
            sendNextDispense()
        }
    }

    Component.onCompleted: { Idle.touch(); ProductsModel.reload() }
    // Re-pull on every entry so changes made on the admin Products page (stock,
    // price, name, image, active) are reflected the moment the customer opens
    // — not only on first creation.
    StackView.onActivated: { Idle.touch(); ProductsModel.reload() }

    Component { id: receiptComponent; VendingReceiptPage {} }

    // ═══════════ DARK HEADER (matches admin pages) ═══════════
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 200
        color: "#1F2A1B"

        // Back button
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 30
            width: 90; height: 90; radius: 45
            color: "#FFFFFF"
            TapHandler {
                // Back to MainPage, NOT all the way to the StartPage.
                onTapped: { Idle.touch(); stackView.pop() }
            }
            Text { anchors.centerIn: parent; text: "←"
                   font.pixelSize: 42; color: "#1F2A1B" }
        }

        // Title in the middle
        Column {
            anchors.centerIn: parent
            spacing: 6
            Text { text: qsTr("Vending")
                   color: "#FFFFFF"
                   font.pixelSize: 60; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Tap items to add to cart")
                   color: "#A5F3FC"
                   font.pixelSize: 18
                   anchors.horizontalCenter: parent.horizontalCenter }
        }

        // Points pill — bigger, vertically centered, on the right.
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 30
            height: 100; radius: 50
            width: pointsRow.implicitWidth + 56
            color: "#0891B2"
            Row {
                id: pointsRow
                anchors.centerIn: parent
                spacing: 14
                Coin3D {
                    anchors.verticalCenter: parent.verticalCenter
                    size: 104
                    transparentBg: true
                }
                Text { text: vendingPage.userPoints - cartTotal()
                       color: "#FFFFFF"
                       font.pixelSize: 50; font.weight: Font.Black
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("pts")
                       color: "#FFFFFF"
                       font.pixelSize: 24; font.weight: Font.DemiBold
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // Cart pill — bottom-centre of the banner.
        Rectangle {
            visible: cart.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            height: 60; radius: 30
            width: cartRow.implicitWidth + 40
            color: "#16A34A"
            TapHandler { onTapped: checkoutDialog.open() }
            Row {
                id: cartRow
                anchors.centerIn: parent
                spacing: 10
                Text { text: "🛒"; font.pixelSize: 24
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: cart.length + " " + qsTr("items")
                       color: "#FFFFFF"
                       font.pixelSize: 18; font.weight: Font.ExtraBold
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: "•"
                       color: "#FFFFFF"; opacity: 0.5
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: cartTotal() + " " + qsTr("pts")
                       color: "#A5F3FC"
                       font.pixelSize: 18; font.weight: Font.ExtraBold
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    // ═══════════ SINGLE GRID — all 8 slots on one page ═══════════
    Grid {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: 20
        anchors.bottom: pageIndicator.top
        anchors.bottomMargin: 14
        anchors.horizontalCenter: parent.horizontalCenter
        width:  parent.width  * 0.96
        columns: 2
        rows: 4
        spacing: 14
        enabled: !checkoutDialog.opened

        Repeater {
            model: 8
            delegate: Item {
                width:  (grid.width  - grid.spacing) / 2
                height: (grid.height - grid.spacing * 3) / 4

                Loader {
                    anchors.fill: parent
                    sourceComponent: slotCardComponent
                    // Push the row index INTO the loaded card. A `property int
                    // slotIdx: index` here is shadowed by the card's own
                    // slotIdx, so every card stayed 0 → all showed "Slot 1".
                    onLoaded: item.slotIdx = index
                }
            }
        }
    }

    // ═══════════ SLOT CARD COMPONENT ═══════════
    Component {
        id: slotCardComponent
        Item {
            id: cell
            // slotIdx provided by the Loader as a property
            property int slotIdx: 0

            // Pull row data from the model
            readonly property var row: ProductsModel.rowCount() > slotIdx
                                       ? { name: "Slot " + (slotIdx + 1) }
                                       : ({})
            // Better: use a Binding to fetch every field individually
            property string pname: ""
            property int    pprice: 0
            property string pimage: ""
            property bool   pactive: false
            property int    pcount: 0
            property bool   pcalibrated: false

            Component.onCompleted: refresh()
            // The Loader assigns slotIdx just after onCompleted, so re-read the
            // model whenever it lands on its real value (0..7).
            onSlotIdxChanged: refresh()
            function refresh() {
                // QAbstractListModel exposes data via index()
                const idx = ProductsModel.index(slotIdx, 0)
                if (!idx.valid) return
                pname       = ProductsModel.data(idx, Qt.UserRole + 2)  // RoleName
                pprice      = ProductsModel.data(idx, Qt.UserRole + 3)
                pimage      = ProductsModel.data(idx, Qt.UserRole + 4)
                pactive     = ProductsModel.data(idx, Qt.UserRole + 5)
                pcount      = ProductsModel.data(idx, Qt.UserRole + 6)
                pcalibrated = ProductsModel.data(idx, Qt.UserRole + 11) // RoleCalibrated
            }
            Connections {
                target: ProductsModel
                function onDataChanged() { refresh() }
                function onModelReset()  { refresh() }
            }

            // Stock is set by the admin's In-stock toggle (count > 0). New
            // products default to out of stock until the admin marks them in.
            readonly property bool inStock:  pcount > 0
            readonly property bool isActive: pactive === true || pactive === 1
            readonly property bool canBuy:   isActive && inStock
            readonly property bool tooPoor:  canBuy && (vendingPage.userPoints - vendingPage.cartTotal()) < pprice
            readonly property bool tappable: canBuy && !tooPoor

            Rectangle {
                anchors.fill: parent
                radius: 28
                color: "#FFFFFF"
                border.width: 3
                border.color: cell.tappable   ? "#7A8B6A"
                            : !cell.isActive  ? "#DC2626"
                                              : "#D8E0CF"
                opacity: cell.tappable ? 1.0 : 0.55

                Rectangle {
                    width: 64; height: 34; radius: 17
                    color: "#1A1D1A"
                    anchors.top: parent.top; anchors.left: parent.left
                    anchors.margins: 14
                    Text { anchors.centerIn: parent; text: "#" + (cell.slotIdx + 1)
                           color: "#FFFFFF"
                           font.pixelSize: 16; font.weight: Font.ExtraBold }
                }

                Rectangle {
                    visible: !cell.tappable
                    width: badgeText.implicitWidth + 26; height: 34; radius: 17
                    color: !cell.isActive ? "#DC2626"
                         : !cell.inStock  ? "#9CA3AF"
                                          : "#92400E"
                    anchors.top: parent.top; anchors.right: parent.right
                    anchors.margins: 14
                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: !cell.isActive ? qsTr("OFF")
                            : !cell.inStock  ? qsTr("EMPTY")
                                             : qsTr("LOW PTS")
                        color: "#FFFFFF"
                        font.pixelSize: 13; font.weight: Font.Bold
                    }
                }

                Rectangle {
                    id: img
                    anchors.top: parent.top; anchors.topMargin: 70
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 170; height: 170; radius: 20
                    color: "#E8EEDB"
                    Image { anchors.fill: parent; anchors.margins: 8
                            source: cell.pimage
                            fillMode: Image.PreserveAspectFit
                            visible: cell.pimage.length > 0 }
                    Text { anchors.centerIn: parent; text: "Empty"
                           color: "#9CA3AF"; font.pixelSize: 18
                           visible: cell.pimage.length === 0 }
                }

                Column {
                    anchors.top: img.bottom; anchors.topMargin: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 30
                    spacing: 4
                    Text { text: cell.pname
                           color: "#1F2A1B"
                           font.pixelSize: 22; font.weight: Font.ExtraBold
                           elide: Text.ElideRight
                           anchors.horizontalCenter: parent.horizontalCenter
                           horizontalAlignment: Text.AlignHCenter
                           width: parent.width }
                    Text { text: cell.pprice + " " + qsTr("pts")
                           color: "#0891B2"
                           font.pixelSize: 22; font.weight: Font.Black
                           anchors.horizontalCenter: parent.horizontalCenter }
                }

                TapHandler {
                    enabled: cell.tappable
                    onTapped: addToCart(cell.slotIdx + 1, cell.pname,
                                        cell.pprice, cell.pimage)
                }
            }
        }
    }

    // ═══════════ PAGE INDICATOR (decorative — single page) ═══════════
    PageIndicator {
        id: pageIndicator
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60          // clears the 14-pt copyright at parent bottom
        anchors.horizontalCenter: parent.horizontalCenter
        count: 1
        currentIndex: 0
        delegate: Rectangle {
            implicitWidth: 18; implicitHeight: 18; radius: 9
            color: "#0891B2"
        }
    }

    // ═══════════ CHECKOUT DIALOG ═══════════
    Dialog {
        id: checkoutDialog
        title: qsTr("Checkout")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: "#A0000000" }
        width: 700
        height: 720
        standardButtons: Dialog.Cancel

        Column {
            anchors.fill: parent
            spacing: 12

            Row {
                width: parent.width
                Text { text: cart.length + " " + qsTr("items")
                       color: "#1F2A1B"; font.pixelSize: 22; font.weight: Font.ExtraBold }
                Item { width: parent.width - 400; height: 1 }
                Text { text: cartTotal() + " " + qsTr("pts")
                       color: "#0891B2"; font.pixelSize: 22; font.weight: Font.ExtraBold }
            }
            Rectangle { width: parent.width; height: 2; color: "#D8E0CF" }

            ListView {
                width: parent.width
                height: parent.height - 200
                model: cart
                clip: true
                spacing: 8
                delegate: Rectangle {
                    width: parent.width
                    height: 76
                    radius: 14
                    color: "#FFFFFF"
                    border.width: 1; border.color: "#D8E0CF"
                    Row {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 16
                        Image { width: 56; height: 56
                                source: modelData.imagePath
                                fillMode: Image.PreserveAspectFit
                                visible: modelData.imagePath !== ""
                                anchors.verticalCenter: parent.verticalCenter }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: modelData.name
                                   color: "#1F2A1B"; font.pixelSize: 18; font.weight: Font.ExtraBold }
                            Text { text: qsTr("Slot ") + modelData.slot
                                   color: "#5A6B52"; font.pixelSize: 12 }
                        }
                        Item { width: 1; height: 1 }
                        Text { text: modelData.price + " " + qsTr("pts")
                               color: "#0891B2"; font.pixelSize: 20; font.weight: Font.Black
                               anchors.verticalCenter: parent.verticalCenter }
                        Rectangle {
                            width: 50; height: 50; radius: 25; color: "#DC2626"
                            anchors.verticalCenter: parent.verticalCenter
                            TapHandler { onTapped: removeFromCart(index) }
                            Text { anchors.centerIn: parent; text: "×"
                                   color: "#FFFFFF"; font.pixelSize: 28; font.weight: Font.Black }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width; height: 90; radius: 45
                color: vendingPage.userPoints >= cartTotal() && cart.length > 0
                       ? "#16A34A" : "#9CA3AF"
                enabled: vendingPage.userPoints >= cartTotal() && cart.length > 0
                TapHandler {
                    enabled: parent.enabled
                    // Hands off to the per-item state machine. We no longer
                    // pre-deduct cartTotal — points come off only as each
                    // DISPENSE replies "Done". Faults disable that slot and
                    // skip the deduction (= effective refund).
                    onTapped: startDispenseSequence()
                }
                Text {
                    anchors.centerIn: parent
                    text: vendingPage.userPoints >= cartTotal()
                          ? qsTr("Confirm — ") + cartTotal() + " " + qsTr("pts")
                          : qsTr("Not enough points")
                    color: "#FFFFFF"
                    font.pixelSize: 26; font.weight: Font.ExtraBold
                }
            }
        }
    }

    // ═══════════ DISPENSING OVERLAY ═══════════
    // Blocks input + shows progress while items are dropping. Auto-closes
    // when the sequence finishes (dispensingIndex resets to -1).
    Rectangle {
        anchors.fill: parent
        color: "#CC000000"
        visible: vendingPage.dispensing
        z: 100
        // Eat touches so the user can't double-tap.
        TapHandler { enabled: vendingPage.dispensing; onTapped: Idle.touch() }

        Column {
            anchors.centerIn: parent
            spacing: 30

            BusyIndicator {
                running: vendingPage.dispensing
                width: 160; height: 160
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: vendingPage.dispensingStatus
                color: "#FFFFFF"
                font.pixelSize: 36; font.weight: Font.ExtraBold
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("Please wait — do not touch the chute")
                color: "#A5F3FC"
                font.pixelSize: 20
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ═══════════ SORRY / REFUND DIALOG ═══════════
    Dialog {
        id: sorryDialog
        property var    failedItem:   ({})
        property string failedReason: ""

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        Overlay.modal: Rectangle { color: "#A0000000" }
        closePolicy: Popup.NoAutoClose
        width: 720; height: 540
        standardButtons: Dialog.NoButton

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 18

            Text {
                text: "😞"
                font.pixelSize: 96
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("Sorry — that item didn't drop")
                color: "#1F2A1B"
                font.pixelSize: 28; font.weight: Font.ExtraBold
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Text {
                text: (sorryDialog.failedItem.name || qsTr("Item")) + " — " +
                      qsTr("Slot ") + (sorryDialog.failedItem.slot || "?")
                color: "#5A6B52"
                font.pixelSize: 20
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("Your %1 points have been refunded.")
                          .arg(sorryDialog.failedItem.price || 0)
                color: "#0891B2"
                font.pixelSize: 22; font.weight: Font.Bold
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("We've disabled this slot so no one else hits the same issue. " +
                           "An admin will service it.")
                color: "#1F2A1B"
                font.pixelSize: 16
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
                wrapMode: Text.WordWrap
            }
            // Tiny fault-reason line — useful while bringing the hardware up,
            // can be hidden later by setting visible: false.
            Text {
                text: qsTr("Reason: ") + sorryDialog.failedReason
                color: "#9CA3AF"
                font.pixelSize: 12
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Rectangle {
                width: 280; height: 70; radius: 35
                color: "#16A34A"
                anchors.horizontalCenter: parent.horizontalCenter
                TapHandler { onTapped: sorryDialog.close() }
                Text {
                    anchors.centerIn: parent
                    text: qsTr("OK")
                    color: "#FFFFFF"
                    font.pixelSize: 24; font.weight: Font.ExtraBold
                }
            }
        }
    }
}
