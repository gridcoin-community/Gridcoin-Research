/*
    A sylized popup showing text.
    Often used in conjuction with the HelpHover class
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Effects
import MMPTheme 1.0
ToolTip {
    id: main
    delay: 200
    contentWidth: infoTextItem.implicitWidth
    contentHeight: infoTextItem.implicitHeight
    verticalPadding: 1
    horizontalPadding: 3
    property alias infoText: infoTextItem.text
    background: Item {
        id: background
        RectangularShadow {
            id: rightShadow
            anchors.fill: backgroundRect
            offset.y: 2
            radius: 5
            cached: true
            color: "#44000000"
        }
        Rectangle {
            id: backgroundRect
            color: MMPTheme.bodyColor
            radius: 4
            border.color: MMPTheme.translucent(MMPTheme.themeSelect(MMPTheme.cLilyWhite,MMPTheme.cOxfordOffBlue), 1)
            border.width: 1
            anchors.fill: parent
        }


    }
    contentItem:
        Text {
            id: infoTextItem
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            textFormat: Text.RichText
            color: MMPTheme.lightTextColor
            //horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

        }
}
