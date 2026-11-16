/*
    Footer for the MainWindow showing wallet status information
*/
import QtQuick
import MMPTheme 1.0
import Gridcoin.Qml
Rectangle {
    id: footer
    gradient: Gradient {
        orientation: Gradient.Vertical
        GradientStop { position: 0; color: MMPTheme.themeSelect("#f2f2f2", "#171717")}
        GradientStop { position: 1; color: MMPTheme.themeSelect("#d4d4d4", "#171717")}
    }
    Rectangle {
        id: topBorderRect
        height: 1
        color: MMPTheme.themeSelect("#e7e7e7","#19222a")
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
    }

    Rectangle {
        id: testNetRect
        objectName: "testNetMarker"
        color: MMPTheme.cCarminePink
        width: visible ? testNetText.implicitWidth + 10 : 0
        height: testNetText.implicitHeight + 5
        radius: 4
        visible: ClientStatus.isTestNet
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            leftMargin: 5
        }
        Text {
            id: testNetText
            text: qsTr("TESTNET")
            font.pixelSize: 10
            font.weight: Font.Bold
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 5
            }
        }
    }

    Text {
        id: clientMessage
        objectName: "footerMessage"
        textFormat: Text.PlainText
        text: ClientStatus.statusBarWarnings
        color: MMPTheme.themeSelect("#54575d","#b7b7b7")
        font.pixelSize: 10
        clip: true
        anchors {
            left: testNetRect.right
            right: iconsRow.left
            verticalCenter: parent.verticalCenter
            leftMargin: 5
        }
    }
    Row {
        id: iconsRow
        layoutDirection: Qt.RightToLeft
        spacing: 8
        property int iconSize: 15
        anchors {
            right: parent.right
            rightMargin: spacing
            verticalCenter: parent.verticalCenter
        }

        HelpHover {
            id: lockIcon
            objectName: "lockIcon"
            // No image until the wallet's lock state has been read.
            imageSource: {
                if (!WalletSummary.encryptionStatusKnown) return ""
                switch (WalletSummary.encryptionStatus) {
                case WalletSummary.Locked:
                    return MMPTheme.themeSelect("qrc:/icons/statusbar/ic_locked_light.svg","qrc:/icons/statusbar/ic_locked_dark.svg")
                case WalletSummary.UnlockedForStakingOnly:
                    return MMPTheme.themeSelect("qrc:/icons/statusbar/ic_unlocked_staking_light.svg","qrc:/icons/statusbar/ic_unlocked_staking_dark.svg")
                case WalletSummary.Unencrypted:
                    // Unlike Widgets, an unencrypted wallet does not share an unlocked one's open lock.
                    return "qrc:/icons/generic/ic_alert.svg"
                default:
                    return "qrc:/icons/statusbar/ic_unlocked.svg"
                }
            }
            iconSize: iconsRow.iconSize
            text: WalletSummary.encryptionStatusKnown ? WalletSummary.encryptionToolTip : qsTr("The wallet's lock state is not known yet.")
        }
        HelpHover {
            id: connectionStrengthIcon
            objectName: "connectionIcon"
            readonly property var levelNames: ["na", "poor", "average", "normal", "good"]
            imageSource: MMPTheme.themeSelect("qrc:/icons/statusbar/ic_connection_" + levelNames[ClientStatus.connectionLevel] + "_light.svg",
                                              "qrc:/icons/statusbar/ic_connection_" + levelNames[ClientStatus.connectionLevel] + "_dark.svg")
            iconSize: iconsRow.iconSize
            text: ClientStatus.connectionToolTip
        }
        HelpHover {
            id: stakingIcon
            objectName: "stakingIcon"
            imageSource: ClientStatus.stakingState === ClientStatus.Staking
                         ? MMPTheme.themeSelect("qrc:/icons/statusbar/ic_staking_yes_light.svg","qrc:/icons/statusbar/ic_staking_yes_dark.svg")
                         : ClientStatus.stakingState === ClientStatus.UnableToStake
                         ? MMPTheme.themeSelect("qrc:/icons/statusbar/ic_staking_problem_light.svg","qrc:/icons/statusbar/ic_staking_problem_dark.svg")
                         : MMPTheme.themeSelect("qrc:/icons/statusbar/ic_staking_no_light.svg","qrc:/icons/statusbar/ic_staking_no_dark.svg")
            iconSize: iconsRow.iconSize
            text: ClientStatus.stakingToolTip
        }
        HelpHover {
            id: networkIcon
            objectName: "syncIcon"
            imageSource: {
                if (ClientStatus.syncState === ClientStatus.UpToDate) return "qrc:/icons/statusbar/ic_syncing_uptodate.svg"
                var name = ClientStatus.syncState === ClientStatus.NoConnections ? "stuck"
                         : ClientStatus.syncProgress === 0 ? "unsynced" : String(ClientStatus.syncProgress)
                return MMPTheme.themeSelect("qrc:/icons/statusbar/ic_syncing_" + name + "_light.svg",
                                            "qrc:/icons/statusbar/ic_syncing_" + name + "_dark.svg")
            }
            iconSize: iconsRow.iconSize
            text: ClientStatus.syncToolTip
        }
        HelpHover {
            id: beaconIcon
            objectName: "beaconIcon"
            visible: ResearcherSummary.beaconVisible
            imageSource: ResearcherSummary.beaconActive
                         ? MMPTheme.themeSelect("qrc:/icons/statusbar/ic_beacon_online_light.svg","qrc:/icons/statusbar/ic_beacon_online_dark.svg")
                         : MMPTheme.themeSelect("qrc:/icons/statusbar/ic_beacon_offline_light.svg","qrc:/icons/statusbar/ic_beacon_offline_dark.svg")
            iconSize: iconsRow.iconSize
            text: ResearcherSummary.beaconToolTip
        }
        HelpHover {
            id: scraperIcon
            objectName: "scraperIcon"
            imageSource: {
                var name = "waiting"
                switch (ClientStatus.scraperState) {
                case ClientStatus.Inactive: name = "inactive"; break
                case ClientStatus.Converged: name = "ok"; break
                case ClientStatus.NoConvergence: name = "no_convergence"; break
                }
                return MMPTheme.themeSelect("qrc:/icons/statusbar/ic_scraper_" + name + "_light.svg",
                                            "qrc:/icons/statusbar/ic_scraper_" + name + "_dark.svg")
            }
            iconSize: iconsRow.iconSize
            text: ClientStatus.scraperToolTip
        }
    }
}
