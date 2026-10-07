/*
    A typical application about window
*/
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import MMPTheme 1.0
Window {
    id: window
    flags: Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: qsTr("About Gridcoin")
    visible: true
    minimumHeight: 280
    minimumWidth: 650
    height: minimumHeight
    width: minimumWidth
    maximumHeight: minimumHeight
    maximumWidth: minimumWidth
    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: window.close()
    }
    Rectangle {
        id: background
        anchors.fill: parent
        border.color: "transparent"
        gradient: Gradient {
            orientation: MMPTheme.isLightTheme ? Gradient.Vertical : Gradient.Horizontal
            GradientStop { position: 0; color: MMPTheme.themeSelect(MMPTheme.cWhite, "#465360") }
            GradientStop { position: 1; color: MMPTheme.themeSelect(MMPTheme.cLilyWhite, "#2c3540") }
        }
        Image {
            id: logoImage
            source: "qrc:/icons/logos/ic_logo_app_gradient_white.svg"
            sourceSize: Qt.size(160, 160)
            anchors {
                top: parent.top
                left: parent.left
                margins: 25
            }
        }
        Text {
            id: gridcoinText
            text: qsTr("Gridcoin")
            color: MMPTheme.themeSelect("#4e2fad", MMPTheme.cWhite)
            font.pixelSize: 34
            font.weight: Font.Bold
            font.family: "Montserrat"
            anchors {
                horizontalCenter: logoImage.horizontalCenter
                top: logoImage.bottom
                topMargin: 5
            }
        }
        Text {
            id: versionText
            color: MMPTheme.themeSelect(MMPTheme.cLightSlateGray, MMPTheme.cWhite)
            text: _clientModel.fullVersion
            font.weight: Font.DemiBold
            font.family: "Montserrat"
            anchors {
                top: gridcoinText.bottom
                horizontalCenter: gridcoinText.horizontalCenter
                topMargin: 5
            }
        }
        Column{
            id: textColumn
            spacing: 20
            anchors {
                left: gridcoinText.right
                leftMargin: 20
                right: parent.right
                rightMargin: 20
                verticalCenter: parent.verticalCenter
            }
            Text {
                color: MMPTheme.textColor
                text: qsTr("© 2009 - The Bitcoin/Peercoin/Black-Coin/Gridcoin Developers.")
            }
            Text {
                color: MMPTheme.textColor
                text: qsTr("This is experimental software.")
            }
            Text {
                width: parent.width
                color: MMPTheme.textColor
                wrapMode: Text.WordWrap
                text: qsTr("Distributed under the MIT/X11 software license, see the accompanying file COPYING or https://www.opensource.org/licenses/mit-license.php.")
            }
            Text {
                width: parent.width
                color: MMPTheme.textColor
                wrapMode: Text.WordWrap
                text: qsTr("This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (https://openssl.org) and cryptographic software written by Eric Young (eay@cryptsoft) and UPnP software written by Thomas Bernard.")
            }
        }
    }
}
