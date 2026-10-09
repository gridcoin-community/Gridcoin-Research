/*
    Used in PollView to filter polls
    Currently only used in PollView but could be used elsewhere
*/
import QtQuick
import QtQuick.Controls
import MMPTheme 1.0

Button {
    id: control
    //Has property text
    property bool current: false
    leftPadding: 20

    contentItem: Item {
        implicitHeight: label.implicitHeight
        implicitWidth: label.implicitWidth
        Text {
            id: label
            text: control.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.centerIn: parent
            elide: Text.ElideRight
            color: {
                if (current){
                    return MMPTheme.isLightTheme ? MMPTheme.cOxfordBlue : MMPTheme.cHavelockBlue;
                } else {
                    return MMPTheme.translucent(MMPTheme.isLightTheme ? MMPTheme.cOxfordBlue : MMPTheme.cWhite, 0.7);
                }
            }
        }
        Rectangle {
            height: 3
            color: {
                if (current){
                    return MMPTheme.isLightTheme ? MMPTheme.cOxfordBlue : MMPTheme.cHavelockBlue;
                } else {
                    return "transparent";
                }
            }
            anchors {
                left: label.left
                right: label.right
                bottom: parent.bottom
            }
        }
    }

    background: Rectangle {
        color: "transparent"
        implicitHeight: 40

    }

}
