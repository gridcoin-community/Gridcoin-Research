/*
    A text header with an underline, typically used for a 'panel' of information
*/
import QtQuick
import MMPTheme 1.0
Item {
    id: titleRow
    property alias text: title.text
    implicitHeight: title.implicitHeight+separator.implicitHeight+separator.anchors.topMargin

    Text {
        id: title
        color: MMPTheme.textColor
        font.weight: Font.DemiBold
        font.pointSize: 13
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left

        }
    }
    Rectangle {
        id: separator
        height: 1
        width: parent.width
        color: MMPTheme.separatorColor
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: titleRow.bottom
            topMargin: 5
        }
    }
}
