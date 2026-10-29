/*
    A view containing the wallet settings
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0

Rectangle{
    id: main
    color: MMPTheme.backgroundColor
    property bool settingsChanged: true
    Rectangle {
        id: settingsTabMenuRect
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
            property Item currentItem: generalSettingsTabButton
            Shortcut {
                sequences: [StandardKey.MoveToNextPage]
                onActivated: {
                    switch(tabMenuRowLayout.currentItem) {
                    case generalSettingsTabButton:
                        tabMenuRowLayout.currentItem = networkSettingsTabButton
                        break
                    case networkSettingsTabButton:
                        tabMenuRowLayout.currentItem = windowSettingsTabButton
                        break
                    case windowSettingsTabButton:
                        tabMenuRowLayout.currentItem = displaySettingsTabButton
                        break
                    case displaySettingsTabButton:
                        tabMenuRowLayout.currentItem = nodesSettingsTabButton
                        break
                    case nodesSettingsTabButton:
                        break
                    }
                }
            }
            Shortcut {
                sequences: [StandardKey.MoveToPreviousPage]
                onActivated: {
                    switch(tabMenuRowLayout.currentItem) {
                    case generalSettingsTabButton:
                        break
                    case networkSettingsTabButton:
                        tabMenuRowLayout.currentItem = generalSettingsTabButton
                        break
                    case windowSettingsTabButton:
                        tabMenuRowLayout.currentItem = networkSettingsTabButton
                        break
                    case displaySettingsTabButton:
                        tabMenuRowLayout.currentItem = windowSettingsTabButton
                        break
                    case nodesSettingsTabButton:
                        tabMenuRowLayout.currentItem = displaySettingsTabButton
                        break
                    }
                }
            }

            MouseArea {
                id: generalSettingsTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==generalSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = generalSettingsTabButton
                Image {
                    id: generalSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_general_light.svg", "/icons/tabs/ic_tab_general_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: generalSettingsLabel
                    text: qsTr("General")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==generalSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: generalSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: networkSettingsTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==networkSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = networkSettingsTabButton
                Image {
                    id: networkSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_network_light.svg", "/icons/tabs/ic_tab_network_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: networkSettingsLabel
                    text: qsTr("Network")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==networkSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: networkSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: windowSettingsTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==windowSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = windowSettingsTabButton
                Image {
                    id: windowSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_window_light.svg", "/icons/tabs/ic_tab_window_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: windowSettingsLabel
                    text: qsTr("Window")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==windowSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: windowSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: displaySettingsTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==displaySettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = displaySettingsTabButton
                Image {
                    id: displaySettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_display_light.svg", "/icons/tabs/ic_tab_display_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: displaySettingsLabel
                    text: qsTr("Display")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==displaySettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: displaySettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }

            }
            MouseArea {
                id: nodesSettingsTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==nodesSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = nodesSettingsTabButton
                Image {
                    id: nodesSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_nodes_light.svg", "/icons/tabs/ic_tab_nodes_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: nodesSettingsLabel
                    text: qsTr("Nodes")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==nodesSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: nodesSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
        MouseArea {
            id: debugConsoleButton
            height: 50
            width: 50
            opacity: pressed ? 1 : 0.4
            onPressed: {
                var component = Qt.createComponent("DebugConsoleWindow.qml")
                var windowObj = component.createObject(window)
                windowObj.show()
            }

            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }

            Image {
                id: debugIcon
                sourceSize: Qt.size(25,25)
                source: MMPTheme.themeSelect("qrc:/icons/tabs/ic_tab_more_light.svg", "qrc:/icons/tabs/ic_tab_more_dark.svg")
                anchors.centerIn: parent
            }
        }
    }
    Rectangle {
        id: body
        color: MMPTheme.bodyColor
        radius: 4
        anchors {
            top: settingsTabMenuRect.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 10
        }
        Loader {
            id: settingsContentLoader
            sourceComponent: {
                switch(tabMenuRowLayout.currentItem) {
                case generalSettingsTabButton:
                    return generalSettingsView
                case networkSettingsTabButton:
                    return networkSettingsView
                case windowSettingsTabButton:
                    return windowSettingsView
                case displaySettingsTabButton:
                    return displaySettingsView
                case nodesSettingsTabButton:
                    return nodesSettingsView
                }
            }
            onSourceComponentChanged: if(item && item.defaultFocusItem) item.defaultFocusItem.forceActiveFocus()
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                bottom: bottomAccentRect.top
            }
        }
        Rectangle {
            id: bottomAccentRect
            height: 40
            color: MMPTheme.secondaryBodyColor
            radius: 4
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            Rectangle {
                id: accentFillRect
                color: MMPTheme.secondaryBodyColor
                width: parent.width
                height: 8
            }
            Button {
                id: saveButton
                text: qsTr("Save")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_save_light.svg", "qrc:/icons/buttons/ic_btn_save_dark.svg")
                enabled: settingsChanged
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: 20
                }
            }
            Button {
                id: discardButton
                text: qsTr("Discard")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_discard_light.svg", "qrc:/icons/buttons/ic_btn_discard_dark.svg")
                enabled: settingsChanged
                anchors {
                    right: saveButton.left
                    verticalCenter: parent.verticalCenter
                    rightMargin: 10
                }
            }
        }
    }

    Component {
        id: generalSettingsView
        ScrollView {
            id: generalSettingsScrollView
            property Item defaultFocusItem: transactionFeeSpinbox
            anchors.fill: parent
            clip: true
            contentWidth: availableWidth
            contentHeight: contentColumn.implicitHeight + contentColumn.anchors.topMargin +
                           sideStakeRect.implicitHeight + sideStakeRect.anchors.topMargin +
                           secondContentGrid.implicitHeight + secondContentGrid.anchors.topMargin + 20
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            GridLayout {
                id: contentColumn
                rowSpacing: 20
                columnSpacing: 10
                columns: 3
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                Text {
                    id: transactionFeeLabel
                    text: qsTr("Pay Transaction Fee:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                SpinBox {
                    id: transactionFeeSpinbox
                    property int decimals: 8
                    property real factor: Math.pow(10, decimals)
                    KeyNavigation.tab: reserveSpinbox
                    Layout.preferredWidth: 120
                    editable: true
                    from: 0
                    to: factor * 10
                    value: factor/100
                    stepSize: factor/1000 //0.01
                    validator: DoubleValidator{
                        bottom: Math.min(transactionFeeSpinbox.from, transactionFeeSpinbox.to)
                        top:  Math.max(transactionFeeSpinbox.from, transactionFeeSpinbox.to)
                        notation: DoubleValidator.StandardNotation
                        decimals: 8
                    }
                    textFromValue: function(value, locale) {
                        return Number(value / factor).toLocaleString(locale, 'f', transactionFeeSpinbox.decimals)
                    }
                    valueFromText: function(text, locale) {
                        return Number.fromLocaleString(locale, text) * factor
                    }
                }
                Item {
                    //This item sets the last column to fill the width
                    Layout.fillWidth: true
                    HelpHover {
                        id: transactionFeeHelp
                        text: qsTr("Optional transaction fee per kB that helps ensure your transactions are processed quickly. A fee of 0.01 GRC/kB is recommmended.")
                        popupWidth: 200
                        horiontalPadding: 10
                        verticalPadding: 10
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    id: reserveLabel
                    text: qsTr("Reserve Amount:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                SpinBox {
                    id: reserveSpinbox
                    property int decimals: 2
                    property real factor: Math.pow(10, decimals)
                    Layout.preferredWidth: 120
                    editable: true
                    from: 0
                    to: 10000000 * factor
                    value: 0
                    stepSize: factor/1000 //0.01
//                    KeyNavigation.tab: sideStakingCheckbox This doesn't work. Idk why, all the others do
                    KeyNavigation.tab: {    //This also doesn't work
                        if (sideStakingCheckbox.checked && sideStakeListView.count>0) return sideStakeListView.itemAtIndex(0).addressTextField
                        return utxoOptimisationCheckbox
                    }
                    validator: DoubleValidator{
                        bottom: Math.min(reserveSpinbox.from, reserveSpinbox.to)
                        top:  Math.max(reserveSpinbox.from, reserveSpinbox.to)
                        notation: DoubleValidator.StandardNotation
                        decimals: 8
                    }
                    textFromValue: function(value, locale) {
                        return Number(value / factor).toLocaleString(locale, 'f', reserveSpinbox.decimals)
                    }
                    valueFromText: function(text, locale) {
                        return Number.fromLocaleString(locale, text) * factor
                    }
                    onValueChanged: main.settingsChanged = true
                }
                HelpHover {
                    id: reserveHelp
                    text: qsTr("Reserve amount does not participate in staking and hence is spendable at any time")
                    popupWidth: 200
                    horiontalPadding: 10
                    verticalPadding: 10
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                }
                CheckBox {
                    id: sideStakingCheckbox
                    text: qsTr("Enable Sidestaking")
                    enabled: true
                    KeyNavigation.tab: {
                        if (checked && sideStakeListView.count>0) return sideStakeListView.itemAtIndex(0).addressTextField
                        return utxoOptimisationCheckbox
                    }
                }
                HelpHover {
                    id: sideStakingHelp
                    text: qsTr("Side staking allocates a proportion of your staking rewards to other addresses")
                    popupWidth: 200
                    horiontalPadding: 10
                    verticalPadding: 10
                    Layout.columnSpan: 2
                }
            }
            Rectangle {
                id: sideStakeRect
                color: "transparent"
                border.color: MMPTheme.lightBorderColor
                radius: 4
                implicitHeight: 25 + 25 + 6*25
                anchors {
                    top: contentColumn.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                TableHeader {
                    id: sideStakeHeader
                    x:1
                    y:1
                    height: 24
                    width: parent.width-2
                    radius: parent.radius
                    borderColor: sideStakeRect.border.color
                    model: [{text: qsTr("Side Stake Address"), width: sideStakeHeader.width*0.75}, {text: qsTr("Percentage"), width: sideStakeHeader.width*0.25}]
                }
                ListView {
                    id: sideStakeListView
                    clip: true
                    currentIndex: 0
                    interactive: false
                    anchors {
                        top: sideStakeHeader.bottom
                        left: parent.left
                        right: parent.right
                        bottom: sideStakeFooter.top
                        leftMargin: 1
                        rightMargin: 1
                    }
                    model: ListModel{
                        id: sideStakeListModel
                        ListElement{address: "SBsdjflids"; percentage: 34}
                        ListElement{address: "SBISDHFIODdsifhdis"; percentage: 21}
                        ListElement{address: "SBISDHFIODdsifhdis"; percentage: 21}
                        ListElement{address: "SBISDHFIODdsifhdis"; percentage: 21}
                        ListElement{address: "SBISDHFIODdsifhdis"; percentage: 21}
                        ListElement{address: "SBISDHFIODdsifhdis"; percentage: 21}
                    }
                    delegate: Rectangle {
                        width: sideStakeListView.width
                        height: 25
                        color: sideStakeListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                        radius: 4
                        property Item addressTextField: addressTextField
                        Row {
                            height: parent.height
                            width: parent.width
                            Item {
                                id: addressItem
                                height: parent.height
                                width: 0.75 * parent.width
                                TextField {
                                    id: addressTextField
                                    text: model.address
                                    placeholderText: qsTr("Enter a Gridcoin Address")
                                    onFocusChanged: {
                                        if (focus) {
                                            sideStakeListView.currentIndex = index
                                            forceActiveFocus()
                                        }
                                    }

                                    onTextEdited: sideStakeListView.model.set(index, {address: text})
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        verticalCenter: parent.verticalCenter
                                    }
                                    background: Item{}
                                    KeyNavigation.tab: percentageTextField
                                }
                            }
                            Item {
                                id: percentageItem
                                height: parent.height
                                width: 0.25 * parent.width
                                TextField {
                                    id: percentageTextField
                                    text: model.percentage
                                    placeholderText: qsTr("Enter a percentage")
                                    onFocusChanged: {
                                        if (focus) {
                                            sideStakeListView.currentIndex = index
                                            forceActiveFocus()
                                        }
                                    }
                                    onTextEdited: {
                                        if (text != "") {
                                            var textInt = parseInt(text)
                                            if (textInt > 100)
                                                sideStakeListView.model.set(index, {percentage: textInt})
                                        }
                                    }
                                    validator: IntValidator {
                                        bottom: 0
                                        top: 100
                                    }
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        verticalCenter: parent.verticalCenter
                                    }
                                    background: Item{}
                                    KeyNavigation.tab: {
                                        if (index === sideStakeListView.count-1) return utxoOptimisationCheckbox
                                        return sideStakeListView.itemAtIndex(index+1).addressTextField
                                    }
                                }
                            }
                        }
                    }
                }

                TableFooter {
                    id: sideStakeFooter
                    borderColor: sideStakeRect.border.color
                    RowLayout {
                        id: buttonRow
                        height: parent.height
                        spacing: 0
                        Button {
                            id: addSSButton
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_add_light.svg","/icons/buttons/ic_btn_add_dark.svg")
                            background: Item{}
                            onClicked: {
                                sideStakeListModel.append({address: "", percentage: 0})
                                sideStakeListView.currentIndex = sideStakeListModel.count-1
                            }
                            enabled: sideStakeListModel.count < 6
                        }
                        Rectangle {
                            id: buttonSeparator
                            color: sideStakeRect.border.color
                            width: 1
                            Layout.fillHeight: true
                        }
                        Button {
                            id: removeSSButton
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_remove_light.svg","/icons/buttons/ic_btn_remove_dark.svg")
                            background: Item{}
                            onClicked: sideStakeListModel.remove(sideStakeListView.currentIndex)
                            enabled: sideStakeListModel.count > 0
                        }
                        Rectangle {
                            id: buttonSeparator2
                            color: sideStakeRect.border.color
                            width: 1
                            Layout.fillHeight: true
                        }
                    }
                }
            }
            GridLayout {
                id: secondContentGrid
                columns: 2
                rowSpacing: 20
                columnSpacing: 10
                anchors {
                    top: sideStakeRect.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                }
                CheckBox {
                    id: utxoOptimisationCheckbox
                    text: qsTr("Stake Splitting")
                    KeyNavigation.tab: targetEfficiencyTextField
                }
                Item {
                    Layout.fillWidth: true
                    HelpHover {
                        id: utxoOptimisationHelpHover
                        text: qsTr("Optimises stakes to minimise cooldown times")
                        popupWidth: 200
                        horiontalPadding: 10
                        verticalPadding: 10
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }


                Text {
                    id: targetEfficiencyLabel
                    text: qsTr("Target Efficiency:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                TextField {
                    id: targetEfficiencyTextField
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.preferredWidth: 60
                    enabled: utxoOptimisationCheckbox.checked
                    KeyNavigation.tab: minSizeTextField
                    validator: IntValidator{
                        bottom: 0
                        top: 99
                    }
                }
                Text {
                    id: minSizeLabel
                    text: qsTr("Minimum UTXO Size:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                TextField {
                    id: minSizeTextField
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.preferredWidth: 60
                    enabled: utxoOptimisationCheckbox.checked
                    KeyNavigation.tab: startAtLoginCheckbox
                    validator: IntValidator{
                        bottom: 0
                    }
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                }
                CheckBox {
                    id: startAtLoginCheckbox
                    text: qsTr("Start at system login")
                    Layout.columnSpan: 2
                    KeyNavigation.tab: disableUpdateCheckBox
                }
                CheckBox {
                    id: disableUpdateCheckBox
                    text: qsTr("Disable update checks")
                    KeyNavigation.tab: transactionFeeSpinbox
                }
                HelpHover {
                    id: updateChecksHelpHover
                    text: qsTr("Increases user interface performance by reducing updates")
                    popupWidth: 200
                    horiontalPadding: 10
                    verticalPadding: 10
                }
            }
        }
    }
    Component {
        id: networkSettingsView
        Item {
            property Item defaultFocusItem: upnpCheckbox
            GridLayout {
                id: contentColumn
                rowSpacing: 20
                columnSpacing: 10
                columns: 2
                width: Math.min(implicitWidth, parent.width-40)
                anchors {
                    top: parent.top
                    left: parent.left
                    topMargin: 20
                    leftMargin: 20
                }
                CheckBox {
                    id: upnpCheckbox
                    text: qsTr("Map port using UPnP")
                    KeyNavigation.tab: socksCheckbox
                }
                Item {
                    Layout.fillWidth: true
                    HelpHover {
                        id: upnpHelpHover
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                CheckBox {
                    id: socksCheckbox
                    text: qsTr("Connect through SOCKS proxy")
                    KeyNavigation.tab: checked ? proxyIPTextField : upnpCheckbox
                }
                HelpHover {
                    id: socksHelpHover
                }
                RowLayout {
                    id: socksRow
                    Layout.columnSpan: 2
                    spacing: 10
                    Text {
                        id: proxyIPLabel
                        text: qsTr("Proxy IP: ")
                        color: MMPTheme.textColor
                    }
                    TextField {
                        id: proxyIPTextField
                        Layout.preferredWidth: 140
                        placeholderText: "255.255.255.255"
                        enabled: socksCheckbox.checked
                    }
                    Text {
                        id: portLabel
                        text: qsTr("Port: ")
                        color: MMPTheme.textColor
                    }
                    TextField {
                        id: portTextField
                        placeholderText: "9050"
                        Layout.preferredWidth: 60
                        enabled: socksCheckbox.checked
                        KeyNavigation.tab: socksVersionComboBox
                    }
                    Text {
                        id: socksVersionLabel
                        text: qsTr("SOCKS version: ")
                        color: MMPTheme.textColor
                    }
                    ComboBox {
                        id: socksVersionComboBox
                        Layout.preferredWidth: 40
                        model: [5, 4]
                        enabled: socksCheckbox.checked
                        KeyNavigation.tab: upnpCheckbox
                    }
                }
            }
        }
    }
    Component {
        id: windowSettingsView
        Item {
            property Item defaultFocusItem: minimiseToTrayCheckbox
            ColumnLayout {
                spacing: 20
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                CheckBox {
                    id: minimiseToTrayCheckbox
                    text: qsTr("Minimise to tray instead of the taskbar")
                    KeyNavigation.tab: minimiseOnCloseCheckbox
                }
                CheckBox {
                    id: minimiseOnCloseCheckbox
                    text: qsTr("Minimise on close")
                    KeyNavigation.tab: confirmOnCloseCheckbox
                }
                CheckBox {
                    id: confirmOnCloseCheckbox
                    text: qsTr("Confirm on close")
                    KeyNavigation.tab: disableTransactionNotificationCheckbox
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                }
                CheckBox {
                    id: disableTransactionNotificationCheckbox
                    text: qsTr("Disable transaction notifications")
                    KeyNavigation.tab: disablePollNotificationCheckbox
                }
                CheckBox {
                    id: disablePollNotificationCheckbox
                    text: qsTr("Disable poll notifications")
                    KeyNavigation.tab: minimiseToTrayCheckbox
                }
            }
        }
    }
    Component {
        id: displaySettingsView
        Item {
            property Item defaultFocusItem: languageCombobox
            GridLayout {
                id: contentGridView
                columns: 2
                rowSpacing: 20
                columnSpacing: 10
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                Text {
                    id: languageLabel
                    text: qsTr("User interface language:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                ComboBox {
                    id: languageCombobox
                    model: [qsTr("System Default"),"American English", "British English", "Australian English", "Francais"]
                    KeyNavigation.tab: themeCombobox
                }
                Text {
                    id: themeLabel
                    text: qsTr("Theme:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                ComboBox {
                    id: themeCombobox
                    currentIndex: MMPTheme.theme
                    model: [qsTr("Light"), qsTr("Dark")]
                    onCurrentIndexChanged: MMPTheme.theme = currentIndex
                    KeyNavigation.tab: addressesInTransactionsCheckbox
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                }
                //                CheckBox {
                //                    id: displayTransactionsAfter
                //                    text: qsTr("Only display transactions from after date:")
                //                }
                //TODO Adapt DatePicker to select a single date instead of a range
                //                Button {
                //                    id: showDateSelectorButton
                //                    text: qsTr("Select Date")
                //                    icon.source: MMPTheme.themeSelect("qrc:/icons/generic/ic_date_light.svg","qrc:/icons/generic/ic_date_dark.svg")
                //                }
                CheckBox {
                    id: addressesInTransactionsCheckbox
                    text: qsTr("Display addresses in transactions list")
                    Layout.columnSpan: 2
                    KeyNavigation.tab: coinControlCheckbox
                }
                CheckBox {
                    id: coinControlCheckbox
                    text: qsTr("Display advanced coin control features")
                    Layout.columnSpan: 2
                    KeyNavigation.tab: languageCombobox
                }
                //Set the second column to fill the width
                Item {
                    Layout.preferredHeight: 1
                }
                Item {
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                }
            }
        }
    }
    Component {
        id: nodesSettingsView
        Item {

            Text {
                id: addNodeInfoLabel
                text: qsTr("Add nodes are nodes the wallet connects to on startup. It is not usually necessary to change these")
                wrapMode: Text.WordWrap
                color: MMPTheme.textColor
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    topMargin: 20
                    leftMargin: 10
                    rightMargin: 10
                }
            }
            Rectangle {
                id: sideStakeRect
                color: "transparent"
                border.color: MMPTheme.lightBorderColor
                radius: 4
                anchors {
                    left: parent.left
                    right: parent.right
                    top: addNodeInfoLabel.bottom
                    bottom: parent.bottom
                    margins: 10
                }
                ListView {
                    id: addNodeListView
                    currentIndex: 0
                    clip: true
                    anchors {
                        top: parent.top
                        right: parent.right
                        left: parent.left
                        bottom: addNodeFooter.top
                        margins: 1
                        bottomMargin: 0
                    }
                    model: ListModel {
                        id: addNodeListModel
                        ListElement{address: "www.google.com"}
                    }
                    ScrollIndicator.vertical: ScrollIndicator {
                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            right: parent.right
                            rightMargin: 1
                        }
                    }
                    delegate: Rectangle {
                        width: addNodeListView.width
                        height: 25
                        color: addNodeListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                        radius: 4
                        property Item addressTextField: addressTextField
                        TextField {
                            id: addressTextField
                            anchors.fill: parent
                            background: Item{}
                            text: model.address
                            placeholderText: qsTr("Enter Node Address")
                            onTextEdited: addNodeListView.model.set(index, {address: text})
                            onFocusChanged: {
                                if (focus) {
                                    addNodeListView.currentIndex = index
                                    forceActiveFocus()
                                }
                            }
                        }
                    }
                }
                TableFooter{
                    id: addNodeFooter
                    RowLayout {
                        id: buttonRow
                        height: parent.height
                        spacing: 0
                        Button {
                            id: addNodeButton
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_add_light.svg","/icons/buttons/ic_btn_add_dark.svg")
                            background: Item{}
                            onClicked: {
                                addNodeListModel.append({address: ""})
                                addNodeListView.currentIndex = addNodeListModel.count-1
                            }
                        }
                        Rectangle {
                            id: buttonSeparator
                            color: MMPTheme.lightBorderColor
                            width: 1
                            Layout.fillHeight: true
                        }
                        Button {
                            id: removeNodeButton
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_remove_light.svg","/icons/buttons/ic_btn_remove_dark.svg")
                            background: Item{}
                            onClicked: addNodeListModel.remove(addNodeListView.currentIndex)
                            enabled: addNodeListModel.count > 0
                        }
                        Rectangle {
                            id: buttonSeparator2
                            color: MMPTheme.lightBorderColor
                            width: 1
                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }
    }
}
