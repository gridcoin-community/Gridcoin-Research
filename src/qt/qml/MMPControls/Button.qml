import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MMPTheme 1.0

T.Button {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 4
    spacing: 4

    property bool active: visualFocus || hovered

    icon.width: 16
    icon.height: 16

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text
        font: MMPTheme.baseFont
        color: MMPTheme.themeSelect(MMPTheme.cOxfordBlue, MMPTheme.cWhite)

        opacity: !control.enabled ? 0.3 : (control.down ? 1 : 0.7)
    }

    background: Rectangle {
        implicitWidth: control.text != "" ? 100 : 24
        implicitHeight: 24
        visible: !control.flat || control.down || control.checked || control.highlighted
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0
                color: control.active && !control.down ?
                           MMPTheme.themeSelect(MMPTheme.cLilyWhite, "#1991eb") :
                           MMPTheme.themeSelect(MMPTheme.cWhite, "#2da1f8")
            }
            GradientStop {
                position: 1
                color: control.active && !control.down ?
                           MMPTheme.themeSelect(MMPTheme.cWhite, "#2da1f8") :
                           MMPTheme.themeSelect(MMPTheme.cLilyWhite, "#1991eb") }
        }

        border.color: MMPTheme.themeSelect(MMPTheme.translucent(MMPTheme.cOxfordBlue, !control.enabled ? 0.2 : (control.down ? 0.7 : (control.active ? 0.5 : 0.3))),
                                  MMPTheme.translucent(MMPTheme.cBlack, control.enabled && (control.active || control.down) ? 0.15 : 0.05))
        border.width: 1
        radius: 4
    }
}
