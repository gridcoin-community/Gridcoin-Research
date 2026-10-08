/*
A root qml item that allows for the managment of windows, menu bar/tray icon managment
Acts as the parent item for windows. This is to prevent unwanted garbage collection
*/
import QtQuick
import QtQuick.Window
import QtQml
import Qt.labs.platform
import Gridcoin.Qml
Item {
    id: root

    property MainWindow mainWindow: null
    property AboutWindow aboutWindow: null
    property var splashWindow: null

    // The splash may already be active when the root loads: the front end
    // creates it before it loads the root.
    Component.onCompleted: {
        if (Splash.active && !Splash.finished) showSplashScreen()
    }

    Connections {
        target: Splash
        function onActiveChanged() {
            if (Splash.active && !Splash.finished) showSplashScreen()
        }
    }
    Connections {
        target: Shell
        function onShowMainRequested(minimized) {
            // A quit was requested first: the process is about to exit.
            if (Shell.quitRequested) return
            if (minimized) {
                showMainWindowMinimized()
            } else {
                showMainWindow()
            }
        }
        function onHideMainRequested() {
            if (mainWindow) mainWindow.hide()
        }
        function onHideToTrayRequested() {
            if (mainWindow) mainWindow.hide()
        }
    }

    SystemTrayIcon {
        id: trayIcon
        objectName: "trayIcon"
        visible: true
        icon.source: "qrc:/icons/logos/ic_logo_app_gradient_white.svg"
        onActivated: (reason) => {
            if (reason === SystemTrayIcon.Trigger && Shell.shown && !Shell.quitRequested) showMainWindow()
        }
        menu: Menu {
            MenuItem {
                id: trayQuit
                text: qsTr("Quit")
                // As in the Widgets front end, the tray offers no action until
                // the main window has been shown.
                enabled: Shell.shown
                onTriggered: Qt.quit()
            }
        }
    }
    MenuBar {
        Menu {
            title: qsTr("File")
            MenuItem { text: qsTr("Backup Wallet/Config")}
            MenuItem {
                text: qsTr("Export")
                enabled: false
            }
            MenuItem { text: qsTr("Snapshot Download")}
            MenuItem {text: qsTr("Export")}
            MenuItem {
                id: aboutMenuItem
                role: MenuItem.AboutRole
                text: qsTr("About")
                enabled: Shell.shown
                onTriggered: showAboutWindow()
            }
        }
        Menu {
            id: editMenu
            title: qsTr("&Edit")
        }
        Menu {
            id: viewMenu
            title: qsTr("&View")
        }
        Menu {
            id: helpMenu
            title: qsTr("&Help")
        }

    }

    function showSplashScreen() {
        if (splashWindow) return
        var component = Qt.createComponent("SplashScreen.qml")
        splashWindow = component.createObject(root)
        splashWindow.show()
    }

    // The main window's state is changed only through the QWindow methods:
    // Qt 6.7 and later defer a QML property write that shows or minimizes a
    // window whose parent is this Item, which is in no window itself.
    function showMainWindow() {
        createMainWindow()
        // setVisible(true) keeps a maximized window maximized; show() would
        // restore it to its normal size.
        if (mainWindow.visibility === Window.Minimized) {
            mainWindow.showNormal()
        } else {
            mainWindow.setVisible(true)
        }
        mainWindow.raise()
        mainWindow.requestActivate()
    }

    function showMainWindowMinimized() {
        createMainWindow()
        mainWindow.showMinimized()
    }

    function createMainWindow() {
        if (!mainWindow) {
            var component = Qt.createComponent("MainWindow.qml")
            mainWindow = component.createObject(root, { objectName: "mainWindow" })
        }
    }

    function showOnboardingWindow() {
        var component = Qt.createComponent("OnboardingWindow.qml")
        var windowObj = component.createObject(root)
        windowObj.onboardingFinished.connect(showMainWindow)
        windowObj.show()
    }

    function showAboutWindow() {
        if (!aboutWindow) {
            var component = Qt.createComponent("AboutWindow.qml")
            aboutWindow = component.createObject(root)
            aboutWindow.onClosing.connect(function(){
                aboutWindow = null
            })
        }
        aboutWindow.show()
        aboutWindow.raise()
        aboutWindow.requestActivate()
    }
}
