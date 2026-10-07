/*
    View in wallet showing favourite addresses
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0

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
            text: qsTr("Favourites")
            font.weight: Font.DemiBold
            font.pixelSize: 22
            color: MMPTheme.textColor
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 22
            }
        }
        Rectangle {
            id: recordSeparator
            color: MMPTheme.separatorColor
            height: titleText.height*0.625
            width: 1
            anchors {
                bottom: titleText.baseline
                bottomMargin: -1
                left: titleText.right
                leftMargin: 8
            }
        }

        property string recordString: qsTr("%n record(s)", "", addressListView.count)
        Text {
            id: recordNumText
            text: parent.recordString.slice(0,1)
            color: MMPTheme.highlightColor
            font.weight: Font.DemiBold
            anchors {
                baseline: titleText.baseline
                baselineOffset: -2
                left: recordSeparator.right
                leftMargin: 8
            }
        }
        Text {
            id: recordLabelText
            height: recordSeparator.height
            text:  parent.recordString.slice(1)
            color: MMPTheme.textColor
            anchors {
                baseline: recordNumText.baseline
                left: recordNumText.right
            }
        }

        SearchBox {
            placeholderText: qsTr("Search by label or address")
//            onTextChanged: listview.sortModel(text)
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 20
            }
        }
    }
    Rectangle {
        id: favoritesPanel
        color: MMPTheme.bodyColor
        radius: 4
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 10
        }
        Rectangle {
            id: addressListRect
            property int labelColumnWidth: width*0.5
            property int addressColumnWidth: width*0.5
            color: MMPTheme.themeSelect(MMPTheme.cWhite, "#17222c")
            border.color: MMPTheme.themeSelect("#c3c7ce", "#3b475d")
            radius: 4
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                bottom: bottomAccentItem.top
                margins: 10
            }

            TableHeader {
                id: tableHeaderRect
                height: 25
                radius: parent.radius
                model: [
                    { text: qsTr("Label"), width: addressListRect.labelColumnWidth },
                    { text: qsTr("Address"), width: addressListRect.addressColumnWidth }
                ]
            }
            ListView {
                id: addressListView
                clip: true
                currentIndex: 0
                ScrollIndicator.vertical: ScrollIndicator {
                    parent: addressListView.parent
                    anchors {
                        top: addressListView.top
                        bottom: addressListView.bottom
                        right: addressListView.right
                        rightMargin: 1
                    }
                }
                anchors {
                    top: tableHeaderRect.bottom
                    left: parent.left
                    right: parent.right
                    bottom: tableFooterRect.top
                    leftMargin: 1
                    rightMargin: 1
                }

                model: ListModel {
                    id: addressListModel
                    ListElement {
                        label: "Donation"
                        address: "SBPvphumk9BmzdLqCBy4b7U62tj39iynLo"
                    }
                    ListElement {
                        label: "Inbox"
                        address: "SGMMMMMMMMMMMMMMMMMMMMMMMMMMMMfqtz"
                    }
                }
                delegate: MouseArea {
                    width: parent.width
                    height: 25
                    onClicked: addressListView.currentIndex=index
                    Rectangle {
                        anchors.fill: parent
                        color: addressListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                        Item {
                            id: labelTextItem
                            height: parent.height
                            width: addressListRect.labelColumnWidth
                            Text {
                                id: labelText
                                height: parent.height
                                text: label
                                color: MMPTheme.translucent(MMPTheme.textColor, addressListView.currentIndex===index ? 1 : 0.7)
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                anchors {
                                    left: parent.left
                                    leftMargin: 10
                                }
                            }
                        }
                        Text {
                            id: addressText
                            height: parent.height
                            text: address
                            color: MMPTheme.translucent(MMPTheme.textColor, addressListView.currentIndex===index ? 1 : 0.7)
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            anchors {
                                left: labelTextItem.right
                                leftMargin: 10
                                right: copyButton.visible ? copyButton.left : parent.right
                                rightMargin: 1
                            }
                        }
                        Button {
                            id: copyButton
                            visible: addressListView.currentIndex===index
                            icon.source: MMPTheme.themeSelect("qrc:/icons/generic/ic_copy_light.svg","qrc:/icons/generic/ic_copy_dark.svg")
                            anchors {
                                top: parent.top
                                bottom: parent.bottom
                                right: parent.right
                                rightMargin: 2
                            }
                            background: Item {}
                        }
                    }
                }
            }
            TableFooter {
                id: tableFooterRect
                borderColor: MMPTheme.lightBorderColor
                RowLayout {
                    id: buttonRow
                    y: 1
                    height: parent.height-1
                    spacing: 0

                    Button {
                        id: addAddressButton
                        Layout.fillHeight: true
                        implicitWidth: 30
                        icon.source: MMPTheme.themeSelect("qrc:/icons/generic/ic_add_light.svg","qrc:/icons/generic/ic_add_dark.svg")
                        background: Item{}
                    }
                    Rectangle {
                        id: buttonSeparator
                        color: addressListRect.border.color
                        Layout.fillHeight: true
                        width: 1
                    }
                    Button {
                        id: removeAddressButton
                        Layout.fillHeight: true
                        implicitWidth: 30
                        icon.source: MMPTheme.themeSelect("qrc:/icons/generic/ic_remove_light.svg","qrc:/icons/generic/ic_remove_dark.svg")
                        background: Item{}
                    }
                    Rectangle {
                        color: addressListRect.border.color
                        width: 1
                        Layout.fillHeight: true
                    }
                }
            }
        }
        Item {
            id: bottomAccentItem
            height: 40
            width: parent.width
            anchors.bottom: parent.bottom
            //Background
            Rectangle {
                id: bottomCurvedRect
                color: MMPTheme.secondaryBodyColor
                width: parent.width
                height: 2*radius
                radius: favoritesPanel.radius
                anchors.bottom: parent.bottom
            }
            Rectangle {
                id: bottomAccentRect
                width: parent.width
                height: 40
                color: MMPTheme.secondaryBodyColor
                anchors {
                    top: parent.top
                    bottom: bottomCurvedRect.verticalCenter
                }
            }

            Button {
                text: qsTr("Show QR Code")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_qr_code_light.svg","qrc:/icons/buttons/ic_btn_qr_code_dark.svg")
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                    leftMargin: 10
                }
                onPressed: {
                    var component = Qt.createComponent("QRCodeWindow.qml")
                    var windowObj = component.createObject(window)
                    windowObj.address = addressListView.model.get(addressListView.currentIndex).address
                    windowObj.show()
                }
            }
            Button {
                id: verifyMessageButton
                text: qsTr("Verify Message")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_verify_light.svg","qrc:/icons/buttons/ic_btn_verify_dark.svg")
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                    rightMargin: 10
                }
            }
            Button {
                text: qsTr("Sign Message")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_sign_light.svg","qrc:/icons/buttons/ic_btn_sign_dark.svg")
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: verifyMessageButton.left
                    rightMargin: 10
                }
            }
        }
    }
}
