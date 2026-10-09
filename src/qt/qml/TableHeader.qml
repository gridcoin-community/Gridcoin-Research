/*
    Gradiented rectangle with headers used at the top of a table
    Allows for the changing of sort order
*/
import QtQuick
import MMPTheme 1.0

Rectangle {
    id: tableHeaderRect
    x:1
    y:1
    height: 25
    width: parent.width-2
    radius: parent.radius
    property int currentSortIndex: 0
    property alias model: columnHeadersRepeater.model   //A list with each item describing a column header eg. [{text:"", width:100}]
    property alias borderColor: borderBottomRect.color
    signal columnSortOrderChanged(int columnIndex, bool ascending)
    gradient: Gradient {
        GradientStop { position: 0; color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cMirage) }
        GradientStop { position: 1; color: MMPTheme.themeSelect(MMPTheme.cLilyWhite, MMPTheme.cMirage) }
    }
    Rectangle {
        id: roundedCornerFillRect
        width: parent.width
        gradient: Gradient {
                GradientStop { position: 0; color: MMPTheme.themeSelect("#f6f7f7", MMPTheme.cMirage) }
                GradientStop { position: 1; color: MMPTheme.themeSelect(MMPTheme.cLilyWhite, MMPTheme.cMirage) }
            }
        anchors {
            top: parent.verticalCenter
            bottom: parent.bottom
        }
    }

    Rectangle {
        id: borderBottomRect
        width: parent.width
        height: 1
        color: MMPTheme.lightBorderColor
        anchors.bottom: parent.bottom
    }


    Row {
        anchors.fill: parent
        Repeater {
            id: columnHeadersRepeater
            MouseArea {
                id: headerArea
                property bool ascending: false
                property bool active: tableHeaderRect.currentSortIndex === model.index
                height: parent.height
                width: modelData.width
                implicitWidth: headerLabel.implicitWidth+headerArrow.implicitWidth+headerArea.anchors.leftMargin+headerLabel.anchors.leftMargin
                clip: true
                onClicked: {
                    if (active){
                        ascending = !ascending
                    } else {
                        tableHeaderRect.currentSortIndex = index
                    }
                    columnSortOrderChanged(index, ascending)
                }
                Text {
                    id: headerLabel
                    text: modelData.text
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    color: MMPTheme.textColor
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    font.pixelSize: 11
                }
                Image {
                    id: headerArrow
                    visible: active
                    height: 10
                    width: 10
                    source: headerArea.ascending ?
                                MMPTheme.themeSelect("qrc:/icons/generic/ic_arrow_up_light.svg","qrc:/icons/generic/ic_arrow_up_dark.svg") :
                                MMPTheme.themeSelect("qrc:/icons/generic/ic_arrow_down_light.svg","qrc:/icons/generic/ic_arrow_down_dark.svg")
                    sourceSize: Qt.size(width, height)
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: headerLabel.right
                        leftMargin: 5
                    }
                }
            }
        }
    }
}
