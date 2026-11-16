/*
    A view allowing for the sending of funds
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0
import Gridcoin.Qml

Rectangle {
    id: main
    color: MMPTheme.backgroundColor

    // Commits every row's field texts, then asks to send: a field edited
    // without Return or a focus change has not reached the model yet.
    function sendAll() {
        for (var i = 0; i < outputList.count; ++i) {
            var row = outputList.itemAtIndex(i)
            if (row) row.commitFields()
        }
        SendCoins.send()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: header
            color: MMPTheme.headerColor
            Layout.fillWidth: true
            Layout.preferredHeight: 70

            Rectangle {
                id: bottomBorder
                height: 1
                color: MMPTheme.themeSelect("transparent", MMPTheme.cBlack)
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 20
                spacing: 10

                Text {
                    id: titleText
                    text: qsTr("Send Funds")
                    font.weight: Font.DemiBold
                    font.pixelSize: 22
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                }

                Item {
                    Layout.fillWidth: true
                }

                Column {
                    id: balColumn
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

                    Text {
                        id: balanceValue
                        objectName: "sendBalance"
                        text: WalletSummary.balanceText
                        color: MMPTheme.highlightColor
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        id: balanceLabel
                        text: qsTr("Available")
                        color: MMPTheme.textColor
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 10
                        font.weight: Font.Light
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }

        ScrollView {
            clip: true
            contentHeight: outputColumn.implicitHeight
            contentWidth: availableWidth
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 10
            Layout.bottomMargin: 10

            Column {
                id: outputColumn
                spacing: 10
                width: parent.width - 20
                x: 10

                ListView {
                    id: outputList
                    objectName: "recipientList"
                    spacing: 10
                    interactive: false
                    model: SendCoins.recipients
                    height: contentHeight
                    width: parent.width

                    delegate: Rectangle {
                        id: delegateRect
                        color: MMPTheme.bodyColor
                        width: ListView.view.width
                        implicitHeight: delegateLayout.implicitHeight
                        radius: 4

                        // Writes this row's field texts to the model.
                        function commitFields() {
                            SendCoins.updateRecipient(index, {
                                "recipient": recipientField.text,
                                "label": labelTextField.text,
                                "amount": transactionAmountEdit.text,
                                "message": messageTextField.text
                            })
                        }

                        ColumnLayout {
                            id: delegateLayout
                            anchors.fill: parent
                            spacing: 0

                            GridLayout {
                                Layout.fillWidth: true
                                Layout.margins: 20
                                columns: 2
                                rowSpacing: 20
                                columnSpacing: 10

                                // Row 1
                                Text {
                                    id: recipientLabel
                                    text: qsTr("Recipient") + ":"
                                    color: MMPTheme.textColor
                                    font.pixelSize: 13
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: recipientField
                                        objectName: "recipientField"
                                        text: model.recipient
                                        color: model.addressValid ? MMPTheme.textColor : MMPTheme.cCarminePink
                                        placeholderText: "Gridcoin Address (eg. bc3NA8e8E3EoTL1qhRmeprbjWcmuoZ26A2)"
                                        onEditingFinished: SendCoins.updateRecipient(index, {"recipient": text})
                                        Layout.fillWidth: true
                                    }
                                    Button {
                                        id: dropdownButton
                                        icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_open_menu_light.svg","qrc:/icons/buttons/ic_btn_open_menu_dark.svg")
                                    }
                                    Button {
                                        id: clipboardButton
                                        objectName: "recipientPaste"
                                        icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_paste_light.svg","qrc:/icons/buttons/ic_btn_paste_dark.svg")
                                        // Replaces the recipient with the clipboard's text, as the
                                        // Widgets paste button does; the field follows the model.
                                        onClicked: SendCoins.updateRecipient(index, {"recipient": Clipboard.text()})
                                    }
                                }

                                // Row 2
                                // Only the first row's message is sent, and only while
                                // the network accepts message contracts.
                                Text {
                                    id: messageLabel
                                    visible: index === 0 && SendCoins.messageAllowed
                                    text: qsTr("Message") + ":"
                                    color: MMPTheme.textColor
                                    font.pixelSize: 13
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                RowLayout {
                                    visible: index === 0 && SendCoins.messageAllowed
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: messageTextField
                                        objectName: "messageField"
                                        text: model.message
                                        onEditingFinished: SendCoins.updateRecipient(index, {"message": text})
                                        Layout.fillWidth: true
                                    }
                                    Button {
                                        id: attachFileButton
                                        icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_attach_light.svg","qrc:/icons/buttons/ic_btn_attach_dark.svg")
                                    }
                                }

                                // Row 3
                                Text {
                                    id: labelLabel
                                    text: qsTr("Label") + ":"
                                    color: MMPTheme.textColor
                                    font.pixelSize: 13
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: labelTextField
                                        placeholderText: qsTr("Add a label to save it to Favourites")
                                        text: model.label
                                        onEditingFinished: SendCoins.updateRecipient(index, {"label": text})
                                        Layout.fillWidth: true
                                    }
                                    Item {
                                        Layout.preferredWidth: 15
                                    }
                                    Text {
                                        id: amountLabel
                                        text: qsTr("Amount") + ":"
                                        color: MMPTheme.textColor
                                        font.pixelSize: 13
                                        Layout.alignment: Qt.AlignVCenter
                                    }
                                    TextField {
                                        id: transactionAmountEdit
                                        objectName: "amountField"
                                        text: model.amountText
                                        color: model.amountValid ? MMPTheme.textColor : MMPTheme.cCarminePink
                                        Layout.preferredWidth: 160
                                        onEditingFinished: SendCoins.updateRecipient(index, {"amount": text})
                                    }
                                    Text {
                                        id: grcText
                                        text: SendCoins.unitName
                                        verticalAlignment: Text.AlignVCenter
                                        color: MMPTheme.textColor
                                        font.pixelSize: 13
                                        Layout.alignment: Qt.AlignVCenter
                                    }
                                    CheckBox {
                                        id: subtractFeeBox
                                        objectName: "subtractFeeBox"
                                        text: qsTranslate("SendCoinsEntry", "Subtract fee from amount")
                                        checked: model.subtractFee
                                        onCheckedChanged: {
                                            if (checked !== model.subtractFee) SendCoins.updateRecipient(index, {"subtractFee": checked})
                                        }
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 50

                                Rectangle {
                                    id: controlTopRect
                                    color: MMPTheme.ternaryBodyColor
                                    height: parent.height - controlBottomRect.radius
                                    anchors {
                                        top: parent.top
                                        right: parent.right
                                        left: parent.left
                                    }
                                }
                                Rectangle {
                                    id: controlBottomRect
                                    color: MMPTheme.ternaryBodyColor
                                    radius: delegateRect.radius
                                    height: 2 * radius
                                    anchors {
                                        verticalCenter: controlTopRect.bottom
                                        left: parent.left
                                        right: parent.right
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 20
                                    anchors.rightMargin: 20

                                    Button {
                                        id: removeButton
                                        icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_remove_light.svg","qrc:/icons/buttons/ic_btn_remove_dark.svg")
                                        text: qsTr("Remove")
                                        enabled: outputList.count > 1
                                        onPressed: SendCoins.removeRecipient(index)
                                    }
                                }
                            }
                        }
                    }
                }

                Button {
                    id: newOutputButton
                    icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_add_light.svg", "qrc:/icons/buttons/ic_btn_add_dark.svg")
                    text: qsTr("New Recipient")
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: SendCoins.addRecipient()
                }
            }
        }

        Rectangle {
            id: bottomControls
            color: MMPTheme.ternaryBodyColor
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.margins: 10
            radius: 4
            border.color: MMPTheme.borderColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 20

                Button {
                    id: removeAllButton
                    icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_remove_light.svg","qrc:/icons/buttons/ic_btn_remove_dark.svg")
                    text: qsTr("Remove All")
                    onClicked: SendCoins.clearRecipients()
                }
                Button {
                    id: advancedCoinControlButton
                    icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_sign_light.svg","qrc:/icons/buttons/ic_btn_sign_dark.svg")
                    text: qsTr("Advanced Coin Control")
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: sendButton
                    objectName: "sendButton"
                    icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_send_light.svg","qrc:/icons/buttons/ic_btn_send_dark.svg")
                    text: qsTr("Send")
                    enabled: !SendCoins.busy
                    onClicked: main.sendAll()
                }
            }
        }
    }

}
