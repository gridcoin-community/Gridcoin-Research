/*
    Overview view. The front page of the wallet
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import MMPTheme 1.0

Rectangle {
    id: main
    color: MMPTheme.backgroundColor

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
                        text: _researcherModel.magnitude
                        color: MMPTheme.highlightColor
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                    }

                    Rectangle {
                        id: separator
                        color: MMPTheme.separatorColor
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: headerStats.implicitHeight - 10
                        Layout.rowSpan: 2
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        id: balanceValue
                        text: _walletModel.balance.toLocaleString(Qt.locale(), 'f', 2)
                        color: MMPTheme.highlightColor
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                    }

                    Text {
                        id: magLabel
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
                                text: {
                                    if (_researcherModel.researcherMode == 0) {
                                        return qsTr("Non-cruncher");
                                    } else if (_researcherModel.researcherMode == 1) {
                                        return qsTr("Pool");
                                    } else {
                                        return qsTr("CPID");
                                    }
                                }
                                color: MMPTheme.textColor
                                font.weight: Font.Light
                                anchors.centerIn: parent
                            }
                        }

                        Text {
                            id: cpidText
                            visible: _researcherModel.researcherMode == 2
                            color: MMPTheme.textColor
                            text: _researcherModel.cpid
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
                            text: _walletModel.balance.toLocaleString(Qt.locale(), 'f', 2) + qsTr(" GRC")
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }
                        
                        Text {
                            id: stakeLabel
                            color: MMPTheme.textColor
                            text: qsTr("Stake")+":"
                        }
                        Text {
                            id: stakeValue
                            text: _walletModel.stake.toLocaleString(Qt.locale(), 'f', 2) + qsTr(" GRC")
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: unconfirmedLabel
                            color: MMPTheme.textColor
                            text: qsTr("Unconfirmed")+":"
                        }
                        Text {
                            id: unconfirmedValue
                            text: _walletModel.unconfirmedBalance.toLocaleString(Qt.locale(), 'f', 2) + qsTr(" GRC")
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: totalLabel
                            color: MMPTheme.textColor
                            font.weight: Font.DemiBold
                            text: qsTr("Total")+":"
                        }
                        Text {
                            id: totalValue
                            property real total: _walletModel.balance + _walletModel.stake + _walletModel.unconfirmedBalance
                            text: total.toLocaleString(Qt.locale(), 'f', 2) + qsTr(" GRC")
                            color: MMPTheme.lightTextColor
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
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
                            text: {
                                if  (_clientModel.numBlocks < _clientModel.numBlocksPeers) {
                                    var percent = Math.floor((_clientModel.numBlocks / _clientModel.numBlocksPeers) * 100)
                                    return qsTr("%1 of %2 (%3%)").arg(_clientModel.numBlocks).arg(_clientModel.numBlocksPeers).arg(percent)
                                } else {
                                    return _clientModel.numBlocks
                                }
                            }
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
                            text: _clientModel.difficulty.toLocaleString(Qt.locale(), 'f', 3)
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
                            text: _clientModel.networkWeight.toLocaleString(Qt.locale(), 'f', 0)
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
                            text: _clientModel.coinWeight.toLocaleString(Qt.locale(), 'f', 0)
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
                            text: _researcherModel.status
                            color: _clientModel.inSync ? MMPTheme.highlightColor : MMPTheme.cCarminePink
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
                            text: _researcherModel.magnitude
                            color: MMPTheme.lightTextColor
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                        }

                        Text {
                            id: pendingLabel
                            color: MMPTheme.textColor
                            text: qsTr("Pending Rewards:")
                        }
                        Text {
                            id: estRRValue
                            text: _researcherModel.accrual
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
                        color: MMPTheme.textColor
                        text: _votingModel.currentPollTitle
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
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
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: recentTransactionList
                            anchors.fill: parent
                            clip: true
                            interactive: false
                            model: _walletModel.transactionTableModel
                            delegate: RecentTransactionItem {
                                amount: model.amount
                                icon: model.iconSource
                                account: model.address
                                transactionDate: model.date
                            }
                        }

                        ColumnLayout {
                            id: nothingYet
                            visible: recentTransactionList.count === 0
                            anchors.centerIn: parent
                            spacing: 5
                            
                            Image {
                                source: "qrc:/icons/generic/ic_no_result.svg"
                                Layout.alignment: Qt.AlignHCenter
                                sourceSize: Qt.size(50, 50)
                            }
                            Text {
                                text: qsTr("Nothing here yet...")
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
