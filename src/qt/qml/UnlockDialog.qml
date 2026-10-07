import QtQuick
import QtQuick.Controls
import MMPTheme 1.0

Dialog {
    id: unlockDialog
    title: qsTr("Unlock Wallet")
    modal: true
    width: 300
    height: 140
    anchors.centerIn: parent
    
    onOpened: {
        passwordField.forceActiveFocus()
    }
    onAccepted: {
       _walletModel.unlockWallet(passwordField.text)
    }
    onRejected: {
        _walletModel.cancelUnlock()
    }
    onClosed: {
        passwordField.text = ""
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
        text: unlockDialog.title
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
            top: unlockDialog.header.bottom
            bottom: unlockDialog.footer.top
            left: parent.left
            right: parent.right
            leftMargin: 1
            rightMargin: 1
        }
        Text {
            id: passphraseLabel
            text: qsTr("Enter passphrase")
            color: MMPTheme.textColor
            anchors {
                left: parent.left
                leftMargin: 10
                verticalCenter: parent.verticalCenter
            }
        }

        TextField {
            id: passwordField
            echoMode: TextInput.Password
            Keys.onReturnPressed: unlockDialog.accept()
            anchors {
                left: passphraseLabel.right
                leftMargin: 20
                verticalCenter: passphraseLabel.verticalCenter
                right: parent.right
                rightMargin: 20
            }
        }
    }

    footer: DialogButtonBox {
        onAccepted: unlockDialog.accept()
        onRejected: unlockDialog.reject()
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