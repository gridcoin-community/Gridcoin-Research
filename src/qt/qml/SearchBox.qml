/*
    A sylized search box
*/
import QtQuick
import QtQuick.Controls
import MMPTheme 1.0

FocusScope {
    id: control
    implicitHeight: 30
    implicitWidth: 200
    property alias placeholderText: searchTextField.placeholderText
    property alias text: searchTextField.text
    Rectangle {
        id: backgroundRect
        radius: 4
        color: MMPTheme.themeSelect(MMPTheme.cFrostWhite, MMPTheme.cMirage)
        border.color: MMPTheme.themeSelect(MMPTheme.translucent(MMPTheme.cOxfordBlue, !control.enabled ? 0.1 : (control.activeFocus ? 0.7 : 0.4)),
                                           control.activeFocus ? MMPTheme.translucent(MMPTheme.cWhite, 0.7) : MMPTheme.translucent(MMPTheme.cOxfordBlue, control.enabled ? 1 : 0.3));
        anchors.fill: parent

        ColorizableImage {
            id: searchIcon
            height: parent.height*0.5
            width: height
            source: MMPTheme.themeSelect("qrc:/icons/generic/ic_search_light.svg","qrc:/icons/generic/ic_search_dark.svg")
            tintColor: MMPTheme.themeSelect(control.activeFocus ? "#757d8e" : "#b0b5be",
                                            control.activeFocus ? "#babcbe" : "#3a465e")
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 8
            }
        }
        TextField {
            id: searchTextField
            focus: true
            height: parent.height
            anchors {
                left: searchIcon.right
                right: parent.right
            }
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }
        }
    }
}
