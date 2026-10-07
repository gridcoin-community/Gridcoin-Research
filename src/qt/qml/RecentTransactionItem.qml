/*
    Item used in overview view to show recent transactions alongside an icon
    Each state represents a different transaction type
*/
import QtQuick
import QtQml
import MMPTheme 1.0

Item {
    id: main
    property real amount
    property date transactionDate
    property alias account: addressLabel.text
    property alias icon: transactionIcon.source
    height: 42  //Chosen to have a fit of 7 transactions on the page
    width: parent.width

    Image {
        id: transactionIcon
        height: 30
        width: 30
        sourceSize: Qt.size(width, height)
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
        }
    }
    Column {
        id: transactionInfoColumn
        anchors {
            verticalCenter: parent.verticalCenter
            left: transactionIcon.right
            right: parent.right
            leftMargin: 10
        }
        Item {
            id: transactionInfoTopRow
            width: parent.width
            height: amountLabel.implicitHeight
            anchors {
                left: parent.left
                right: parent.right
            }

            Text {
                id: amountLabel
                text: (amount >= 0 ? Qt.locale().positiveSign : "") + amount.toLocaleString(Qt.locale(), 'g', 8)
                clip: true
                color: amount >= 0 ? MMPTheme.cDullLime : MMPTheme.cCarminePink
                anchors {
                    left: parent.left
                    right: dateLabel.left
                }
            }
            Text {
                id: dateLabel
                horizontalAlignment: Text.AlignRight
                text: transactionDate.toLocaleDateString(Qt.locale(), Locale.ShortFormat) +" "+ transactionDate.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                font.weight: Font.Light
                font.pointSize: 10
                color: MMPTheme.lightTextColor
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                }
            }
        }
        Text {
            id: addressLabel
            width: parent.width
            text: account
            font.pixelSize: 11
            color: MMPTheme.lightTextColor
            elide: Text.ElideRight
        }
    }

}
