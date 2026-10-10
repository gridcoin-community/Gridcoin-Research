/*
    The mixed-build banner: shown while the GUI and the Gridcoin daemon were
    built from different commits (Shell.ipcGitCommitMismatch), until the user
    dismisses it.
*/
import QtQuick
import MMPTheme 1.0
import Gridcoin.Qml

Rectangle {
    id: banner
    objectName: "buildBanner"

    // Set by the dismiss button.
    property bool dismissed: false

    visible: Shell.ipcGitCommitMismatch && !dismissed
    // A hidden banner takes no space above the views.
    height: visible ? implicitHeight : 0
    implicitHeight: Math.max(bannerText.implicitHeight, dismissButton.implicitHeight) + 10
    color: MMPTheme.cCarminePink

    Text {
        id: bannerText
        objectName: "buildBannerText"
        textFormat: Text.PlainText
        text: qsTranslate("BitcoinGUI", "The GUI (%1) and the Gridcoin daemon (%2) were built from different commits. Mixed builds can behave unexpectedly.").arg(Shell.ipcGuiVersion).arg(Shell.ipcNodeVersion)
        wrapMode: Text.WordWrap
        color: MMPTheme.cWhite
        anchors {
            left: parent.left
            right: dismissButton.left
            verticalCenter: parent.verticalCenter
            leftMargin: 10
            rightMargin: 10
        }
    }

    Button {
        id: dismissButton
        objectName: "buildBannerDismiss"
        text: qsTranslate("BitcoinGUI", "Dismiss")
        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
            rightMargin: 10
        }
        onClicked: banner.dismissed = true
    }
}
