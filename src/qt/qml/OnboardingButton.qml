/*
    Stylized button used in the onboarding
*/
import QtQuick
import QtQuick.Controls
import MMPTheme 1.0

Button {
    id: control
    horizontalPadding: 10
    opacity: control.enabled ? 1 : 0.7
    background: Rectangle {
        implicitWidth: 110
        implicitHeight: 40
        radius: height/2
        color: MMPTheme.translucent(MMPTheme.cWhite, 0.3)
        border.color: MMPTheme.translucent(MMPTheme.cWhite, control.hovered && control.enabled ? 1 : 0.7)
    }
    contentItem: Text {
        text: control.text
        color: MMPTheme.translucent(MMPTheme.cWhite, control.hovered && control.enabled ? 1 : 0.8)
        font.pixelSize: 14
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
