/*
    Window to display a QR code for an address
*/
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import MMPTheme 1.0

Window {
    id: window
    property string address
    title: qsTr("Address QR Code")
    width: 650
    height: 300
    minimumWidth: 450
    minimumHeight: 250
    maximumHeight: 650
    maximumWidth: 1200
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
                bottom: saveButton.top
                left: parent.left
                right: parent.right
                margins: 10
            }
            Image {
                id: qrRect
                width: Math.min(height, background.width*0.5)
                source: "qrc:/icons/buttons/ic_btn_qr_code_light.svg" //TODO change to render qr code
                sourceSize: Qt.size(600, 600) //Remove to render at native image res
                fillMode: Image.PreserveAspectFit
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                    margins: 10
                }
            }
            Text {
                id: urlText
                text: "gridcoin:"+address+"?label:Zigzagoon Balloon"
                wrapMode: Text.WrapAnywhere
                color: MMPTheme.textColor
                font.weight: Font.Medium
                anchors {
                    left: qrRect.right
                    top: parent.top
                    right: parent.right
                    margins: 10
                }
            }
            Text {
                id: titleLabel
                text: qsTr("Title:")
                color: MMPTheme.textColor
                horizontalAlignment: Text.AlignRight
                anchors {
                    left: qrRect.right
                    right: messageLabel.right
                    top: urlText.bottom
                    leftMargin: 10
                    topMargin: 20
                }
            }
            TextField {
                id: titleField
                anchors {
                    left: titleLabel.right
                    right: parent.right
                    verticalCenter: titleLabel.verticalCenter
                    margins: 10
                }
            }
            Text {
                id: messageLabel
                text: qsTr("Message:")
                color: MMPTheme.textColor
                anchors {
                    left: qrRect.right
                    top: titleLabel.bottom
                    leftMargin: 10
                    topMargin: 20
                }
            }
            TextArea {
                id: messageArea
                wrapMode: Text.WordWrap
                anchors {
                    left: messageLabel.right
                    top: titleField.bottom
                    right: parent.right
                    bottom: parent.bottom
                    margins: 10
                }
            }
        }
        Button {
            id: saveButton
            text: qsTr("Save to File")
            icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_save_qr_light.svg","qrc:/icons/buttons/ic_btn_save_qr_dark.svg")
            anchors {
                right: parent.right
                bottom: parent.bottom
                margins: 10
            }
        }
    }
}
