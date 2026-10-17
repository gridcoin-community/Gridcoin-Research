/*
    The primary window of the wallet containg the various views and a menu to select from them
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MMPTheme 1.0
ApplicationWindow {
    id: window
    height: minimumHeight
    minimumHeight: 640
    minimumWidth: 720
    visible: true
    title: qsTr("Gridcoin Wallet")
    opacity: 1

    Behavior on opacity {
        NumberAnimation { duration: 1000 }
    }

    UnlockDialog {
        id: unlockDialog
    }

    Connections {
        target: _walletModel
        function onRequireUnlock() {
            console.debug("MainWindow Require unlock")
            unlockDialog.open()
        }
    }

    TabMenu{
        id: menu
        width: 90
        onMenuButtonClicked: (identification) => loader.source=identification
        anchors{
            top: parent.top
            bottom: statusBar.top
            left: parent.left
        }
    }

    Loader {
        id: loader
        source: "OverviewView.qml"
        anchors {
            top: parent.top
            bottom: statusBar.top
            right: parent.right
            left: menu.right
        }
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
