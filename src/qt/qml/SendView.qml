/*
    A view allowing for the sending of funds
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0

Rectangle {
    id: main
    color: MMPTheme.backgroundColor

    Connections {
        target: _sendCoinsController
        function onCoinsSentOrFailed(message) {
            _nativeDialog.information(qsTr("Send Coins"), message)
        }
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
                        text: _walletModel.balance.toLocaleString(Qt.locale(), 'f', 2)
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
                    spacing: 10
                    interactive: false
                    model: _sendCoinsController.recipients
                    height: contentHeight
                    width: parent.width
                    
                    delegate: Rectangle {
                        id: delegateRect
                        color: MMPTheme.bodyColor
                        width: ListView.view.width
                        implicitHeight: delegateLayout.implicitHeight
                        radius: 4

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
                                        text: modelData.recipient
                                        placeholderText: "Gridcoin Address (eg. bc3NA8e8E3EoTL1qhRmeprbjWcmuoZ26A2)"
                                        onEditingFinished: _sendCoinsController.updateRecipient(index, {"recipient": text})
                                        Layout.fillWidth: true
                                    }
                                    Button {
                                        id: dropdownButton
                                        icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_open_menu_light.svg","qrc:/icons/buttons/ic_btn_open_menu_dark.svg")
                                    }
                                    Button {
                                        id: clipboardButton
                                        icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_paste_light.svg","qrc:/icons/buttons/ic_btn_paste_dark.svg")
                                    }
                                }

                                // Row 2
                                Text {
                                    id: messageLabel
                                    text: qsTr("Message") + ":"
                                    color: MMPTheme.textColor
                                    font.pixelSize: 13
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: messageTextField
                                        text: modelData.message
                                        onEditingFinished: _sendCoinsController.updateRecipient(index, {"message": text})
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
                                        text: modelData.label
                                        onEditingFinished: _sendCoinsController.updateRecipient(index, {"label": text})
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
                                        text: modelData.amount
                                        Layout.preferredWidth: 160
                                        onEditingFinished: _sendCoinsController.updateRecipient(index, {"amount": text})
                                        validator: DoubleValidator {
                                            bottom: 0
                                            top:  _walletModel.balance
                                            decimals: 8
                                            notation: DoubleValidator.StandardNotation
                                            locale: "en_US"
                                        }
                                    }
                                    Text {
                                        id: grcText
                                        text: qsTr("GRC")
                                        verticalAlignment: Text.AlignVCenter
                                        color: MMPTheme.textColor
                                        font.pixelSize: 13
                                        Layout.alignment: Qt.AlignVCenter
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
                                        onPressed: _sendCoinsController.removeRecipient(index)
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
                    onClicked: _sendCoinsController.addRecipient()
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
                    onClicked: _sendCoinsController.clearRecipients()
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
                    icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_send_light.svg","qrc:/icons/buttons/ic_btn_send_dark.svg")
                    text: qsTr("Send")
                    onClicked: {
                        if (_nativeDialog.question(qsTr("Are you sure?"), qsTr("Are you sure you want to send this transaction?"))) {
                            _sendCoinsController.sendCoins()
                        }
                    }
                }
            }
        }
    }

}
