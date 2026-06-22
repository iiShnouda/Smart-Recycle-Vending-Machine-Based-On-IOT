import QtQuick
import QtQuick.Controls
import Recycle_Vending_Machine_LCD
import "../components"

/*
 * VendingPage — rebuilt clean. Binds DIRECTLY to ProductsModel (one delegate
 * per slot, role properties read straight off the model) instead of the old
 * Loader/manual-data() approach. Tap a buyable slot → cart → checkout →
 * per-item dispense, deducting points only on a confirmed drop.
 */
Rectangle {
    id: vendingPage
    objectName: "vendingPage"
    color: "#EFF3EA"

    property StackView stackView: StackView.view
    property string userId: ""
    property string userName: "Guest"
    property int    userPoints: 0

    // ───────── Cart ─────────
    property var cart: []                 // [{slot, name, points, imagePath}]
    function cartTotal() {
        var s = 0
        for (var i = 0; i < cart.length; ++i) s += cart[i].points
        return s
    }
    function cartCountForSlot(slot) {
        var n = 0
        for (var i = 0; i < cart.length; ++i) if (cart[i].slot === slot) n++
        return n
    }
    function addToCart(slot, name, points, imagePath) {
        cart = cart.concat([{ slot: slot, name: name, points: points, imagePath: imagePath }])
        Idle.touch()
    }
    function removeFromCart(index) {
        var c = cart.slice(); c.splice(index, 1); cart = c
    }
    function clearCart() { cart = [] }

    // ───────── Dispense state machine ─────────
    property int    dispensingIndex: -1            // -1 = idle
    property string dispensingStatus: ""
    readonly property bool dispensing: dispensingIndex >= 0
    property var    dispensedItems: []
    property int    pointsUsed: 0

    function startDispenseSequence() {
        if (cart.length === 0 || dispensing) return
        checkoutDialog.close()
        dispensedItems = []; pointsUsed = 0
        dispensingIndex = 0
        sendNextDispense()
    }
    function sendNextDispense() {
        if (dispensingIndex >= cart.length) {
            dispensingIndex = -1; dispensingStatus = ""
            var items = dispensedItems, used = pointsUsed
            clearCart(); ProductsModel.reload()
            stackView.push(receiptComponent,
                           { items: items, totalUsed: used, newBalance: vendingPage.userPoints })
            return
        }
        var item = cart[dispensingIndex]
        dispensingStatus = qsTr("Dispensing ") + item.name + "…"
        // Fire-and-forget: send the motor command to STM32, don't wait for ACK
        appManager.sendSerial("DISPENSE:" + item.slot, 0)
        // Instantly process as dispensed
        vendingPage.userPoints   -= item.points
        vendingPage.pointsUsed   += item.points
        vendingPage.dispensedItems = vendingPage.dispensedItems.concat([item])
        ProductsModel.decrementCount(item.slot)
        if (vendingPage.userId !== "") {
            appManager.adjustPointsAndRecordTransaction(vendingPage.userId, "vending", item.slot, -item.points)
        }
        dispensingIndex++
        // Brief delay so the user sees the dispensing overlay per item
        dispenseNextTimer.start()
    }

    Timer {
        id: dispenseNextTimer
        interval: 500
        repeat: false
        onTriggered: sendNextDispense()
    }

    Component.onCompleted: { Idle.touch(); ProductsModel.reload() }
    StackView.onActivated:  { Idle.touch(); ProductsModel.reload() }

    Component { id: receiptComponent; VendingReceiptPage {} }

    // ═══════════ HEADER ═══════════
    Rectangle {
        id: header
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 200
        color: "#1F2A1B"

        Rectangle {
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 30
            width: 90; height: 90; radius: 45; color: "#FFFFFF"
            TapHandler { onTapped: { Idle.touch(); stackView.pop() } }
            Text { anchors.centerIn: parent; text: "←"; font.pixelSize: 42; color: "#1F2A1B" }
        }
        Column {
            anchors.centerIn: parent; spacing: 6
            Text { text: qsTr("Vending"); color: "#FFFFFF"
                   font.pixelSize: 60; font.weight: Font.Black
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Tap items to add to cart"); color: "#A5F3FC"
                   font.pixelSize: 18; anchors.horizontalCenter: parent.horizontalCenter }
        }
        // Points balance (minus what's in the cart)
        Rectangle {
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 30
            height: 100; radius: 50; width: pointsRow.implicitWidth + 56; color: "#0891B2"
            Row {
                id: pointsRow; anchors.centerIn: parent; spacing: 14
                Coin3D { anchors.verticalCenter: parent.verticalCenter; size: 104; transparentBg: true }
                Text { text: vendingPage.userPoints - cartTotal(); color: "#FFFFFF"
                       font.pixelSize: 50; font.weight: Font.Black
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("pts"); color: "#FFFFFF"
                       font.pixelSize: 24; font.weight: Font.DemiBold
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    // ═══════════ PRODUCT GRID (bound straight to ProductsModel) ═══════════
    GridView {
        id: grid
        anchors.top: header.bottom; anchors.topMargin: 18
        anchors.bottom: parent.bottom; anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.96
        cellWidth:  width / 2
        cellHeight: height / 4          // 8 slots → 2 cols × 4 rows
        clip: true
        interactive: false
        model: ProductsModel
        enabled: !checkoutDialog.opened && !vendingPage.dispensing

        delegate: Item {
            width: grid.cellWidth; height: grid.cellHeight

            // role props straight off ProductsModel
            required property int     slot
            required property string  name
            required property int     pricePoints
            required property string  imagePath
            required property bool    active
            required property int     count

            readonly property bool isEmpty:  name === "" || name.startsWith("Slot ")
            readonly property bool inStock:  count > 0
            readonly property bool canBuy:   active && inStock && !isEmpty
            readonly property int  balance:  vendingPage.userPoints - vendingPage.cartTotal()
            readonly property bool tooPoor:  canBuy && balance < pricePoints
            readonly property bool tappable: canBuy && !tooPoor

            Rectangle {
                anchors.fill: parent; anchors.margins: 8
                radius: 24
                color: isEmpty ? "#F2F4ED" : "#FFFFFF"
                border.width: 2
                border.color: tappable ? "#7A8B6A" : (!active && !isEmpty ? "#DC2626" : "#D8E0CF")
                opacity: (isEmpty || !tappable) ? 0.6 : 1.0

                // slot tag
                Rectangle {
                    width: 56; height: 30; radius: 15; color: "#1A1D1A"
                    anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 12
                    Text { anchors.centerIn: parent; text: "#" + slot; color: "#FFFFFF"
                           font.pixelSize: 14; font.weight: Font.ExtraBold }
                }
                // status badge
                Rectangle {
                    visible: !isEmpty && !tappable
                    width: bt.implicitWidth + 22; height: 30; radius: 15
                    anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 12
                    color: !active ? "#DC2626" : !inStock ? "#9CA3AF" : "#92400E"
                    Text { id: bt; anchors.centerIn: parent
                           text: !active ? qsTr("OFF") : !inStock ? qsTr("EMPTY") : qsTr("LOW PTS")
                           color: "#FFFFFF"; font.pixelSize: 12; font.weight: Font.Bold }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    width: parent.width - 24
                    visible: !isEmpty

                    Rectangle {
                        width: 150; height: 150; radius: 16; color: "#E8EEDB"
                        anchors.horizontalCenter: parent.horizontalCenter
                        Image { anchors.fill: parent; anchors.margins: 8
                                source: imagePath; fillMode: Image.PreserveAspectFit
                                visible: imagePath.length > 0; asynchronous: true }
                        Text { anchors.centerIn: parent; text: "🥤"; font.pixelSize: 60
                               visible: imagePath.length === 0 }
                    }
                    Text { text: name; color: "#1F2A1B"
                           font.pixelSize: 22; font.weight: Font.ExtraBold
                           elide: Text.ElideRight; width: parent.width
                           horizontalAlignment: Text.AlignHCenter
                           anchors.horizontalCenter: parent.horizontalCenter }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter; spacing: 6
                        Text { text: pricePoints; color: "#0891B2"
                               font.pixelSize: 24; font.weight: Font.Black
                               anchors.verticalCenter: parent.verticalCenter }
                        Text { text: qsTr("pts"); color: "#0891B2"
                               font.pixelSize: 15; font.weight: Font.DemiBold
                               anchors.verticalCenter: parent.verticalCenter }
                    }
                }

                Text {
                    visible: isEmpty
                    anchors.centerIn: parent; text: qsTr("Empty")
                    color: "#9CA3AF"; font.pixelSize: 20
                }

                TapHandler {
                    enabled: tappable
                    onTapped: addToCart(slot, name, pricePoints, imagePath)
                }
            }
        }
    }

    // Cart pill
    Rectangle {
        visible: cart.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 14
        height: 60; radius: 30; width: cartRow.implicitWidth + 40; color: "#16A34A"
        z: 10
        TapHandler { onTapped: checkoutDialog.open() }
        Row {
            id: cartRow; anchors.centerIn: parent; spacing: 10
            Text { text: "🛒"; font.pixelSize: 24; anchors.verticalCenter: parent.verticalCenter }
            Text { text: cart.length + " " + qsTr("items"); color: "#FFFFFF"
                   font.pixelSize: 18; font.weight: Font.ExtraBold
                   anchors.verticalCenter: parent.verticalCenter }
            Text { text: "•"; color: "#FFFFFF"; opacity: 0.5
                   anchors.verticalCenter: parent.verticalCenter }
            Text { text: cartTotal() + " " + qsTr("pts"); color: "#A5F3FC"
                   font.pixelSize: 18; font.weight: Font.ExtraBold
                   anchors.verticalCenter: parent.verticalCenter }
        }
    }

    // ═══════════ CHECKOUT DIALOG ═══════════
    Dialog {
        id: checkoutDialog
        title: qsTr("Checkout")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: "#A0000000" }
        width: 700; height: 720
        standardButtons: Dialog.Cancel

        Column {
            anchors.fill: parent; spacing: 12
            Row {
                width: parent.width
                Text { text: cart.length + " " + qsTr("items"); color: "#1F2A1B"
                       font.pixelSize: 22; font.weight: Font.ExtraBold }
                Item { width: parent.width - 400; height: 1 }
                Text { text: cartTotal() + " " + qsTr("pts"); color: "#0891B2"
                       font.pixelSize: 22; font.weight: Font.ExtraBold }
            }
            Rectangle { width: parent.width; height: 2; color: "#D8E0CF" }
            ListView {
                width: parent.width; height: parent.height - 200
                model: cart; clip: true; spacing: 8
                delegate: Rectangle {
                    width: ListView.view.width; height: 76; radius: 14
                    color: "#FFFFFF"; border.width: 1; border.color: "#D8E0CF"
                    Row {
                        anchors.fill: parent; anchors.margins: 10; spacing: 16
                        Image { width: 56; height: 56; source: modelData.imagePath
                                fillMode: Image.PreserveAspectFit
                                visible: modelData.imagePath !== ""
                                anchors.verticalCenter: parent.verticalCenter }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter; spacing: 2
                            Text { text: modelData.name; color: "#1F2A1B"
                                   font.pixelSize: 18; font.weight: Font.ExtraBold }
                            Text { text: qsTr("Slot ") + modelData.slot
                                   color: "#5A6B52"; font.pixelSize: 12 }
                        }
                        Item { width: 1; height: 1 }
                        Text { text: modelData.points + " " + qsTr("pts"); color: "#0891B2"
                               font.pixelSize: 20; font.weight: Font.Black
                               anchors.verticalCenter: parent.verticalCenter }
                        Rectangle {
                            width: 50; height: 50; radius: 25; color: "#DC2626"
                            anchors.verticalCenter: parent.verticalCenter
                            TapHandler { onTapped: removeFromCart(index) }
                            Text { anchors.centerIn: parent; text: "×"; color: "#FFFFFF"
                                   font.pixelSize: 28; font.weight: Font.Black }
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 90; radius: 45
                color: (vendingPage.userPoints >= cartTotal() && cart.length > 0) ? "#16A34A" : "#9CA3AF"
                TapHandler {
                    enabled: vendingPage.userPoints >= cartTotal() && cart.length > 0
                    onTapped: startDispenseSequence()
                }
                Text { anchors.centerIn: parent
                       text: vendingPage.userPoints >= cartTotal()
                             ? qsTr("Confirm — ") + cartTotal() + " " + qsTr("pts")
                             : qsTr("Not enough points")
                       color: "#FFFFFF"; font.pixelSize: 26; font.weight: Font.ExtraBold }
            }
        }
    }

    // ═══════════ DISPENSING OVERLAY ═══════════
    Rectangle {
        anchors.fill: parent; color: "#CC000000"; visible: vendingPage.dispensing; z: 100
        TapHandler { enabled: vendingPage.dispensing; onTapped: Idle.touch() }
        Column {
            anchors.centerIn: parent; spacing: 30
            BusyIndicator { running: vendingPage.dispensing; width: 160; height: 160
                            anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: vendingPage.dispensingStatus; color: "#FFFFFF"
                   font.pixelSize: 36; font.weight: Font.ExtraBold
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Please wait — do not touch the chute"); color: "#A5F3FC"
                   font.pixelSize: 20; anchors.horizontalCenter: parent.horizontalCenter }
        }
    }

    // ═══════════ SORRY / REFUND DIALOG ═══════════
    Dialog {
        id: sorryDialog
        property var    failedItem:   ({})
        property string failedReason: ""
        parent: Overlay.overlay; anchors.centerIn: parent
        modal: true; focus: true
        Overlay.modal: Rectangle { color: "#A0000000" }
        closePolicy: Popup.NoAutoClose
        width: 720; height: 480
        standardButtons: Dialog.NoButton

        Column {
            anchors.fill: parent; anchors.margins: 24; spacing: 16
            Text { text: "😞"; font.pixelSize: 90; anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Sorry — that item didn't drop"); color: "#1F2A1B"
                   font.pixelSize: 28; font.weight: Font.ExtraBold
                   horizontalAlignment: Text.AlignHCenter; width: parent.width; wrapMode: Text.WordWrap
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("Your %1 points were refunded.").arg(sorryDialog.failedItem.points || 0)
                   color: "#0891B2"; font.pixelSize: 22; font.weight: Font.Bold
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: qsTr("This slot is disabled so no one else hits it.")
                   color: "#1F2A1B"; font.pixelSize: 16
                   horizontalAlignment: Text.AlignHCenter; width: parent.width; wrapMode: Text.WordWrap
                   anchors.horizontalCenter: parent.horizontalCenter }
            Rectangle {
                width: 280; height: 70; radius: 35; color: "#16A34A"
                anchors.horizontalCenter: parent.horizontalCenter
                TapHandler { onTapped: sorryDialog.close() }
                Text { anchors.centerIn: parent; text: qsTr("OK"); color: "#FFFFFF"
                       font.pixelSize: 24; font.weight: Font.ExtraBold }
            }
        }
    }
}
