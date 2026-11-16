import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls
import QtQuick.Controls.impl
import MMPTheme 1.0

T.RadioButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    //padding: 6
    spacing: 5

    property bool active: visualFocus || hovered

    // keep in sync with RadioDelegate.qml (shared RadioIndicator.qml was removed for performance reasons)
    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: width / 2
        border.width: 1
        // TODO Opacity should be 1 when checked in dark mode
        border.color: MMPTheme.translucent(control.checked ? MMPTheme.themeSelect(MMPTheme.cBluePurple, MMPTheme.cHavelockBlue) : MMPTheme.themeSelect(MMPTheme.cOxfordBlue, MMPTheme.cHavelockBlue), control.checked || control.active || control.activeFocus ? MMPTheme.themeSelect(0.7, 1) : 0.3)
        gradient: Gradient {
              orientation: Gradient.Vertical
              GradientStop { position: 0; color: control.active ? MMPTheme.cLilyWhite : MMPTheme.cWhite }
              GradientStop { position: 1; color: control.active ? MMPTheme.cWhite : MMPTheme.cLilyWhite }
          }

        Rectangle {
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            width: 6
            height: 6
            radius: width / 2
            color: MMPTheme.themeSelect(MMPTheme.translucent(MMPTheme.cBluePurple, 0.7), MMPTheme.cHavelockBlue)
            visible: control.checked
        }
    }

    contentItem: CheckLabel {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        font: MMPTheme.baseFont
        color: MMPTheme.textColor
    }
}
