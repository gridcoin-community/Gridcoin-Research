/*
    The result of a version information check: the version, its details and
    an icon for its upgrade type. AboutWindow.qml creates one for each result
    it shows, with the three values as initial properties, and shows it.
*/
import QtQuick
import QtQuick.Window
import MMPTheme 1.0

Window {
    id: versionInfoWindow
    objectName: "versionInfoWindow"

    property string version
    property string details
    // The upgrade type: 0 unknown, 1 leisure, 2 mandatory, 3 unsupported.
    property int upgradeType: 0

    flags: Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: qsTr("Gridcoin Version Information")
    width: 480
    height: 300
    minimumWidth: 360
    minimumHeight: 200
    color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cSpaceBlack)

    Image {
        id: typeIcon
        source: (versionInfoWindow.upgradeType === 2 || versionInfoWindow.upgradeType === 3)
                ? "qrc:/icons/generic/ic_alert.svg"
                : versionInfoWindow.upgradeType === 1
                  ? MMPTheme.themeSelect("qrc:/icons/generic/ic_info_light.svg", "qrc:/icons/generic/ic_info_dark.svg")
                  : "qrc:/icons/generic/ic_help.svg"
        sourceSize: Qt.size(48, 48)
        anchors {
            top: parent.top
            left: parent.left
            margins: 20
        }
    }
    Text {
        id: versionLabel
        objectName: "versionInfoVersion"
        textFormat: Text.PlainText
        text: versionInfoWindow.version
        font.bold: true
        color: MMPTheme.textColor
        wrapMode: Text.WordWrap
        anchors {
            top: parent.top
            left: typeIcon.right
            right: parent.right
            margins: 20
        }
    }
    Text {
        id: detailsLabel
        objectName: "versionInfoDetails"
        textFormat: Text.PlainText
        text: versionInfoWindow.details
        color: MMPTheme.textColor
        wrapMode: Text.WordWrap
        anchors {
            top: versionLabel.bottom
            left: typeIcon.right
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }
    }
}
