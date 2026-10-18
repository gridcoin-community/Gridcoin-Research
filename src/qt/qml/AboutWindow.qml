/*
    A typical application about window
*/
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import MMPTheme 1.0
import Gridcoin.Qml
Window {
    id: window
    flags: Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: qsTr("About Gridcoin")
    visible: true
    // The multiprocess section adds its two items and their spacing.
    minimumHeight: Shell.ipcActive ? 280 + ipcHeader.implicitHeight + ipcSection.implicitHeight + 2 * textColumn.spacing : 280
    minimumWidth: 650
    height: minimumHeight
    width: minimumWidth
    maximumHeight: minimumHeight
    maximumWidth: minimumWidth

    // The request id of the update check whose result this window shows, or
    // 0. A closed window clears it, so a result that arrives later opens
    // nothing.
    property var checkId: 0

    // Each About open re-reads the update-check settings, and takes over a
    // check that a closed About window started and that is still running.
    Component.onCompleted: {
        AboutInfo.refreshUpdateCheck()
        if (AboutInfo.updateCheckRunning) checkId = AboutInfo.pendingRequestId
    }
    onClosing: checkId = 0

    Connections {
        target: AboutInfo
        function onUpdateCheckFinished(requestId, version, details, upgradeType) {
            if (requestId === 0 || requestId !== window.checkId) return
            var component = Qt.createComponent("VersionInfoWindow.qml")
            var versionWindow = component.createObject(window, { version: version, details: details, upgradeType: upgradeType })
            versionWindow.show()
        }
    }
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
            objectName: "versionText"
            color: MMPTheme.themeSelect(MMPTheme.cLightSlateGray, MMPTheme.cWhite)
            text: AboutInfo.fullVersion
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
                objectName: "copyrightText"
                color: MMPTheme.textColor
                text: AboutInfo.copyrightText
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
            Item {
                // The tooltip says why the button is disabled; the hover is
                // read here, around the button.
                implicitWidth: versionInfoButton.implicitWidth
                implicitHeight: versionInfoButton.implicitHeight
                HoverHandler { id: versionInfoHover }
                ToolTip.visible: versionInfoHover.hovered && AboutInfo.updateCheckDisabledReason !== ""
                ToolTip.text: AboutInfo.updateCheckDisabledReason
                Button {
                    id: versionInfoButton
                    objectName: "versionInfoButton"
                    anchors.fill: parent
                    text: qsTranslate("AboutDialog", "Version Information")
                    enabled: AboutInfo.updateCheckAvailable && !AboutInfo.updateCheckRunning
                    onClicked: window.checkId = AboutInfo.checkForUpdate()
                }
            }
            Text {
                id: ipcHeader
                objectName: "ipcHeader"
                visible: Shell.ipcActive
                width: parent.width
                color: MMPTheme.textColor
                textFormat: Text.StyledText
                text: qsTranslate("AboutDialog", "<b>Multiprocess connection</b>")
            }
            Text {
                id: ipcSection
                objectName: "ipcSection"
                visible: Shell.ipcActive
                width: parent.width
                color: MMPTheme.textColor
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                text: qsTranslate("AboutDialog", "GUI version: %1").arg(Shell.ipcGuiVersion) + "\n"
                      + qsTranslate("AboutDialog", "Node version: %1").arg(Shell.ipcNodeVersion) + "\n"
                      + qsTranslate("AboutDialog", "Node built: %1").arg(Shell.ipcNodeBuiltAt) + "\n"
                      + qsTranslate("AboutDialog", "IPC schema %1, protocol %2").arg(Shell.ipcSchema).arg(Shell.ipcProtocol) + "\n"
                      + qsTranslate("AboutDialog", "Node identity: %1 (%2)").arg(Shell.ipcNodeIdentity === "" ? qsTranslate("AboutDialog", "unavailable") : Shell.ipcNodeIdentity).arg(Shell.ipcNetwork) + "\n"
                      + qsTranslate("AboutDialog", "Socket: %1").arg(Shell.ipcSocketPath)
            }
        }
    }
}
