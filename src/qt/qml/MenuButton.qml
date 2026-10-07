/*
    Button used in the TabMenu to select different views in the MainWindow
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0
Button {
    id: buttonMain
    property alias imagePath: buttonIcon.source
    property alias labelText: label.text
    property int svgScale: 50
    property bool current: false
    implicitWidth: col.implicitWidth
    implicitHeight: col.implicitHeight
    hoverEnabled: true
    background: Column {
        id: col
        property int topPad: 10
        property int midPad: 3
        property int botPad: 3
        //anchors.fill: parent
        height: parent.height
        width: parent.width
        anchors.centerIn: parent
        Item {
            //id: topPadding
            height: parent.topPad
            width: parent.width
        }

        ColorizableImage {
            id: buttonIcon
            width: parent.width
            height: parent.height-label.height-col.topPad-col.midPad-col.botPad
            colorization: 1.0
            tintColor: {
                if (current || buttonMain.hovered) {
                    return MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cHavelockBlue)
                } else {
                    return MMPTheme.themeSelect("#c3b2d2", "#babec1")
                }
            }

        }

        Item {
            //id: midPadding
            height: parent.midPad
            width: parent.width
        }
        Text {
            id: label
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
            font: MMPTheme.baseFont
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap

            color: {
                if (current || buttonMain.hovered) {
                    return MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cHavelockBlue)
                } else {
                    return MMPTheme.translucent(MMPTheme.cWhite, 0.7)
                }
            }
        }
        Item {
            //id: bottomPadding
            height: parent.botPad
            width: parent.width
        }
    }
}
