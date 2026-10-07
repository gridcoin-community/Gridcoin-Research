import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls
import QtQuick.Controls.impl
import MMPTheme 1.0

T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    padding: 5
    leftPadding: padding + 5

    color: MMPTheme.translucent(MMPTheme.themeSelect(MMPTheme.cOxfordBlue, MMPTheme.cWhite), !control.enabled ? 0.3 : (control.activeFocus ? 1 : 0.7))
    selectionColor: control.palette.highlight
    selectedTextColor: control.palette.highlightedText
//    placeholderTextColor: Color.transparent(control.color, 0.5)
    placeholderTextColor: MMPTheme.translucent(MMPTheme.themeSelect(MMPTheme.cOxfordBlue, MMPTheme.cWhite), 0.3)
    verticalAlignment: TextInput.AlignVCenter

    font: MMPTheme.baseFont

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)

        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
        renderType: control.renderType
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 24
        border.width: 1
        radius: 4
        color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cOxfordOffBlue)
        border.color: MMPTheme.themeSelect(MMPTheme.translucent(MMPTheme.cOxfordBlue, !control.enabled ? 0.1 : (control.activeFocus ? 0.7 : 0.4)),
                                                   control.activeFocus ? MMPTheme.translucent(MMPTheme.cWhite, 0.7) : MMPTheme.translucent(MMPTheme.cOxfordBlue, control.enabled ? 1 : 0.3));
    }
}
