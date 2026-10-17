import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MMPTheme 1.0

T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + 2 * padding +
                            up.implicitIndicatorWidth +
                            down.implicitIndicatorWidth)
    implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding,
                             implicitBackgroundHeight,
                             up.implicitIndicatorHeight,
                             down.implicitIndicatorHeight)

    padding: 5
    leftPadding: padding + 5
    rightPadding: 25
    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    up.indicator: Rectangle {
        x: parent.width - width
        y: 0
        implicitWidth: 12
        implicitHeight: 12
        width: implicitWidth + 2*control.padding
        height: parent.height / 2
        color: "transparent"
        Image {
            x: parent.width / 2 - width / 2
            y: parent.height / 2 - height / 2
            source: MMPTheme.themeSelect("qrc:/icons/generic/ic_chevron_up_light.svg", "qrc:/icons/generic/ic_chevron_up_dark.svg")
            opacity: !control.enabled ? 0.3 : (control.activeFocus ? 1 : 0.7)
            sourceSize: Qt.size(10,10)
        }
    }

    down.indicator: Rectangle {
        x: parent.width - width
        y: parent.height / 2
        implicitWidth: 12
        implicitHeight: 12
        width: implicitWidth + 2*control.padding
        height: parent.height / 2
        color: "transparent"
        Image {
            x: parent.width / 2 - width / 2
            y: parent.height / 2 - height / 2
            source: MMPTheme.themeSelect("qrc:/icons/generic/ic_chevron_down_light.svg", "qrc:/icons/generic/ic_chevron_down_dark.svg")
            opacity: !control.enabled ? 0.3 : (control.activeFocus ? 1 : 0.7)
            sourceSize: Qt.size(10,10)
        }
    }

    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        font: MMPTheme.baseFont
        color: MMPTheme.translucent(MMPTheme.themeSelect(MMPTheme.cOxfordBlue, MMPTheme.cWhite), !control.enabled ? 0.3 : (control.activeFocus ? 1 : 0.7))
        selectionColor: control.palette.highlight
        selectedTextColor: control.palette.highlightedText
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        clip: true
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    // Line separator
    Rectangle {
        x: Math.min(up.indicator.x, down.indicator.x) - 1
        y: 0
        width: 1
        height: parent.height
        color: control.background.border.color
    }

    // Indicator background
    Item {
        id: element
        visible: MMPTheme.isLightTheme
        x: Math.min(up.indicator.x, down.indicator.x)
        y: control.background.border.width
        width: parent.width - x
        height: parent.height - 2*control.background.border.width
        clip: true
        Rectangle {
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0; color: control.activeFocus ? MMPTheme.cFrostWhite : MMPTheme.cWhite }
                GradientStop { position: 1; color: control.activeFocus ? MMPTheme.cWhite : MMPTheme.cFrostWhite }
            }

            width: parent.width + control.background.radius
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.bottom: parent.bottom

            radius: control.background.radius
        }
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 24
        border.width: 1
        radius: 4
        color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cOxfordOffBlue)
        border.color: MMPTheme.themeSelect(MMPTheme.translucent(MMPTheme.cOxfordBlue, !control.enabled ? 0.2 : (control.activeFocus ? 0.7 : 0.3)),
                                                   control.activeFocus ? MMPTheme.translucent(MMPTheme.cWhite, 0.7) : MMPTheme.translucent(MMPTheme.cOxfordBlue, control.enabled ? 1 : 0.3))
    }
}
