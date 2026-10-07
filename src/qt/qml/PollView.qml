/*
    View showing current and completed polls
    Provides access to poll creation and poll details
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
            text: qsTr("Polls")
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
            id: activeSeparator
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

        property string activeString: qsTr("%n active", "", pollListView.count)
        Text {
            id: activeNumText
            text: parent.activeString.slice(0,1)
            color: MMPTheme.highlightColor
            font.weight: Font.DemiBold
            anchors {
                baseline: titleText.baseline
                baselineOffset: -2
                left: activeSeparator.right
                leftMargin: 8
            }
        }
        Text {
            id: activeLabelText
            height: activeSeparator.height
            text:  parent.activeString.slice(1)
            color: MMPTheme.textColor
            anchors {
                baseline: activeNumText.baseline
                left: activeNumText.right
            }
        }

        Rectangle {
            id: completedSeparator
            color: MMPTheme.separatorColor
            height: titleText.height*0.625
            width: 1
            anchors {
                bottom: titleText.baseline
                bottomMargin: -1
                left: activeLabelText.right
                leftMargin: 8
            }
        }

        property string completedString: qsTr("%n completed", "", pollListView.count)
        Text {
            id: completedNumText
            text: parent.completedString.slice(0,1)
            color: MMPTheme.highlightColor
            font.weight: Font.DemiBold
            anchors {
                baseline: titleText.baseline
                baselineOffset: -2
                left: completedSeparator.right
                leftMargin: 8
            }
        }
        Text {
            id: completedLabelText
            height: completedSeparator.height
            text:  parent.completedString.slice(1)
            color: MMPTheme.textColor
            anchors {
                baseline: completedNumText.baseline
                left: completedNumText.right
            }
        }

        SearchBox {
            placeholderText: qsTr("Search by title")
            //onTextChanged: listview.sortModel(text)
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 20
            }
        }
    }
    Rectangle {
        id: tabBarSeperator
        height: 1
        color: MMPTheme.bodySeparatorColor
        anchors {
            left: parent.left
            right: parent.right
            bottom: header.bottom
        }
    }

    Rectangle {
        id: tabSelection
        color: MMPTheme.ternaryBodyColor
        height: 40
        anchors {
            left: parent.left
            right: parent.right
            top: tabBarSeperator.bottom
        }

        PollTabButton {
            id: activeTabButton
            text: "Active"
            current: true
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            onClicked: {
                current = true
                completedTabButton.current = false
//                participatedTabButton.current = false
                //TODO: filter
            }
        }
        PollTabButton {
            id: completedTabButton
            text: "Completed"
            current: false
            anchors {
                left: activeTabButton.right
                top: parent.top
                bottom: parent.bottom
            }
            onClicked: {
                current = true
                activeTabButton.current = false
//                participatedTabButton.current = false
                //TODO: filter
            }
        }
        //Not in use
//        PollTabButton {
//            id: participatedTabButton
//            text: "Participated"
//            current: false
//            anchors {
//                left: completedTabButton.right
//                top: parent.top
//                bottom: parent.bottom
//            }
//            onClicked: {
//                current = true
//                completedTabButton.current = false
//                activeTabButton.current = false
//                //TODO: filter
//            }
//        }


        Button {
            id: refreshButton
            text: "Refresh"
            icon.source: MMPTheme.isLightTheme ? "/icons/buttons/ic_btn_refresh_light.svg" : "/icons/buttons/ic_btn_refresh_dark.svg"
            anchors {
                right: parent.right
                rightMargin: 10
                verticalCenter: parent.verticalCenter
            }
        }
        Button {
            id: createPollButton
            text: "Create Poll"
            icon.source: MMPTheme.isLightTheme ? "/icons/buttons/ic_btn_create_light.svg" : "/icons/buttons/ic_btn_create_dark.svg"
            anchors {
                right: refreshButton.left
                rightMargin: 10
                verticalCenter: parent.verticalCenter
            }
            onPressed: {
                var component = Qt.createComponent("PollCreationWindow.qml")
                var windowObj = component.createObject(window)
                windowObj.show()
            }
        }
    }


    ListView {
        id: pollListView
        clip: true
        spacing: 10
        ScrollIndicator.vertical: ScrollIndicator {
            parent: pollListView.parent
            anchors {
                top: pollListView.top
                bottom: pollListView.bottom
                right: pollListView.right
                rightMargin: 1
            }
        }
        anchors {
            top: tabSelection.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 10
        }
        //(new Date().toLocaleString(Qt.locale(), Locale.LongFormat))
        model: ListModel {
            id: pollModel
            //Types:
            // 0 Balance
            // 1 Balance + Magnitude
            ListElement {
                title: "Give Zigzagoon everyone's GRC?"
                expires:  1609823156
                votes: 20
                totalWeight: 1044009
                bestAnswer: "This is a really really really really long best answer"
                activeVoteWeight: 12874824
                type: 1
            }
            ListElement {
                title: "Happy New Year?"
                expires:  1909823156
                votes: 2213
                totalWeight: 123984
                bestAnswer: "Yes"
                activeVoteWeight: 12874824
                type: 0
            }
        }
        delegate: Rectangle {
            width: parent.width
            height: 155
            color: MMPTheme.bodyColor
            radius: 4

            Text {
                id: titleLabel
                text: model.title
                color: MMPTheme.textColor
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                anchors {
                    top: parent.top
                    topMargin: 15
                    left: parent.left
                    leftMargin: 20
                    right: parent.right
                    rightMargin: 20
                }
            }
            Text {
                id: expirationLabel
                text: qsTr("Expiration:")
                color: MMPTheme.textColor
                anchors {
                    top: titleLabel.bottom
                    topMargin: 15
                    left: titleLabel.left
                }
            }
            Text {
                id: bestAnswerLabel
                text: qsTr("Top Answer:")
                color: MMPTheme.textColor
                anchors {
                    top: expirationLabel.bottom
                    topMargin: 5
                    left: titleLabel.left
                }
            }
            Text {
                id: expirationDataLabel
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                text: (new Date(model.expires*1000).toLocaleString(Qt.locale(), "d MMM yyyy, hh:mm:ss"))
                anchors {
                    top: titleLabel.bottom
                    topMargin: 15
                    left: expirationLabel.right
                    leftMargin: 20
                }
            }
            Text {
                id: bestAnswerDataLabel
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                text: model.bestAnswer
                elide: Text.ElideRight
                anchors {
                    top: expirationDataLabel.bottom
                    topMargin: 5
                    left: expirationDataLabel.left
                    right: parent.horizontalCenter
                    rightMargin: -20
                }
            }

            Text {
                id: activeVoteWeightLabel
                text: qsTr("Active Vote Weight:")
                color: MMPTheme.textColor
                anchors {
                    left: titleLabel.left
                    top: bestAnswerLabel.bottom
                    topMargin: 5
                }
            }
            Text {
                id: activeVoteWeightDataLabel
                text: model.activeVoteWeight
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                anchors {
                    left: activeVoteWeightLabel.right
                    leftMargin: 10
                    top: activeVoteWeightLabel.top
                }
            }

            Text {
                id: votesLabel
                color: MMPTheme.textColor
                text: qsTr("Votes:")
                anchors {
                    top: expirationLabel.top
                    left: parent.horizontalCenter
                    leftMargin: 70
                }
            }
            Text {
                id: votesDataLabel
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                text: model.votes
                anchors {
                    left: weightDataLabel.left
                    top: votesLabel.top
                }
            }
            Text {
                id: weightLabel
                color: MMPTheme.textColor
                text: qsTr("Total Weight:")
                anchors {
                    left: votesLabel.left
                    top: bestAnswerLabel.top
                }
            }
            Text {
                id: weightDataLabel
                text: model.totalWeight
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                anchors {
                    top: weightLabel.top
                    left: weightLabel.right
                    leftMargin: 10
                }
            }
            Text {
                id: percentActiveVoteWeightLabel
                text: qsTr("% of AVW:")
                color: MMPTheme.textColor
                anchors {
                    left: votesLabel.left
                    top: activeVoteWeightLabel.top
                }
            }
            Text {
                id: percentActiveVoteWeightDataLabel
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                text: (model.totalWeight/model.activeVoteWeight*100).toFixed(2) + "%"
                anchors {
                    top: percentActiveVoteWeightLabel.top
                    left: weightDataLabel.left
                }
            }

            Rectangle {
                id: pollFooterRect
                color: MMPTheme.ternaryBodyColor
                anchors {
                    top: activeVoteWeightLabel.bottom
                    topMargin: 15
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    bottomMargin: 4
                }
                Rectangle {
                    id: pollFooterRectCornerRect
                    height: 8
                    radius: 4
                    color: MMPTheme.ternaryBodyColor
                    anchors {
                        verticalCenter: parent.bottom
                        left: parent.left
                        right: parent.right
                    }
                }
                Rectangle {
                    id: balanceIndicatorCircle
                    color: MMPTheme.cHavelockBlue
                    height: 20
                    width: balanceLabel.implicitWidth + 20
                    radius: 10
                    anchors {
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                        leftMargin: 20
                    }

                    Text {
                        id: balanceLabel
                        color: MMPTheme.cWhite
                        text: qsTr("Balance")
                        anchors.centerIn: parent
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
                Rectangle {
                    id: magnitudeIndicatorCircle
                    color: MMPTheme.cDullLime
                    height: 20
                    width: magnitudeLabel.implicitWidth + 20
                    radius: 10
                    visible: model.type === 1
                    anchors {
                        left: balanceIndicatorCircle.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }

                    Text {
                        id: magnitudeLabel
                        color: MMPTheme.cWhite
                        text: qsTr("Magnitude")
                        anchors.centerIn: parent
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Button {
                    id: detailsButton
                    text: qsTr("Details")
                    icon.source: MMPTheme.isLightTheme ? "qrc:/icons/buttons/ic_btn_details_light.svg" : "qrc:/icons/buttons/ic_btn_details_dark.svg"
                    onPressed: {
                        var component = Qt.createComponent("PollDetailsWindow.qml")
                        var windowObj = component.createObject(window)
                        windowObj.title = model.title
                        windowObj.endDate = model.expires
                        windowObj.topAnswer = model.bestAnswer
                        windowObj.show()
                    }
                    anchors {
                        right: parent.right
                        rightMargin: 10
                        verticalCenter: parent.verticalCenter
                    }
                }
                Button {
                    id: voteButton
                    text: qsTr("Vote")
                    icon.source: MMPTheme.isLightTheme ? "qrc:/icons/buttons/ic_btn_vote_light.svg" : "qrc:/icons/buttons/ic_btn_vote_dark.svg"
                    visible: CurrentTime.currentTime < new Date(model.expires*1000)
                    onPressed: {
                        var component = Qt.createComponent("PollVoteWindow.qml")
                        var windowObj = component.createObject(window)
                        windowObj.show()
                    }

                    anchors {
                        right: detailsButton.left
                        rightMargin: 10
                        verticalCenter: parent.verticalCenter
                    }
                }

            }
        }
    }
}
