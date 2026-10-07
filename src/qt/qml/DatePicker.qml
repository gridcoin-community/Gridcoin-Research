/*
    A popup displaying a calendar allowing for the user selection of a date range
*/
import QtQuick
import QtQuick.Controls 1.4 as QC1
import QtQuick.Controls.Styles 1.4
import QtQuick.Controls 2.15
import QtQuick.Effects
import MMPTheme 1.0

Popup {
    id: popup
    property string currentSelectionType: allTimeButtonText.text
    property alias selectionStart: calendar.selectionStart
    property alias selectionEnd: calendar.selectionEnd
    width: 420
    height: 330
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Item {
        RectangularShadow {
            id: rightShadow
            anchors.fill: backgroundRect
            offset.y: 2
            radius: 5
            cached: true
            color: "#44000000"
        }
        Rectangle {
            id: backgroundRect
            anchors.fill: parent
            color: MMPTheme.themeSelect(MMPTheme.cWhite, "#333d47")
            border.color: MMPTheme.themeSelect(MMPTheme.cLilyWhite,MMPTheme.cOxfordOffBlue)
            radius: 4
        }
    }
    contentItem: Item {
        id: content
        anchors.fill: parent
        QC1.Calendar {
            id: calendar
            width: 275
            frameVisible: false
            property date selectionStart
            property date selectionEnd: new Date()
            property bool startLastSet: false
            Component.onCompleted: selectionStart = new Date(0)
            onClicked: {
                popup.currentSelectionType=qsTr("Custom")
                dateRangeButtonColumn.currentIndex=-1
                if (startLastSet) {
                    if (selectedDate<selectionStart) {
                        selectionEnd=selectionStart
                        selectionStart=date
                        startLastSet=true
                    } else {
                        selectionEnd=date
                        startLastSet=false
                    }
                } else {
                    if (selectedDate>selectionEnd){
                        selectionStart=selectionEnd
                        selectionEnd=date
                        startLastSet=false
                    } else {
                        selectionStart=date
                        startLastSet=true
                    }
                }
            }
            //Ensure dates always encapsulate the entire day
            onSelectionStartChanged: selectionStart = new Date(selectionStart.setHours(0,0,0,0))
            onSelectionEndChanged: selectionEnd = new Date(selectionEnd.setHours(23, 59, 59, 999))

            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                margins: 10
            }
            style: CalendarStyle {
                id: style
                gridVisible: false
                background: Item{}
                dayDelegate: Item {
                    id: dayDelegateItem
                    property int circleRadius: height*0.9
                    implicitHeight: 50
                    implicitWidth: implicitHeight
                    states: [
                        State {
                            name: "selectionStart"
                            when: calendar.selectionStart.toDateString() === styleData.date.toDateString()
                            PropertyChanges {
                                target: dateBackgroundImg
                                visible: true
                                source: MMPTheme.themeSelect("qrc:/icons/calendar/ic_date_start_background_light.svg","qrc:/icons/calendar/ic_date_start_background_dark.svg")
                            }
                            PropertyChanges { target: dateText; color: MMPTheme.cWhite }
                            PropertyChanges {
                                target: dateBackgroundRightRectangle
                                visible: styleData.index%7<6 && calendar.selectionStart.toDateString() != calendar.selectionEnd.toDateString()
                            }
                        },
                        State {
                            name: "selectionEnd"
                            when: calendar.selectionEnd.toDateString() === styleData.date.toDateString()
                            PropertyChanges {
                                target: dateBackgroundImg
                                visible: true
                                source: MMPTheme.themeSelect("qrc:/icons/calendar/ic_date_end_background_light.svg","qrc:/icons/calendar/ic_date_end_background_dark.svg")
                            }
                            PropertyChanges { target: dateText; color: MMPTheme.cWhite }
                            PropertyChanges {
                                target: dateBackgroundLeftRectangle
                                visible: styleData.index%7>0 && calendar.selectionStart.toDateString() != calendar.selectionEnd.toDateString()
                            }
                        },
                        State {
                            name: "selectionRowStart"
                            when:  styleData.date > calendar.selectionStart && styleData.date < calendar.selectionEnd && styleData.index%7===0
                            PropertyChanges { target: dateBackgroundCircle; visible: true }
                            PropertyChanges { target: dateBackgroundRightRectangle; visible: true }
                        },
                        State {
                            name: "selectionRowEnd"
                            when: styleData.date > calendar.selectionStart && styleData.date < calendar.selectionEnd && styleData.index%7===6
                            PropertyChanges { target: dateBackgroundCircle; visible: true }
                            PropertyChanges { target: dateBackgroundLeftRectangle; visible: true }
                        },
                        State {
                            name: "selectionRowMiddle"
                            when: styleData.date > calendar.selectionStart && styleData.date < calendar.selectionEnd
                            PropertyChanges { target: dateBackgroundRightRectangle; visible: true }
                            PropertyChanges { target: dateBackgroundLeftRectangle; visible: true }
                        }
                    ]
                    Rectangle {
                        id: dateBackgroundCircle
                        visible: false
                        //anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        height: dayDelegateItem.circleRadius
                        width: height
                        radius: height/2
                        color: MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#2e353d")
                    }
                    Rectangle {
                        id: dateBackgroundRightRectangle
                        x: parent.width/2
                        y: (parent.height-height)/2
                        width: parent.width/2
                        height: dayDelegateItem.circleRadius
                        visible: false
                        color: MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#2e353d")
//                        anchors {
//                            left: parent.horizontalCenter
//                            right: parent.right
//                            verticalCenter: parent.verticalCenter
//                        }
                    }
                    Rectangle {
                        id: dateBackgroundLeftRectangle
                        x: 0
                        y: (parent.height-height)/2
                        width: parent.width/2
                        height: dayDelegateItem.circleRadius
                        visible: false
                        color: MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#2e353d")
//                        anchors {
//                            right: parent.horizontalCenter
//                            left: parent.left
//                            verticalCenter: parent.verticalCenter
//                        }
                    }

                    Image {
                        //Displays the colourful background when the date is selected
                        id: dateBackgroundImg
                        visible: false
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        height: dayDelegateItem.circleRadius
                        width: height
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        id: dateText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        opacity: styleData.visibleMonth ? 1 : 0.4
                        text: styleData.date.toLocaleDateString(Qt.locale(), "d")
                        color: MMPTheme.textColor
                        font.weight: Font.Medium
                        textFormat: Text.PlainText
                    }
                }
                dayOfWeekDelegate: Item {
                    implicitHeight: 40
                    Text {
                        id: dayOfWeekText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        textFormat: Text.PlainText
                        color: {
                            if (styleData.dayOfWeek === Locale.Sunday ||styleData.dayOfWeek === Locale.Saturday) {
                                return MMPTheme.cCarminePink
                            } else {
                                MMPTheme.textColor
                            }
                        }
                        text: {
                            switch (styleData.dayOfWeek) {
                            case Locale.Sunday:
                                return Date.fromLocaleDateString(Qt.locale("en_AU"), "Sun", "ddd").toLocaleDateString(Qt.locale(), "ddd")
                            case Locale.Monday:
                                return Date.fromLocaleDateString(Qt.locale("en_AU"), "Mon", "ddd").toLocaleDateString(Qt.locale(), "ddd")
                            case Locale.Tuesday:
                                return Date.fromLocaleDateString(Qt.locale("en_AU"), "Tue", "ddd").toLocaleDateString(Qt.locale(), "ddd")
                            case Locale.Wednesday:
                                return Date.fromLocaleDateString(Qt.locale("en_AU"), "Wed", "ddd").toLocaleDateString(Qt.locale(), "ddd")
                            case Locale.Thursday:
                                return Date.fromLocaleDateString(Qt.locale("en_AU"), "Thu", "ddd").toLocaleDateString(Qt.locale(), "ddd")
                            case Locale.Friday:
                                return Date.fromLocaleDateString(Qt.locale("en_AU"), "Fri", "ddd").toLocaleDateString(Qt.locale(), "ddd")
                            case Locale.Saturday:
                                return Date.fromLocaleDateString(Qt.locale("en_AU"), "Sat", "ddd").toLocaleDateString(Qt.locale(), "ddd")
                            }
                        }
                    }
                }
                navigationBar: Item {
                    implicitHeight: 40
                    MouseArea {
                        id: leftArrowArea
                        height: parent.height
                        width: height
                        onClicked: style.control.showPreviousMonth()
                        Rectangle {
                            id: leftArrowBackground
                            height: 0.5*parent.height
                            width: height
                            radius: height/2
                            anchors.centerIn: parent
                            color: MMPTheme.secondaryBodyColor
                            Image {
                                id: leftArrowImg
                                source: MMPTheme.themeSelect("qrc:/icons/generic/ic_chevron_left_light.svg","qrc:/icons/generic/ic_chevron_left_dark.svg")
                                sourceSize: Qt.size(20, 20)
                                anchors.centerIn: parent
                            }
                        }
                    }
                    Text {
                        id: titleText
                        text: styleData.title
                        color: MMPTheme.lightTextColor
                        textFormat: Text.PlainText
                        //font.pixelSize: 13
                        anchors.centerIn: parent
                    }
                    MouseArea {
                        id: rightArrowArea
                        height: parent.height
                        width: height
                        onClicked: style.control.showNextMonth()
                        anchors.right: parent.right
                        Rectangle {
                            id: rightArrowBackground
                            height: 0.5*parent.height
                            width: height
                            radius: height/2
                            anchors.centerIn: parent
                            color: MMPTheme.secondaryBodyColor
                            Image {
                                id: rightArrowImg
                                source: MMPTheme.themeSelect("qrc:/icons/generic/ic_chevron_right_light.svg","qrc:/icons/generic/ic_chevron_right_dark.svg")
                                sourceSize: Qt.size(20, 20)
                                anchors.centerIn: parent
                            }
                        }
                    }
                }
            }
        }
        Column {
            id: dateRangeButtonColumn
            property int buttonHeight: 24
            property int currentIndex: 8
            spacing: 10
            anchors {
                left: calendar.right
                right: parent.right
                verticalCenter: parent.verticalCenter
                margins: 12
            }

            MouseArea {
                id: todayButton
                property int index: 0
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date()
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=todayButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: todayButtonText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("Today")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: yesterdayButton
                property int index: 1
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(new Date().setDate(new Date().getDate()-1))
                    calendar.selectionEnd = new Date(new Date().setDate(new Date().getDate()-1))
                    popup.currentSelectionType=yesterdayButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: yesterdayButtonText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("Yesterday")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: pastWeekButton
                property int index: 2
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(new Date().setDate(new Date().getDate()-6))
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=pastWeekButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: pastWeekButtonText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("Last 7 Days")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: pastFortnightButton
                property int index: 3
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(new Date().setDate(new Date().getDate()-13))
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=pastFortnightButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: pastFortnightButtonText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("Last 14 Days")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: pastMonthButton
                property int index: 4
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(new Date().setDate(new Date().getDate()-29))
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=pastMonthButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: pastMonthButtonText
                        text: qsTr("Last 30 Days")
                        anchors.centerIn: parent
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: pastTwoMonthsButton
                property int index: 5
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(new Date().setDate(new Date().getDate()-59))
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=pastTwoMonthsButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: pastTwoMonthsButtonText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("Last 60 Days")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: pastYearButton
                property int index: 6
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(new Date().setDate(new Date().getDate()-364))
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=pastYearButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: pastYearButtonText
                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("Last 365 Days")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: pastTwoYearsButton
                property int index: 7
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked:  {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(new Date().setDate(new Date().getDate()-729))
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=pastTwoYearsButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: pastTwoYearsButtonText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("Last 730 Days")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
            MouseArea {
                id: allTimeButton
                property int index: 8
                width: parent.width
                height: dateRangeButtonColumn.buttonHeight
                opacity: dateRangeButtonColumn.currentIndex===index ? 1 : 0.4
                onClicked: {
                    dateRangeButtonColumn.currentIndex = index
                    calendar.selectionStart = new Date(0)
                    calendar.selectionEnd = new Date()
                    popup.currentSelectionType=allTimeButtonText.text
                }
                Rectangle {
//                    anchors.fill: parent
                    height: parent.height
                    width: parent.width
                    radius: height/2
                    color: "transparent"
                    border.color: MMPTheme.textColor
                    Text {
                        id: allTimeButtonText
//                        anchors.centerIn: parent
                        x: (parent.width-width)/2
                        y: (parent.height-height)/2
                        text: qsTr("All Time")
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                    }
                }
            }
        }
    }
}
