/*
    Window providing debug information. May need to be converted to a widget to embed widgets such as network traffic
*/
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0

Window {
    id: window
    title: qsTr("Debug Console")
    width: 700
    height: 600
    minimumWidth: 500
    minimumHeight: 450
    flags: Qt.Dialog
    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: window.close()
    }
    Rectangle {
        id: debugTabMenuRect
        color: MMPTheme.headerColor
        height: 70
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        Rectangle {
            id: headerBottomBorderRect
            color: MMPTheme.themeSelect("transparent", MMPTheme.cBlack)
            height: 1
            width: parent.width
            anchors.bottom: parent.bottom
        }
        RowLayout {
            id: tabMenuRowLayout
            anchors.centerIn: parent
            spacing: 30
            property Item currentItem: informationTabButton
            Shortcut {
                sequences: [StandardKey.MoveToNextPage]
                onActivated: {
                    switch(tabMenuRowLayout.currentItem) {
                    case informationTabButton:
                        tabMenuRowLayout.currentItem = networkTrafficTabButton
                        break
                    case networkTrafficTabButton:
                        tabMenuRowLayout.currentItem = consoleTabButton
                        break
                    case consoleTabButton:
                        tabMenuRowLayout.currentItem = scraperTabButton
                        break
                    case scraperTabButton:
                        tabMenuRowLayout.currentItem = peersTabButton
                        break
                    case peersTabButton:
                        break
                    }
                }
            }
            Shortcut {
                sequences: [StandardKey.MoveToPreviousPage]
                onActivated: {
                    switch(tabMenuRowLayout.currentItem) {
                    case informationTabButton:
                        break
                    case networkTrafficTabButton:
                        tabMenuRowLayout.currentItem = informationTabButton
                        break
                    case consoleTabButton:
                        tabMenuRowLayout.currentItem = networkTrafficTabButton
                        break
                    case scraperTabButton:
                        tabMenuRowLayout.currentItem = consoleTabButton
                        break
                    case peersTabButton:
                        tabMenuRowLayout.currentItem = scraperTabButton
                        break
                    }
                }
            }

            MouseArea {
                id: informationTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 80
                opacity: tabMenuRowLayout.currentItem==informationTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = informationTabButton
                Image {
                    id: informationIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_info_light.svg", "/icons/tabs/ic_tab_info_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: informationLabel
                    text: qsTr("Information")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==informationTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: informationIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: networkTrafficTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 80
                opacity: tabMenuRowLayout.currentItem==networkTrafficTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = networkTrafficTabButton
                Image {
                    id: networkTrafficIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("qrc:/icons/tabs/ic_tab_net_traffic_light.svg", "qrc:/icons/tabs/ic_tab_net_traffic_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: networkTrafficLabel
                    text: qsTr("Network Traffic")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==networkTrafficTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: networkTrafficIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: consoleTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 80
                opacity: tabMenuRowLayout.currentItem==consoleTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = consoleTabButton
                Image {
                    id: consoleIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_console_light.svg", "/icons/tabs/ic_tab_console_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: consoleLabel
                    text: qsTr("Console")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==consoleTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: consoleIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: scraperTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==scraperTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = scraperTabButton
                Image {
                    id: scraperIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("qrc:/icons/tabs/ic_tab_cmd_line_light.svg", "qrc:/icons/tabs/ic_tab_cmd_line_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: scraperLabel
                    text: qsTr("Scraper")
                    color: MMPTheme.textColor
                    font.weight: tabMenuRowLayout.currentItem==scraperTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: scraperIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }

            }
            MouseArea {
                id: peersTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 80
                opacity: tabMenuRowLayout.currentItem==peersTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = peersTabButton
                Image {
                    id: peersIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_nodes_light.svg", "/icons/tabs/ic_tab_nodes_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: nodesSettingsLabel
                    text: qsTr("Peers")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==peersTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: peersIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }
    Rectangle {
        id: background
        color: MMPTheme.backgroundColor
        anchors {
            top: debugTabMenuRect.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        Loader {
            id: debugContentLoader
            anchors.fill: parent
            sourceComponent: {
                switch (tabMenuRowLayout.currentItem) {
                case informationTabButton:
                    return informationView
                case networkTrafficTabButton:
                    return networkTrafficView
                case consoleTabButton:
                    return consoleView
                case scraperTabButton:
                    return scraperView
                case peersTabButton:
                    return peersView
                }
            }
        }
    }
    Component {
        id: informationView
        Item {
            Rectangle {
                id: corePanel
                color: MMPTheme.bodyColor
                radius: 4
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.horizontalCenter
                    bottom: debugLogButton.top
                    margins: 10
                    rightMargin: 5
                }
                PanelTitle {
                    id: corePanelTitle
                    text: qsTr("Gridcoin Core")
                    anchors {
                        left: parent.left
                        top: parent.top
                        topMargin: 10
                        leftMargin: 20
                    }
                }
                Rectangle {
                    id: corePanelTitleSeparator
                    height: 1
                    color: MMPTheme.separatorColor
                    anchors {
                        top: corePanelTitle.bottom
                        left: parent.left
                        right: parent.right
                        margins: 20
                        topMargin: 5
                    }
                }
                GridLayout {
                    id: corePanelGridLayout
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 20
                    clip: true
                    anchors {
                        top: corePanelTitleSeparator.bottom
                        left: parent.left
                        right: parent.right
                        margins: 20
                        topMargin: 10
                    }
                    Text {
                        text: qsTr("Client Name:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "Halford"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Client Version:")
                        color: MMPTheme.textColor
                    }
                    Text{
                        text: "v5.3.2.0-unk-3"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("OpenSSL version:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "OpenSSL 1.1.1k  25 Mar 2021"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Boost Version:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "1.76.0"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Qt Version:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "5.15.2"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Startup Time:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: new Date().toLocaleString(Qt.locale(), Locale.ShortFormat)
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Difficulty:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "18.27"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
            Rectangle {
                id: networkPanel
                color: MMPTheme.bodyColor
                radius: 4
                anchors {
                    top: parent.top
                    left: parent.horizontalCenter
                    right: parent.right
                    bottom: debugLogButton.top
                    margins: 10
                    leftMargin: 5
                }
                PanelTitle {
                    id: networkPanelTitle
                    text: qsTr("Network")
                    anchors {
                        left: parent.left
                        top: parent.top
                        topMargin: 10
                        leftMargin: 20
                    }
                }
                Rectangle {
                    id: networkPanelTitleSeparator
                    height: 1
                    color: MMPTheme.separatorColor
                    anchors {
                        top: networkPanelTitle.bottom
                        left: parent.left
                        right: parent.right
                        margins: 20
                        topMargin: 5
                    }
                }
                GridLayout {
                    id: networkPanelGridLayout
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 20
                    clip: true
                    anchors {
                        top: networkPanelTitleSeparator.bottom
                        left: parent.left
                        right: parent.right
                        margins: 20
                        topMargin: 10
                    }
                    Text {
                        text: qsTr("Number of Connections:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "12"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("On Testnet:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: qsTr("No")
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Current Number of Blocks:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "12439728"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Total Estimated Blocks:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: "12000000"
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Last Block Time:")
                        color: MMPTheme.textColor
                    }
                    Text {
                        text: new Date().toLocaleString(Qt.locale(), Locale.ShortFormat)
                        color: MMPTheme.lightTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
            Button {
                id: debugLogButton
                text: qsTr("Event Log")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_log_light.svg", "qrc:/icons/buttons/ic_btn_log_dark.svg")
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    margins: 10
                }
            }
        }
    }
    Component {
        id: networkTrafficView
        Rectangle {
            id: networkTrafficItem
            color: MMPTheme.cDullLime
            radius: 4
            anchors {
                fill: parent
                margins: 10
            }
        }
    }
    Component {
        id: consoleView
        Rectangle {
            id: consoleItem
            color: MMPTheme.cCarminePink
            radius: 4
            anchors {
                fill: parent
                margins: 10
            }
        }
    }
    Component {
        id: scraperView
        Rectangle {
            id: scraperItem
            color: MMPTheme.cBluePurple
            radius: 4
            anchors {
                fill: parent
                margins: 10
            }
        }
    }
    Component {
        id: peersView
        Rectangle {
            id: body
            color: MMPTheme.bodyColor
            radius: 4
            anchors {
                fill: parent
                margins: 10
            }
            Rectangle {
                id: tableRect
                color: "transparent"
                border.color: MMPTheme.lightBorderColor
                radius: 4
                anchors {
                    fill: parent
                    margins: 10
                }
                TableHeader {
                    id: tableHeader
                    property real nodeIDwidth: 0.10
                    property real nodeServiceWidth: 0.25
                    property real pingWidth: 0.10
                    property real sentWidth: 0.15
                    property real receivedWidth: 0.15
                    property real userAgentWidth: 0.25
                    model: [{text: qsTr("Node ID"), width: tableHeader.width*nodeIDwidth},
                        {text: qsTr("Node/Service"), width: tableHeader.width*nodeServiceWidth},
                        {text: qsTr("Ping"), width: tableHeader.width*pingWidth},
                        {text: qsTr("Sent"), width: tableHeader.width*sentWidth},
                        {text: qsTr("Received"), width: tableHeader.width*receivedWidth},
                        {text: qsTr("User Agent"), width: tableHeader.width*userAgentWidth}]
                }
                ListView {
                    id: peersListView
                    clip: true
                    currentIndex: 0
                    anchors {
                        top: tableHeader.bottom
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        leftMargin: 1
                        rightMargin: 1
                    }
                    ScrollIndicator.vertical: ScrollIndicator {
                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            right: parent.right
                            rightMargin: 1
                        }
                    }
                    model: ListModel {
                        id: peersListModel
                        ListElement{
                            nodeID: 123
                            nodeService: "128.123.523.123:12365"
                            ping: "123ms"
                            sent: "123 MB"
                            received: "123 MB"
                            userAgent: "Halford:5.3.2"
                        }
                        ListElement{
                            nodeID: 123
                            nodeService: "128.123.523.123:12365"
                            ping: "123ms"
                            sent: "123 MB"
                            received: "123 MB"
                            userAgent: "Halford:5.3.2"
                        }
                        ListElement{
                            nodeID: 123
                            nodeService: "128.123.523.123:12365"
                            ping: "123ms"
                            sent: "123 MB"
                            received: "123 MB"
                            userAgent: "Halford:5.3.2"
                        }
                    }
                    delegate: Rectangle {
                        id: delegateRect
                        width: peersListView.width
                        height: 25
                        color: index%2===1 ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                        radius: 4
                        Row {
                            id: dataRow
                            height: parent.height
                            Item {
                                id: nodeIDItem
                                width: tableHeader.nodeIDwidth*delegateRect.width
                                height: parent.height
                                Text {
                                    id: nodeIDText
                                    text: model.nodeID
                                    color: MMPTheme.lightTextColor
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                }
                            }
                            Item {
                                id: nodeServiceItem
                                width: tableHeader.nodeServiceWidth*delegateRect.width
                                height: parent.height
                                Text {
                                    id: nodeServiceText
                                    text: model.nodeService
                                    color: MMPTheme.lightTextColor
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                }
                            }
                            Item {
                                id: pingItem
                                width: tableHeader.pingWidth*delegateRect.width
                                height: parent.height
                                Text {
                                    id: pingText
                                    text: model.ping
                                    color: MMPTheme.lightTextColor
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                }
                            }
                            Item {
                                id: sentItem
                                width: tableHeader.sentWidth*delegateRect.width
                                height: parent.height
                                Text {
                                    id: sentText
                                    text: model.sent
                                    color: MMPTheme.lightTextColor
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                }
                            }
                            Item {
                                id: receivedItem
                                width: tableHeader.receivedWidth*delegateRect.width
                                height: parent.height
                                Text {
                                    id: receivedText
                                    text: model.received
                                    color: MMPTheme.lightTextColor
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                }
                            }
                            Item {
                                id: userAgentItem
                                width: tableHeader.userAgentWidth*delegateRect.width
                                height: parent.height
                                Text {
                                    id: userAgentText
                                    text: model.userAgent
                                    color: MMPTheme.lightTextColor
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
