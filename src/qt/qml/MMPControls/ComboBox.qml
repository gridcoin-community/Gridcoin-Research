import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MMPTheme 1.0


T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 5
    leftPadding: padding + 5

    delegate: ItemDelegate {
        width: parent.width
        height: 24
        text: control.textRole ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole]) : modelData
        font.pixelSize: MMPTheme.baseFont.pixelSize
        font.weight: control.currentIndex === index ? Font.Medium : Font.Normal
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
        contentItem: Text {
            y: (parent.height-height)/2
            text: parent.text
            color: MMPTheme.textColor
            verticalAlignment: Text.AlignVCenter

        }
        background: Rectangle {
            x: 1
            y: 1
            width: parent.width-2
            height: parent.height-2
            radius: 4
            color: highlighted ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, MMPTheme.translucent(MMPTheme.cOxfordBlue, 0.3)) : "transparent"
        }
    }

    indicator: Image {
        x: parent.width - width - parent.padding
        y: parent.height / 2 - height / 2
        source: MMPTheme.themeSelect("qrc:/icons/generic/ic_chevron_down_light.svg", "qrc:/icons/generic/ic_chevron_down_dark.svg")
        opacity: !control.enabled ? 0.3 : (control.activeFocus ? 1 : 0.7)
        sourceSize: Qt.size(15, 15)
    }

    contentItem: T.TextField {
        text: control.editable ? control.editText : control.displayText
        font: MMPTheme.baseFont

        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator

        color: MMPTheme.translucent(MMPTheme.themeSelect(MMPTheme.cOxfordBlue, MMPTheme.cWhite), !control.enabled ? 0.3 : (control.activeFocus ? 1 : 0.7))
        selectionColor: control.palette.highlight
        selectedTextColor: control.palette.highlightedText
        verticalAlignment: Text.AlignVCenter
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

    popup: T.Popup {
        y: control.height + 1
        width: control.width
        implicitHeight: contentItem.implicitHeight
        topMargin: 6
        bottomMargin: 6

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

        }

        //T.ScrollIndicator.vertical: ScrollIndicator { }



        background: Rectangle {
            color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cOxfordOffBlue)
            radius: 4
            border.color: MMPTheme.themeSelect(MMPTheme.translucent(MMPTheme.cOxfordBlue, !control.enabled ? 0.1 : (control.activeFocus ? 0.7 : 0.4)),
                                               control.activeFocus ? MMPTheme.translucent(MMPTheme.cWhite, 0.7) : MMPTheme.translucent(MMPTheme.cOxfordBlue, control.enabled ? 1 : 0.3));
            border.width: 1
        }
    }
}
