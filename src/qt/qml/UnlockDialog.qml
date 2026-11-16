import QtQuick
import QtQuick.Controls
import MMPTheme 1.0
import Gridcoin.Qml

Dialog {
    id: unlockDialog
    objectName: "unlockDialog"
    title: qsTr("Unlock Wallet")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 440
    anchors.centerIn: parent

    // The wallet-operation request this dialog answers; 0 while none is open.
    property double requestId: 0
    // The request's mode, WalletOps.Unlock or WalletOps.Elevate.
    property int requestMode: 0
    // The request's text, and the failure text of the last answer.
    property string requestText: ""
    property string failureText: ""
    // An answer is running: OK waits for its result.
    property bool answering: false
    // The request was accepted or finished, so closing does not cancel it.
    property bool settled: false

    // Sends the passphrase and empties the field at once: no passphrase is
    // kept in QML.
    function submit() {
        if (unlockDialog.answering || unlockDialog.requestId === 0) return
        if (WalletOps.answerUnlock(unlockDialog.requestId, passwordField.text)) unlockDialog.answering = true
        passwordField.text = ""
    }

    Connections {
        target: WalletOps
        function onUnlockRequested(id, mode, title, text) {
            unlockDialog.requestId = id
            unlockDialog.requestMode = mode
            unlockDialog.title = title
            unlockDialog.requestText = text
            unlockDialog.failureText = ""
            unlockDialog.answering = false
            unlockDialog.settled = false
            unlockDialog.open()
        }
        function onUnlockFailed(id, reason, text) {
            if (id !== unlockDialog.requestId) return
            passwordField.text = ""
            unlockDialog.failureText = text
            unlockDialog.answering = false
        }
        function onUnlockAccepted(id) {
            if (id !== unlockDialog.requestId) return
            unlockDialog.settled = true
            unlockDialog.close()
        }
        function onFinished(id, result) {
            if (id !== unlockDialog.requestId) return
            unlockDialog.settled = true
            unlockDialog.close()
        }
    }

    onOpened: {
        passwordField.forceActiveFocus()
    }
    // Every close that does not settle the request cancels it: Cancel,
    // Escape and a closed window alike.
    onClosed: {
        var id = unlockDialog.requestId
        var cancel = !unlockDialog.settled && id !== 0
        passwordField.text = ""
        unlockDialog.requestId = 0
        unlockDialog.answering = false
        unlockDialog.settled = false
        if (cancel) WalletOps.cancelUnlock(id)
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

    contentItem: Column {
        spacing: 10
        padding: 10

        Text {
            id: requestLabel
            objectName: "unlockText"
            width: parent.width - 20
            text: unlockDialog.requestText
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: MMPTheme.textColor
        }

        Row {
            spacing: 20

            Text {
                id: passphraseLabel
                text: qsTr("Enter passphrase")
                color: MMPTheme.textColor
                anchors.verticalCenter: parent.verticalCenter
            }

            TextField {
                id: passwordField
                objectName: "unlockPassphrase"
                echoMode: TextInput.Password
                width: 240
                Keys.onReturnPressed: unlockDialog.submit()
                Keys.onEnterPressed: unlockDialog.submit()
            }
        }

        Text {
            id: failureLabel
            objectName: "unlockFailure"
            width: parent.width - 20
            visible: text !== ""
            text: unlockDialog.failureText
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: MMPTheme.cCarminePink
        }
    }

    // The buttons carry no button role, so the box neither accepts nor rejects
    // the dialog: OK keeps it open until the answer's result, and Cancel closes
    // it, which cancels.
    footer: DialogButtonBox {
        Button {
            objectName: "unlockOk"
            text: qsTr("OK")
            enabled: !unlockDialog.answering
            onClicked: unlockDialog.submit()
        }
        Button {
            text: qsTr("Cancel")
            onClicked: unlockDialog.close()
        }

        background: Rectangle {
            anchors.fill: parent
            color: MMPTheme.ternaryBodyColor
            radius: 4
            anchors.margins: 1
        }
    }
}
