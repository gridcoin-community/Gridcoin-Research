/*
    View in the wallet showing transaction history
*/
import QtQuick
import QtQuick.Controls
import MMPTheme 1.0
import CurrentTime 1.0

Rectangle {
    id: main
    color: MMPTheme.backgroundColor
    Rectangle {
        id: header
        color: MMPTheme.headerColor
        height: 70
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        Rectangle {
            id: bottomBorder
            height: 1
            color: MMPTheme.themeSelect("transparent", MMPTheme.cBlack)
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
        }

        Text {
            id: titleText
            text: qsTr("Transaction History")
            font.weight: Font.DemiBold
            font.pixelSize: 22
            color: MMPTheme.textColor
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 22
            }
        }
        SearchBox {
            placeholderText: qsTr("Search by address")
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 20
            }
        }
    }
    Rectangle {
        id: filterRect
        color: MMPTheme.secondaryBodyColor
        height: 40
        width: parent.width
        anchors.top: header.bottom
        MouseArea {
            id: dateSelectionBox
            width: 170
            height: 24
            onPressed: {
                // datePicker.item.open()
                forceActiveFocus(Qt.MouseFocusReason)
            }
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
                leftMargin: 20
            }
            Rectangle {
                //id: dateBackground
                border.width: 1
                radius: 4
                color: MMPTheme.themeSelect(MMPTheme.cWhite, MMPTheme.cOxfordOffBlue)
                border.color: MMPTheme.themeSelect(MMPTheme.translucent(MMPTheme.cOxfordBlue, dateSelectionBox.activeFocus ? 0.7 : 0.3),
                                                   dateSelectionBox.activeFocus ? MMPTheme.translucent(MMPTheme.cWhite, 0.7) : MMPTheme.cOxfordBlue)
                anchors.fill: parent
                Image {
                    id: calendarIcon
                    source: MMPTheme.themeSelect("qrc:/icons/generic/ic_date_light.svg","qrc:/icons/generic/ic_date_dark.svg")
                    sourceSize: Qt.size(10, 10)
                    opacity: dateSelectionBox.activeFocus ? 1 : 0.7
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        leftMargin: 10
                    }
                }
                Image {
                    id: dateBoxDownChevron
                    source: MMPTheme.themeSelect("qrc:/icons/generic/ic_chevron_down_light.svg", "qrc:/icons/generic/ic_chevron_down_dark.svg")
                    opacity: dateSelectionBox.activeFocus ? 1 : 0.7
                    sourceSize: Qt.size(15, 15)
                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: 5
                    }
                }
                Text {
                    id: dateBoxDisplayText
                    // text: datePicker.status===Loader.Ready ? datePicker.item.currentSelectionType : qsTr("All Time")
                    verticalAlignment: Text.AlignVCenter
                    color: MMPTheme.textColor
                    opacity: dateSelectionBox.activeFocus ? 1 : 0.7
                    clip: true
                    anchors {
                        left: calendarIcon.right
                        right: dateBoxDownChevron.left
                        top: parent.top
                        bottom: parent.bottom
                        leftMargin: 6
                        rightMargin: 5
                    }
                }
            }
            // Loader {
            //     // Load DatePicker asynchronously
            //     id: datePicker
            //     x: -width/2 + dateSelectionBox.width*0.9
            //     y: dateSelectionBox.height + 20
            //     width: 420
            //     height: 330
            //     visible: false
            //     source: "qrc:/qml/DatePicker.qml"
            //     asynchronous: true
            // }
        }
        ComboBox {
            id: typeComboBox
            width: 170
            height: 24
            currentIndex: 0
            focus: false
            anchors {
                left: dateSelectionBox.right
                verticalCenter: parent.verticalCenter
                leftMargin: 10
            }
            model: ListModel {
                ListElement { text: qsTr("All Types") }
                ListElement { text: qsTr("Incoming") }
                ListElement { text: qsTr("Outgoing") }
                ListElement { text: qsTr("Block Reward") }
                ListElement { text: qsTr("Research") }
            }
        }
        TextField {
            id: minAmount
            width: 140
            placeholderText: qsTr("Min. Amount")
            validator: DoubleValidator { bottom: 0 }
            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
                rightMargin: 10
            }
        }
    }

    Rectangle {
        id: historyPanel
        color: MMPTheme.bodyColor
        radius: 4
        anchors {
            top: filterRect.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 10
        }
        Rectangle {
            id: transactionBackgroundRect
            property int dateColumnWidth: Math.max(140, width*0.2)
            property int typeColumnWidth: Math.max(100, width*0.2)
            property int addressColumnWidth: width-dateColumnWidth-typeColumnWidth-amountColumnWidth
            property int amountColumnWidth: Math.max(140, width*0.2)
            color: MMPTheme.themeSelect(MMPTheme.cWhite, "#17222c")
            border.color: MMPTheme.themeSelect("#c3c7ce", "#3b475d")
            radius: 4
            anchors {
                fill: parent
                margins: 10
            }
            TableHeader {
                id: tableHeaderRect
                height: 25
                radius: parent.radius
                model: [
                    { text: qsTr("Date"), width: transactionBackgroundRect.dateColumnWidth },
                    { text: qsTr("Type"), width: transactionBackgroundRect.typeColumnWidth },
                    { text: qsTr("Address"), width: transactionBackgroundRect.addressColumnWidth },
                    { text: qsTr("Amount"), width: transactionBackgroundRect.amountColumnWidth }
                ]
            }
            ListView {
                id: transactionListView
                clip: true
                currentIndex: 0
                ScrollIndicator.vertical: ScrollIndicator {
                    parent: transactionListView.parent
                    anchors {
                        top: transactionListView.top
                        bottom: transactionListView.bottom
                        right: transactionListView.right
                        rightMargin: 1
                    }
                }
                anchors {
                    top: tableHeaderRect.bottom
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    leftMargin: 1
                    rightMargin: 1
                }
                model: ListModel {
                    ListElement {
                        date: 1612151516
                        type: "Incoming"
                        address: "Primary Wallet"
                        amount: 32432.3218
                    }
                    ListElement {
                        date: 22039382
                        type: "Outgoing"
                        address: "Primary Wallet"
                        amount: -323.29183
                    }
                }
                delegate: MouseArea {
                    width: parent.width
                    height: 25
                    onClicked: transactionListView.currentIndex=index
                    Rectangle {
                        anchors.fill: parent
                        color: transactionListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                        Item {
                            id: dateItem
                            height: parent.height
                            width: transactionBackgroundRect.dateColumnWidth
                            Image {
                                id: transactionStatusIcon
                                sourceSize: Qt.size(15, 15)
                                anchors {
                                    verticalCenter: parent.verticalCenter
                                    left: parent.left
                                    leftMargin: 10
                                }
                                source: {
                                    var dateDiff = (CurrentTime.currentTime.getTime() - new Date(date*1000).getTime())/1000   //Age of transaction in seconds
                                    //Arbitrary time diffs
                                    if (dateDiff< 90) {
                                        return "qrc:/icons/transactionlevels/ic_tran_lv1.svg"
                                    } else if (dateDiff < 180){
                                        return "qrc:/icons/transactionlevels/ic_tran_lv2.svg"
                                    } else if (dateDiff < 270){
                                        return "qrc:/icons/transactionlevels/ic_tran_lv3.svg"
                                    } else if (dateDiff < 360){
                                        return "qrc:/icons/transactionlevels/ic_tran_lv4.svg"
                                    } else if (dateDiff < 540){
                                        return "qrc:/icons/transactionlevels/ic_tran_lv5.svg"
                                    } else {
                                       return "qrc:/icons/transactionlevels/ic_tran_lv6.svg"
                                    }
                                }
                            }
                            Text {
                                id: dateText
                                text: new Date(date*1000).toLocaleString(Qt.locale(), Locale.ShortFormat)
                                elide: Text.ElideRight
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                color: MMPTheme.translucent(MMPTheme.textColor, transactionListView.currentIndex===index ? 1 : 0.7)
                                anchors {
                                    left: transactionStatusIcon.right
                                    leftMargin: 5
                                    right: parent.right
                                }
                            }
                        }
                        Item {
                            id: typeItem
                            height: parent.height
                            width: transactionBackgroundRect.typeColumnWidth
                            anchors.left: dateItem.right
                            Text {
                                id: typeText
                                text: type
                                elide: Text.ElideRight
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                color: MMPTheme.textColor
                                anchors {
                                    left: parent.left
                                    leftMargin: 10
                                    right: typeIcon.left
                                }
                            }
                            Image {
                                id: typeIcon
                                sourceSize: Qt.size(15,15)
                                anchors {
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                }

                                source: {
                                    switch (type) {
                                    case ("Incoming"):
                                        return "qrc:/icons/events/ic_event_green.svg"
                                    case ("Outgoing"):
                                        return "qrc:/icons/events/ic_event_red.svg"
                                    case ("BlockReward"):
                                        return "qrc:/icons/events/ic_event_yellow.svg"
                                    case ("Research"):
                                        return "qrc:/icons/events/ic_event_purple.svg"
                                    case ("ResearchSideStake"):
                                        return "qrc:/icons/events/ic_event_green_purple.svg"
                                    case ("InterestSideStake"):
                                        return "qrc:/icons/events/ic_event_yellow_purple.svg"
                                    }
                                }
                            }
                        }
                        Item {
                            id: addressItem
                            height: parent.height
                            width: transactionBackgroundRect.addressColumnWidth
                            anchors.left: typeItem.right
                            Text {
                                id: addressText
                                text: address
                                elide: Text.ElideRight
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                color: MMPTheme.translucent(MMPTheme.textColor, transactionListView.currentIndex===index ? 1 : 0.7)
                                anchors {
                                    left: parent.left
                                    leftMargin: 10
                                    right: parent.right
                                }
                            }
                        }
                        Item {
                            id: amountItem
                            height: parent.height
                            width: transactionBackgroundRect.amountColumnWidth
                            anchors.left: addressItem.right
                            Text {
                                id: amountText
                                text: {
                                    var output = ""
                                    if (amount>=0) {
                                        output="+"
                                    }
                                    output += amount
                                    return output
                                }
                                elide: Text.ElideRight
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                color: MMPTheme.translucent(MMPTheme.textColor, transactionListView.currentIndex===index ? 1 : 0.7)
                                anchors {
                                    left: parent.left
                                    leftMargin: 10
                                    right: parent.right
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
