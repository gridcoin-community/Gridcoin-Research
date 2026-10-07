/*
A root qml item that allows for the managment of windows, menu bar/tray icon managment
Acts as the parent item for windows. This is to prevent unwanted garbage collection
*/
import QtQuick
import QtQuick.Window
import QtQml
import Qt.labs.platform
Item {
    id: root

    property MainWindow mainWindow: null
    property AboutWindow aboutWindow: null

    Connections {
        target: _initModel
        function onShowSplashScreen() { showSplashScreen() }
        function onDoneLoading() {
            if (_initModel.startMinimized) {
                showMainWindowMinimized()
            } else {
                showMainWindow()
            }
        }
    }
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state == Qt.ApplicationActive && _initModel.initializationDone) {
                showMainWindow()
            }
        }
    }

    SystemTrayIcon {
        visible: true
        icon.source: "qrc:/icons/logos/ic_logo_app_gradient_white.svg"
        menu: Menu {
            MenuItem {
                text: qsTr("Quit")
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
                enabled: _initModel.initializationDone
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
        var component = Qt.createComponent("SplashScreen.qml")
        var windowObj = component.createObject(root)
        windowObj.show()
    }

    function showMainWindow() {
        createMainWindow()
        mainWindow.show()
        mainWindow.raise()
        mainWindow.requestActivate()
    }

    function showMainWindowMinimized() {
        createMainWindow()
        mainWindow.visibility = Window.Minimized
    }

    function createMainWindow() {
        if (!mainWindow) {
            var component = Qt.createComponent("MainWindow.qml")
            mainWindow = component.createObject(root)
            mainWindow.onClosing.connect(function() {
                mainWindow = null
            })
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
