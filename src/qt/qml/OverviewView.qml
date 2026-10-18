/*
    Overview view. The front page of the wallet
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import MMPTheme 1.0
import Gridcoin.Qml

Rectangle {
    id: main
    objectName: "overviewView"
    color: MMPTheme.backgroundColor

    // Asks the main window to show another view, by its path in the tab menu.
    signal navigateRequested(string path)

    // A right-click on an amount offers to copy it; the item is disabled while
    // the amounts are masked.
    component CopyAmountArea: MouseArea {
        id: copyAmountArea
        property int field: WalletSummary.Balance
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: copyAmountMenu.popup()
        Menu {
            id: copyAmountMenu
            MenuItem {
                objectName: "copyAmountItem"
                text: qsTranslate("OverviewPage", "Copy amount")
                enabled: !WalletSummary.maskValues
                onTriggered: WalletSummary.copyAmount(copyAmountArea.field)
            }
        }
    }

    readonly property int panelSpacing: 10
    readonly property int panelRadius: 4
    readonly property int horBodyMargin: 20
    readonly property int vertBodyMargin: 10

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: header
            color: MMPTheme.headerColor
            Layout.fillWidth: true
            Layout.preferredHeight: 70

            Rectangle {
                id: bottomBorder
                height: 1
                color: MMPTheme.themeSelect("transparent", MMPTheme.cBlack)
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }

            GridLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                columns: 3
                rowSpacing: 2

                Text {
                    id: titleText
                    text: qsTr("Account Overview")
                    font.weight: Font.DemiBold
                    font.pixelSize: 22
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignBottom | Qt.AlignLeft
                }

                Item {
                    // id: widthFiller
                    Layout.fillWidth: true
                    Layout.rowSpan: 2
                }

                GridLayout {
                    id: headerStats
                    columnSpacing: 10
                    rowSpacing: 0
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.rowSpan: 2
                    columns: 3

                    Text {
                        id: magValue
                        visible: ResearcherSummary.showMagnitude
                        text: ResearcherSummary.magnitudeText
                        color: MMPTheme.highlightColor
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                    }

                    Rectangle {
                        id: separator
                        visible: ResearcherSummary.showMagnitude
                        color: MMPTheme.separatorColor
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: headerStats.implicitHeight - 10
                        Layout.rowSpan: 2
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        id: balanceValue
                        text: WalletSummary.balanceRoundedText
                        color: MMPTheme.highlightColor
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                    }

                    Text {
                        id: magLabel
                        visible: ResearcherSummary.showMagnitude
                        text: qsTr("Magnitude")
                        color: MMPTheme.textColor
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 10
                        font.weight: Font.Light
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                    }

                    Text {
                        id: balanceLabel
                        text: qsTr("Your Balance")
                        color: MMPTheme.textColor
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 10
                        font.weight: Font.Light
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                    }
                }

                MouseArea {
                    id: cpidButton
                    implicitWidth: cpidRow.implicitWidth
                    implicitHeight: cpidRow.implicitHeight
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                    onClicked: {
                        //TextEdit workaround without using C++, should probably be changed in the future
                        cpidCopier.selectAll()
                        cpidCopier.copy()
                    }

                    RowLayout {
                        id: cpidRow
                        anchors.fill: parent
                        spacing: 3

                        Rectangle {
                            id: cpidBorder
                            border.color: MMPTheme.textColor
                            radius: 10
                            color: "transparent"
                            implicitWidth: cpidTitleText.implicitWidth + 20
                            implicitHeight: cpidTitleText.implicitHeight + 4
                            Text {
                                id: cpidTitleText
                                text: ResearcherSummary.modeText
                                color: MMPTheme.textColor
                                font.weight: Font.Light
                                anchors.centerIn: parent
                            }
                        }

                        Text {
                            id: cpidText
                            visible: ResearcherSummary.mode === ResearcherSummary.Solo
                            color: MMPTheme.textColor
                            text: ResearcherSummary.cpid
                            clip: true
                            font.pixelSize: 10
                            font.weight: Font.Light
                            Layout.alignment: Qt.AlignVCenter

                            TextEdit{
                                id: cpidCopier
                                text: cpidText.text
                                visible: false
                            }
                        }
                    }

                    ToolTip {
                        x: parent.width/2-width/2
                        y: 20
                        visible: cpidButton.pressed || visible   //Stay visible until timeout
                        timeout: 1000
                        contentItem: Text {
                            id: cpidCopiedText
                            text: qsTr("CPID copied")
                            color: MMPTheme.textColor
                            font.weight: Font.Light
                        }
                        background: Rectangle {
                            id: cpidCopiedBackground
                            color: MMPTheme.bodyColor
                            radius: 8
                            implicitHeight: cpidCopiedText.implicitHeight+10
                            implicitWidth: cpidCopiedText.implicitWidth+10
                            border.color: MMPTheme.borderColor
                        }
                    }
                }
            }
        }

        GridLayout {
            id: panelsGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: panelSpacing
            columnSpacing: panelSpacing
            rowSpacing: panelSpacing
            columns: 2

            Rectangle {
                id: walletDetailsPanel
                color: MMPTheme.bodyColor
                radius: panelRadius
                Layout.column: 0
                Layout.row: 0
                Layout.fillWidth: true
                Layout.preferredWidth: 1 // Equal width with the right panel
                Layout.alignment: Qt.AlignTop
                implicitHeight: walletDetailsLayout.implicitHeight + vertBodyMargin * 2

                ColumnLayout {
                    id: walletDetailsLayout
                    anchors.fill: parent
                    anchors.margins: vertBodyMargin
                    anchors.leftMargin: horBodyMargin
                    anchors.rightMargin: horBodyMargin
                    spacing: 10

                    PanelTitle {
                        id: walletDetailsTitle
                        text: qsTr("Wallet")
                        Layout.fillWidth: true

                        Text {
                            id: walletOutOfSync
                            objectName: "walletOutOfSync"
                            visible: !ClientStatus.inSync
                            text: qsTranslate("OverviewPage", "Out of Sync")
                            color: MMPTheme.cCarminePink
                            anchors {
                                right: walletHelp.left
                                rightMargin: 10
                                verticalCenter: parent.verticalCenter
                            }
                        }

                        HelpHover{
                            id: walletHelp
                            popupWidth: 300
                            verticalPadding: 20
                            horiontalPadding: horBodyMargin
                            text: `
                                <html>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Available")}:</b></font> ${qsTr("Balance available for spending")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Stake")}:</b></font> ${qsTr("Balance that is currently staked")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Unconfirmed")}:</b></font> ${qsTr("Coins that have been received but not yet confirmed")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Total")}:</b></font> ${qsTr("Your total coins")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Research Rewards")}:</b></font> ${qsTr("Earnt research rewards. Stake or make a manual reward claim in the receive view to receive them")}
                                </html>
                            `

                            anchors {
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 4

                        Text {
                            id: availableLabel
                            color: MMPTheme.textColor
                            text: qsTr("Available")+":"
                        }
                        Text {
                            id: availableValue
                            objectName: "availableValue"
                            text: WalletSummary.balanceText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            CopyAmountArea { field: WalletSummary.Balance }
                        }

                        Text {
                            id: stakeLabel
                            color: MMPTheme.textColor
                            text: qsTr("Stake")+":"
                        }
                        Text {
                            id: stakeValue
                            text: WalletSummary.stakeText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            CopyAmountArea { field: WalletSummary.Stake }
                        }

                        Text {
                            id: unconfirmedLabel
                            color: MMPTheme.textColor
                            text: qsTr("Unconfirmed")+":"
                        }
                        Text {
                            id: unconfirmedValue
                            text: WalletSummary.unconfirmedBalanceText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            CopyAmountArea { field: WalletSummary.Unconfirmed }
                        }

                        Text {
                            id: immatureLabel
                            objectName: "immatureLabel"
                            visible: WalletSummary.immatureVisible
                            color: MMPTheme.textColor
                            text: qsTranslate("OverviewPage", "Immature:")
                        }
                        Text {
                            id: immatureValue
                            objectName: "immatureValue"
                            visible: WalletSummary.immatureVisible
                            text: WalletSummary.immatureBalanceText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            CopyAmountArea { field: WalletSummary.Immature }
                        }

                        Text {
                            id: totalLabel
                            color: MMPTheme.textColor
                            font.weight: Font.DemiBold
                            text: qsTr("Total")+":"
                        }
                        Text {
                            id: totalValue
                            objectName: "totalValue"
                            text: WalletSummary.totalText
                            color: MMPTheme.lightTextColor
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            CopyAmountArea { field: WalletSummary.Total }
                        }
                    }
                }
            }

            Rectangle {
                id: networkStatePanel
                color: MMPTheme.bodyColor
                radius: panelRadius
                Layout.column: 0
                Layout.row: 1
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                implicitHeight: networkStateLayout.implicitHeight + vertBodyMargin * 2

                ColumnLayout {
                    id: networkStateLayout
                    anchors.fill: parent
                    anchors.margins: vertBodyMargin
                    anchors.leftMargin: horBodyMargin
                    anchors.rightMargin: horBodyMargin
                    spacing: 10

                    PanelTitle {
                        id: networkStateTitle
                        text: qsTr("Staking")
                        Layout.fillWidth: true

                        HelpHover{
                            id: networkStateQuestionMarkMouseOver
                            popupWidth: 300
                            verticalPadding: 20
                            horiontalPadding: horBodyMargin
                            text: `
                                <html>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Blocks")}:</b></font> ${qsTr("The number of blocks your client currently has on the chain")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Difficulty")}:</b></font> ${qsTr("Current network difficulty. A larger value corresponds to smaller odds of staking")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Net Weight")}:</b></font> ${qsTr("Total number of coins on the entire network which are actively trying to stake")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Coin Weight")}:</b></font> ${qsTr("Number of your coins which are actively trying to stake")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Magnitude")}:</b></font> ${qsTr("Relative measure of your mining contributions")}
                                </html>
                            `

                            anchors {
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 4

                        Text {
                            id: blocksLabel
                            color: MMPTheme.textColor
                            text: qsTr("Blocks")+":"
                        }
                        Text {
                            id: blockValue
                            objectName: "blocksValue"
                            text: ClientStatus.blocksText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: difficultyLabel
                            color: MMPTheme.textColor
                            text: qsTr("Difficulty")+":"
                        }
                        Text {
                            id: difficultyValue
                            text: ClientStatus.difficulty.toLocaleString(Qt.locale(), 'f', 3)
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: netWeightLabel
                            color: MMPTheme.textColor
                            text: qsTr("Net Weight")+":"
                        }
                        Text {
                            id: netWeightValue
                            text: ClientStatus.networkWeight.toLocaleString(Qt.locale(), 'f', 0)
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: coinWeightLabel
                            color: MMPTheme.textColor
                            text: qsTr("Coin Weight")+":"
                        }
                        Text {
                            id: coinWeightValue
                            text: ClientStatus.coinWeightText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Rectangle {
                id: researcherPanel
                color: MMPTheme.bodyColor
                radius: panelRadius
                Layout.column: 0
                Layout.row: 2
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                implicitHeight: researcherLayout.implicitHeight + vertBodyMargin * 2

                ColumnLayout {
                    id: researcherLayout
                    anchors.fill: parent
                    anchors.margins: vertBodyMargin
                    anchors.leftMargin: horBodyMargin
                    anchors.rightMargin: horBodyMargin
                    spacing: 10

                    PanelTitle {
                        id: reseacherTitle
                        text: qsTr("Researcher")
                        Layout.fillWidth: true

                        HelpHover{
                            id: researcherHelp
                            popupWidth: 300
                            verticalPadding: 20
                            horiontalPadding: horBodyMargin
                            text: `
                                <html>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Status\:")}</b></font> ${qsTr("Current wallet status")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Magnitude")}:</b></font> ${qsTr("Relative crunching")}<br><br>
                                <font color='${MMPTheme.textColor}'><b>${qsTr("Research Rewards")}:</b></font> ${qsTr("Magnitude is a measure of your contribution to BOINC projects, calculated from your share of recent average credit (RAC) across whitelisted projects, and used to determine your daily Gridcoin rewards")}
                                </html>
                            `

                            anchors {
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 4

                        Text {
                            id: statusLabel
                            color: MMPTheme.textColor
                            font.pixelSize: 12
                            text: qsTr("Status:")
                        }
                        Text {
                            id: statusValue
                            text: ResearcherSummary.statusText
                            color: ClientStatus.inSync ? MMPTheme.highlightColor : MMPTheme.cCarminePink
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: magnitudeLabel
                            color: MMPTheme.textColor
                            text: qsTr("Magnitude:")
                        }
                        Text {
                            id: magnitudeValue
                            text: ResearcherSummary.magnitudeText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: pendingLabel
                            color: MMPTheme.textColor
                            text: qsTr("Pending Rewards:")

                            Image {
                                id: nearLimitIcon
                                objectName: "nearLimitIcon"
                                visible: ResearcherSummary.accrualNearLimit
                                source: "qrc:/icons/generic/ic_alert.svg"
                                sourceSize: Qt.size(14, 14)
                                anchors {
                                    left: parent.right
                                    leftMargin: 4
                                    verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                        Text {
                            id: estRRValue
                            text: ResearcherSummary.accrualText
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Rectangle {
                id: latestPollsPanel
                color: MMPTheme.bodyColor
                radius: panelRadius
                Layout.column: 0
                Layout.row: 3
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                implicitHeight: pollsLayout.implicitHeight + vertBodyMargin * 2

                ColumnLayout {
                    id: pollsLayout
                    anchors.fill: parent
                    anchors.margins: vertBodyMargin
                    anchors.leftMargin: horBodyMargin
                    anchors.rightMargin: horBodyMargin
                    spacing: 10

                    PanelTitle {
                        id: pollsTitle
                        text: qsTr("Current Polls")
                        Layout.fillWidth: true
                    }

                    Text {
                        id: pollsInfo
                        objectName: "pollsInfo"
                        color: MMPTheme.textColor
                        textFormat: Text.PlainText
                        text: VotingSummary.currentPollTitle
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true

                        MouseArea {
                            objectName: "pollTitleArea"
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: main.navigateRequested("PollView.qml")
                        }
                    }
                }
            }

            Item {
                // id: heightFiller
                Layout.column: 0
                Layout.row: 4
                Layout.fillHeight: true
            }

            Rectangle {
                id: recentTransactionsPanel
                Layout.column: 1
                Layout.row: 0
                Layout.rowSpan: 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1 // Equal width with the left column
                color: MMPTheme.bodyColor
                radius: panelRadius

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: vertBodyMargin
                    anchors.leftMargin: horBodyMargin
                    anchors.rightMargin: horBodyMargin
                    spacing: 10

                    PanelTitle {
                        id: recentTransactionsTitle
                        text: qsTr("Recent Transactions")
                        Layout.fillWidth: true

                        Text {
                            id: transactionsOutOfSync
                            objectName: "transactionsOutOfSync"
                            visible: !ClientStatus.inSync
                            text: qsTranslate("OverviewPage", "Out of Sync")
                            color: MMPTheme.cCarminePink
                            anchors {
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: recentTransactionList
                            objectName: "recentTransactionList"
                            anchors.fill: parent
                            clip: true
                            interactive: false
                            visible: !RecentTransactions.masked
                            model: RecentTransactions
                            delegate: RecentTransactionItem {
                                amountText: model.amountText
                                negative: model.negative
                                icon: model.iconSource
                                account: model.address
                                transactionDate: model.date
                            }
                            // RecentTransactionItem rows are 42 pixels high.
                            onHeightChanged: RecentTransactions.setVisibleRows(Math.floor(height / 42))
                        }

                        ColumnLayout {
                            id: nothingYet
                            objectName: "nothingYet"
                            visible: RecentTransactions.masked || recentTransactionList.count === 0
                            anchors.centerIn: parent
                            spacing: 5

                            Image {
                                source: "qrc:/icons/generic/ic_no_result.svg"
                                Layout.alignment: Qt.AlignHCenter
                                sourceSize: Qt.size(50, 50)
                            }
                            Text {
                                text: RecentTransactions.masked ? qsTranslate("NoResult", "Privacy Enabled...") : qsTr("Nothing here yet...")
                                color: MMPTheme.lightTextColor
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }
                }
            }
        }
    }
}
