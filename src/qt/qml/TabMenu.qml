/*
    Menu allowing for the selection of views in the main window
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0

Rectangle {
    id: backgroundRect
    property int buttonHeight: 50
    signal menuButtonClicked(identification: string)
    color:  MMPTheme.themeSelect(MMPTheme.cViolentViolet, MMPTheme.cSpaceBlack)

    Shortcut {
        sequences: [StandardKey.Preferences]
        onActivated: {
            menuList.currentIndex = 6
            backgroundRect.menuButtonClicked("SettingsView.qml")
        }
    }

    ListModel {
        id: menuModel
        ListElement {
            label: qsTr("Overview")
            lightImageSource: "qrc:/icons/menu/ic_menu_overview_white.svg"
            darkImageSource: "qrc:/icons/menu/ic_menu_overview_blue.svg"
            path: "OverviewView.qml"
        }
        ListElement {
            label: qsTr("Send")
            lightImageSource: "qrc:/icons/menu/ic_menu_send_white.svg"
            darkImageSource: "qrc:/icons/menu/ic_menu_send_blue.svg"
            path: "SendView.qml"
        }
        ListElement {
            label: qsTr("Receive")
            lightImageSource: "qrc:/icons/menu/ic_menu_receive_white.svg"
            darkImageSource: "qrc:/icons/menu/ic_menu_receive_blue.svg"
            path: "ReceiveView.qml"
        }
        ListElement {
            label: qsTr("History")
            lightImageSource: "qrc:/icons/menu/ic_menu_history_white.svg"
            darkImageSource: "qrc:/icons/menu/ic_menu_history_blue.svg"
            path: "HistoryView.qml"
        }
        ListElement {
            label: qsTr("Favourites")
            lightImageSource: "qrc:/icons/menu/ic_menu_favorites_white.svg"
            darkImageSource: "qrc:/icons/menu/ic_menu_favorites_blue.svg"
            path: "FavoritesView.qml"
        }
        ListElement {
            label: qsTr("Polls")
            lightImageSource: "qrc:/icons/menu/ic_menu_polls_white.svg"
            darkImageSource: "qrc:/icons/menu/ic_menu_polls_blue.svg"
            path: "PollView.qml"
        }
//        ListElement {
//            label: qsTr("News")
//            lightImageSource: "qrc:/icons/menu/ic_menu_news_white.svg"
//            darkImageSource: "qrc:/icons/menu/ic_menu_news_blue.svg"
//        }
        ListElement {
            label: qsTr("Settings")
            lightImageSource: "qrc:/icons/menu/ic_menu_settings_white.svg"
            darkImageSource: "qrc:/icons/menu/ic_menu_settings_blue.svg"
            path: "SettingsView.qml"
        }
    }

    Rectangle {
        //id: rightBorderRect
        z:2
        width: 1
        color: MMPTheme.themeSelect("transparent", MMPTheme.cBlack)
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
    }

    Rectangle {
        id: logoRect
        implicitWidth: parent.width
        implicitHeight: 70
        color: "transparent"
        Rectangle {
            //id: bottomBorder
            color: MMPTheme.themeSelect("#4b1a75", MMPTheme.cBlack)
            height: 1
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
        }
        Image {
            //id: gridcoinLogo
            source: "qrc:/icons/logos/ic_logo_app_gradient_white.svg"
            anchors.centerIn: parent
            width: parent.width - 20
            height: parent.height - 20
            sourceSize: Qt.size(width, height)
            fillMode: Image.PreserveAspectFit
        }
    }
    Column {
        //id: buttonColumn
        width: parent.width
        anchors {
            top: logoRect.bottom
            bottom: boinc.top
            bottomMargin: 10
        }

        ListView {
            id: menuList
            height: Math.min(buttonHeight*count, parent.height-separator.height-lockButton.height)
            implicitHeight: contentItem.height
            implicitWidth: parent.width
            clip: true
            interactive: buttonHeight*count > height
            model: menuModel
            currentIndex: 0
            delegate: MenuButton {
                width: parent.width
                height: buttonHeight
                labelText: label
                imagePath: MMPTheme.themeSelect(lightImageSource, darkImageSource)
                current: ListView.isCurrentItem
                onClicked: {
                    menuList.currentIndex = index
                    backgroundRect.menuButtonClicked(path)
                }
            }
        }
        Item {
            id: separator
            height: 20
            width: parent.width
            Rectangle {
                height: 1
                anchors.centerIn: parent
                width: parent.width - 50
                color: MMPTheme.translucent(MMPTheme.cWhite, 0.7)

            }
        }
        MenuButton{
            id: lockButton
            height: buttonHeight+15
            width: parent.width-30
            labelText: qsTr("Lock Wallet")
            imagePath: MMPTheme.themeSelect("qrc:/icons/menu/ic_menu_lock_white.svg","qrc:/icons/menu/ic_menu_lock_blue.svg")
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: {
                if (MMPTheme.isLightTheme){
                    MMPTheme.theme = MMPTheme.darkTheme
                } else {
                    MMPTheme.theme = MMPTheme.lightTheme
                }
                _walletModel.requireUnlock()
            }
        }
    }
    Image {
        id: boinc
        source: "qrc:/icons/logos/ic_logo_boinc_color.svg"
        width: parent.width-30
        height: 30
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: 10
        }

    }
}
