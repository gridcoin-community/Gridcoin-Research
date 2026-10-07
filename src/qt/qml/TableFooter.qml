/*
    Gradiented rectangle sometimes used at the bottom of a table
*/
import QtQuick
import MMPTheme 1.0

Rectangle {
    id: tableFooterRect
    x:1
    height: 24
    width: parent.width-2
    radius: parent.radius
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 1
    property alias borderColor: borderTopRect.color
    gradient: Gradient {
        GradientStop { position: 0; color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cMirage)}
        GradientStop { position: 1; color: MMPTheme.themeSelect(MMPTheme.cLilyWhite, MMPTheme.cMirage)}
    }
    Rectangle {
        id: borderTopRect
        width: parent.width
        height: 1
        color: MMPTheme.lightBorderColor
    }
}
