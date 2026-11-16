/*
    A message from the core: its caption, its text and an OK button. One window
    shows one message; WindowManager.qml creates it and destroys it once the
    message is dismissed.
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MMPTheme 1.0

Window {
    id: coreMessageWindow
    objectName: "coreMessageWindow"

    property string caption
    property string text
    // Set when the window is created, before it is shown: Qt registers a modal
    // window only when it is shown.
    property bool modalMessage: false
    // Set on the first close and never cleared, so the window reports one
    // dismissal at most.
    property bool dismissedOnce: false

    // The message was dismissed: by the OK button, the window's close button or
    // a keyboard close alike, since each of them closes the window.
    signal dismissed()

    title: caption
    width: 420
    height: 180
    minimumWidth: 320
    minimumHeight: 140
    modality: modalMessage ? Qt.ApplicationModal : Qt.NonModal
    color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cSpaceBlack)

    onClosing: {
        if (!dismissedOnce) {
            dismissedOnce = true
            dismissed()
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: MMPTheme.headerColor
        radius: 4
        border {
            color: MMPTheme.borderColor
            width: 1
        }
    }

    Rectangle {
        id: header
        height: 40
        color: MMPTheme.ternaryBodyColor
        radius: 4
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: 1
        }
        Label {
            id: captionLabel
            text: coreMessageWindow.caption
            font.bold: true
            color: MMPTheme.textColor
            elide: Text.ElideRight
            anchors {
                left: parent.left
                right: parent.right
                leftMargin: 15
                rightMargin: 15
                verticalCenter: parent.verticalCenter
            }
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

    Text {
        id: messageText
        text: coreMessageWindow.text
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: MMPTheme.textColor
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: okButton.top
            margins: 15
        }
    }

    Button {
        id: okButton
        objectName: "coreMessageOk"
        text: qsTr("OK")
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 10
        }
        onClicked: coreMessageWindow.close()
    }
}
