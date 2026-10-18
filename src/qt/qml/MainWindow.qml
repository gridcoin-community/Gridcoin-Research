/*
    The primary window of the wallet containg the various views and a menu to select from them
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MMPTheme 1.0
import Gridcoin.Qml
ApplicationWindow {
    id: window
    height: minimumHeight
    minimumHeight: 640
    minimumWidth: 720
    visible: true
    title: qsTr("Gridcoin Wallet") + (ClientStatus.isTestNet ? " " + qsTranslate("BitcoinGUI", "[testnet]") : "")
    opacity: 1

    // Shell.requestClose() hides the window to the tray (and refuses the
    // close) or quits the application.
    onClosing: (close) => {
        close.accepted = Shell.requestClose()
    }

    Behavior on opacity {
        NumberAnimation { duration: 1000 }
    }

    UnlockDialog {
        id: unlockDialog
    }

    // Shell.ipcGitCommitMismatch is set before this window is created, so the
    // banner reads the property, not the signal.
    BuildMismatchBanner {
        id: buildBanner
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    TabMenu{
        id: menu
        objectName: "tabMenu"
        width: 90
        onMenuButtonClicked: (identification) => loader.source=identification
        anchors{
            top: buildBanner.bottom
            bottom: statusBar.top
            left: parent.left
        }
    }

    Loader {
        id: loader
        objectName: "viewLoader"
        source: "OverviewView.qml"
        anchors {
            top: buildBanner.bottom
            bottom: statusBar.top
            right: parent.right
            left: menu.right
        }
    }

    // A view asks for another view by its path; the tab menu switches to it.
    Connections {
        target: loader.item
        ignoreUnknownSignals: true
        function onNavigateRequested(path) { menu.select(path) }
    }

    Shortcut {
        sequence: "Ctrl+Shift+M"
        onActivated: WalletSummary.setMaskValues(!WalletSummary.maskValues)
    }
    StatusFooter {
        id: statusBar
        height: 24
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
    }
}
