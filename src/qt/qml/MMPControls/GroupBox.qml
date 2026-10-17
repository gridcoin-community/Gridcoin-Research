import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Templates as T
import QtQuick.Effects
import MMPTheme 1.0

T.GroupBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding,
                            implicitLabelWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding + label.height + spacing)

    spacing: 10
    padding: 15
    topPadding: padding
    //topPadding: padding + (implicitLabelWidth > 0 ? implicitLabelHeight + spacing : 0)

    label: Text {
        anchors.top: parent.top
        anchors.topMargin: parent.padding
        anchors.left: parent.left
        anchors.leftMargin: parent.padding
        anchors.right: parent.right
        anchors.rightMargin: parent.padding

        text: control.title
        font.family: MMPTheme.baseFont.family
        font.pointSize: 11
        font.weight: Font.DemiBold
        color: MMPTheme.themeSelect(MMPTheme.cOxfordBlue, MMPTheme.cLilyWhite)
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    // TODO Add separator

    contentItem.anchors.top: control.label.bottom
    contentItem.anchors.topMargin: control.spacing

    background: Item {
        anchors.fill: parent

        Rectangle {
            id: background
            anchors.fill: parent

            color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cMirage)
            radius: 4
            border.width: 0
        }

        RectangularShadow {
            anchors.fill: parent
            offset.y: 1
            radius: 0
            color: MMPTheme.themeSelect("#0d000000", "#4d000000")
        }
    }
}
