/*
    A window showing detailed information about a poll such as the poll description and current votes
*/
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import MMPTheme 1.0

Window {
    id: window
    title: qsTr("Poll Details")
    width: 700
    height: 600
    minimumWidth: 450
    minimumHeight: 250
    flags: Qt.Dialog
    property alias title: titleLabel.text //TODO refactor this to a new name
    property int startDate: 0
    property int endDate: 100000
    property string pollDescription: "Description"
    property string moreInfoURL: "https://gridcoin.us"
    property alias topAnswer: topAnswerDataLabel.text
    property int topAnswerWeight: 21837217
    property int totalWeight: 45837217

    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: window.close()
    }

    Rectangle {
        id: background
        color: MMPTheme.backgroundColor
        anchors.fill: parent

        Rectangle {
            id: body
            color: MMPTheme.bodyColor
            radius: 4
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                margins: 10
            }
            Text {
                id: titleLabel
                color: MMPTheme.textColor
                font.weight: Font.DemiBold
                text: "Title"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                anchors {
                    top: dateRangeText.bottom
                    topMargin: 10
                    left: dateRangeText.left
                    right: parent.right
                    rightMargin: 10
                }
            }
            Text {
                id: dateRangeText
                text: new Date(startDate*1000).toLocaleString(Qt.locale(), Locale.ShortFormat) + " → " + new Date(endDate*1000).toLocaleString(Qt.locale(), Locale.ShortFormat)
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                anchors {
                    top: parent.top
                    left: parent.left
                    margins: 20
                }
            }
            Text {
                id: pollDescriptionLabel
                text: pollDescription
                color: MMPTheme.textColor
                wrapMode: Text.WordWrap
                anchors {
                    top: titleLabel.bottom
                    topMargin: 10
                    left: titleLabel.left
                    right: parent.right
                    rightMargin: 10
                }
            }
            ClickableLink {
                id: moreInfoItem
                urlString: "https://gridcoin.us"
                anchors {
                    top: pollDescriptionLabel.bottom
                    topMargin: 10
                    left: titleLabel.left
                }
            }

            Text {
                id: topAnswerLabel
                text: qsTr("Top Answer:")
                color: MMPTheme.textColor
                anchors {
                    left: titleLabel.left
                    top: moreInfoItem.bottom
                    topMargin: 10
                }
            }

            Text {
                id: topAnswerDataLabel
                text: "Yes"
                color: MMPTheme.translucent(MMPTheme.textColor, 0.7)
                anchors {
                    top: topAnswerLabel.top
                    left: topAnswerLabel.right
                    leftMargin: 5
                }
            }

            ListView {
                id: pollResultListView
                clip: true
                ScrollIndicator.vertical: ScrollIndicator {
                    parent: pollResultListView.parent
                    anchors {
                        top: pollResultListView.top
                        bottom: pollResultListView.bottom
                        right: pollResultListView.right
                        rightMargin: 1
                    }
                }
                anchors {
                    left: parent.left
                    right: parent.right
                    top: topAnswerLabel.bottom
                    bottom: parent.bottom
                    margins: 20
                }
                model: ListModel {
                    id: answerModel
                    ListElement {
                        answerTitle: "Yes"
                        voteWeight: 21837217
                    }
                    ListElement {
                        answerTitle: "No"
                        voteWeight: 183721
                    }
                    ListElement {
                        answerTitle: "Abstain"
                        voteWeight: 3624538
                    }
                }
                delegate: Item {
                    id: resultDelegate
                    height: 80
                    width: parent.width

                    Rectangle {
                        id: separatorRect
                        color: MMPTheme.separatorColor
                        height: 1
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                        }
                    }
                    Text {
                        id: answerLabel
                        text: model.answerTitle
                        color: MMPTheme.textColor
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: separatorRect.bottom
                            topMargin: 10
                        }
                    }

                    ProgressBar {
                        id: voteProportionBar
                        value: {
                            var ratio = model.voteWeight / topAnswerWeight
                            if (ratio < 0.005) {
                                return 0.0
                            } else {
                                return ratio
                            }
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                            top: answerLabel.bottom
                            topMargin: 5
                        }
                        background: Rectangle {
                            id: barBackgroundRect
                            height: 12
                            radius: height/2
                            color: MMPTheme.ternaryBodyColor
                        }
                        contentItem: Item {
                            id: content
                            implicitWidth: 200
                            implicitHeight: 12
                            Rectangle {
                                height: parent.height
                                width: voteProportionBar.visualPosition * parent.width
                                radius: height/2
                                gradient: Gradient{
                                    orientation: Gradient.Horizontal
                                    GradientStop {
                                        position: 0.0
                                        color: "#00dbde"
                                    }
                                    GradientStop {
                                        position: 1.0
                                        color: {
                                            var startRed = 0
                                            var startGreen = 219
                                            var startBlue = 222
                                            var endRed = 252
                                            var endGreen = 0
                                            var endBlue = 255
                                            var outRed = voteProportionBar.value * endRed + (1-voteProportionBar.value) * startRed
                                            var outGreen = voteProportionBar.value * endGreen + (1-voteProportionBar.value) * startGreen
                                            var outBlue = voteProportionBar.value * endBlue + (1-voteProportionBar.value) * startBlue
                                            return Qt.rgba(outRed/255, outGreen/255, outBlue/255, 1)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        id: weightLabel
                        text: qsTr("Weight:")
                        color: MMPTheme.translucent(MMPTheme.textColor, 0.5)
                        anchors {
                            top: voteProportionBar.bottom
                            topMargin: 10
                            left: parent.left
                        }
                    }
                    Text {
                        id: weightDataLabel
                        text: model.voteWeight
                        color: MMPTheme.translucent(MMPTheme.textColor, 0.5)
                        anchors {
                            top: weightLabel.top
                            left: weightLabel.right
                            leftMargin: 5
                        }
                    }
                    Text {
                        id: answerPercentLabel
                        text: (model.voteWeight / totalWeight * 100).toFixed(2) + "%"
                        color: MMPTheme.translucent(MMPTheme.textColor, 0.5)
                        anchors {
                            top: weightLabel.top
                            right: parent.right
                        }
                    }
                }
            }



        }
    }
}
