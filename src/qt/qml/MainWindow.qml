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

    // The send, fee, result and out-of-sync prompts. They live here, not in a
    // view, so a prompt never closes with the view that asked for it.
    WalletPrompts {
        id: walletPrompts
        objectName: "walletPrompts"
    }

    // The view on show: the path the tab menu last switched to. The loader's
    // source holds a resolved URL, so it is not compared with a path.
    property string currentPath: "OverviewView.qml"

    // Shows the view at `path`. Send asks first while the wallet is out of
    // sync; No puts the tab menu back on the view on show.
    function navigate(path) {
        if (path === window.currentPath) return
        if (path === "SendView.qml" && !ClientStatus.inSync) {
            walletPrompts.askOutOfSync(function () { window.showView(path) },
                                       function () { menu.select(window.currentPath) })
            return
        }
        window.showView(path)
    }

    function showView(path) {
        window.currentPath = path
        loader.source = path
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
        onMenuButtonClicked: (path) => window.navigate(path)
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

    // An accepted URI has filled the send page: show the window and switch to
    // the page.
    Connections {
        target: SendCoins
        function onUriAccepted() {
            if (window.visibility === Window.Minimized) {
                window.showNormal()
            } else if (!window.visible) {
                window.show()
            }
            window.raise()
            window.requestActivate()
            menu.select("SendView.qml")
        }
    }

    // A URI dropped on the window takes the path a received URI takes.
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            for (var i = 0; i < drop.urls.length; ++i) {
                SendCoins.acceptUri(drop.urls[i].toString())
            }
        }
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
