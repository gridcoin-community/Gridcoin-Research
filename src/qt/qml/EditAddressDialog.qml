import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0

Dialog {
    id: editAddressDialog
    title: {
        if (dialogMode === "NewReceivingAddress") return qsTr("New receiving address")
        if (dialogMode === "NewSendingAddress") return qsTr("New sending address")
        if (dialogMode === "EditReceivingAddress") return qsTr("Edit receiving address")
        if (dialogMode === "EditSendingAddress") return qsTr("Edit sending address")
        if (dialogMode === "AddExistingReceivingAddress") return qsTr("Add existing receiving address")
        return qsTr("Edit Address")
    }
    modal: true
    width: 400
    height: 180
    anchors.centerIn: parent

    property string dialogMode: "NewReceivingAddress"
    property string address: ""
    property string label: ""
    property var unbookedAddresses: []

    onOpened: {
        labelField.text = label
        if (dialogMode === "AddExistingReceivingAddress") {
            unbookedAddresses = _walletModel.addressTableModel.unbookedReceiveAddresses()
            addressComboBox.model = unbookedAddresses
            if (unbookedAddresses.length > 0) {
                addressComboBox.currentIndex = 0
            } else {
                _nativeDialog.warning(qsTr("No unbooked addresses"))
                editAddressDialog.reject()
            }
        }
        addressField.text = address
        labelField.forceActiveFocus()
    }

    onAccepted: {
        label = labelField.text
        if (dialogMode === "AddExistingReceivingAddress") {
            address = addressComboBox.currentText
        } else {
            address = addressField.text
        }
    }

    onClosed: {
        labelField.text = ""
        addressField.text = ""
    }

    background: Rectangle {
        id: background
        anchors.fill: parent
        color: MMPTheme.headerColor
        radius: 4
        border {
            color: MMPTheme.borderColor
            width: 1
        }
    }

    header: Label {
        text: editAddressDialog.title
        font.bold: true
        padding: 15
        height: 40
        color: MMPTheme.textColor
        background: Rectangle {
            color: MMPTheme.ternaryBodyColor
            anchors.fill: parent
            radius: 4
            anchors.margins: 1
        }
        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 1
            color: MMPTheme.borderColor
        }
    }

    contentItem: Item {
        anchors.margins: 10
        anchors {
            top: editAddressDialog.header.bottom
            bottom: editAddressDialog.footer.top
            left: parent.left
            right: parent.right
            leftMargin: 1
            rightMargin: 1
        }

        GridLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            columns: 2
            rowSpacing: 15
            columnSpacing: 10

            Text {
                text: qsTr("Label")
                color: MMPTheme.textColor
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }

            TextField {
                id: labelField
                Layout.fillWidth: true
                color: MMPTheme.textColor
                onAccepted: {
                    if (dialogMode === "AddExistingReceivingAddress") {
                        editAddressDialog.accept()
                    } else {
                        addressField.enabled ? addressField.forceActiveFocus() : editAddressDialog.accept()
                    }
                }
            }

            Text {
                text: qsTr("Address")
                color: MMPTheme.textColor
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                visible: dialogMode !== "NewReceivingAddress"
            }

            TextField {
                id: addressField
                Layout.fillWidth: true
                color: MMPTheme.textColor
                onAccepted: editAddressDialog.accept()
                enabled: dialogMode !== "EditReceivingAddress"
                visible: dialogMode !== "AddExistingReceivingAddress" && dialogMode !== "NewReceivingAddress"
            }

            ComboBox {
                id: addressComboBox
                Layout.fillWidth: true
                visible: dialogMode === "AddExistingReceivingAddress"
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("OK")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        background: Rectangle {
            anchors.fill: parent
            color: MMPTheme.ternaryBodyColor
            radius: 4
            anchors.margins: 1
        }
    }
}
