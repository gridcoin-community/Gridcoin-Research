/*
    Provides a blue clickable hyperlink with an icon identifying it as an external resource.
    The displayed text can be set to different from the link by setting displayString
*/
import QtQuick
import MMPTheme 1.0

MouseArea {
    id: mouseArea
    property string urlString: "https://gridcoin.us"
    property string displayString   //Set to show text different from link
    width: linkIcon.implicitWidth + linkLabel.implicitWidth + linkLabel.anchors.leftMargin
    height: 15
    cursorShape: Qt.PointingHandCursor
    onClicked: {
        Qt.openUrlExternally(urlString)
    }

    Image {
        id: linkIcon
        height: parent.height
        width: height
        sourceSize.width: width
        sourceSize.height: height
        source: "qrc:/icons/generic/ic_open_link.svg"
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
        }
    }
    Text {
        id: linkLabel
        color: MMPTheme.cHavelockBlue
        text: displayString ? displayString : urlString
        verticalAlignment: Text.AlignVCenter
        anchors {
            left: linkIcon.right
            leftMargin: 3
            verticalCenter: parent.verticalCentre
        }
    }
}
