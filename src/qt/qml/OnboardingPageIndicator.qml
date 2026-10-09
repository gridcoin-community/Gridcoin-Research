/*
    Stylized current page indicator used in the onboarding
*/
import QtQuick
import MMPTheme 1.0
Item {
    id: pageIndicator
    property int pageCount: 3
    property int currentPage: 1
    height: listView.implicitHeight
    width: listView.implicitWidth
    ListView {
        id: listView
        implicitHeight: 8
        implicitWidth: pageCount*8 + (pageCount-1) * spacing
        interactive: false
        orientation: ListView.Horizontal
        spacing: 20
        currentIndex: currentPage - 1
        model: pageCount
        delegate: Circle {
            radius: ListView.isCurrentItem ? 5 : 4
            color: ListView.isCurrentItem ? MMPTheme.cWhite : MMPTheme.translucent(MMPTheme.cBlack, 0.4)
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
