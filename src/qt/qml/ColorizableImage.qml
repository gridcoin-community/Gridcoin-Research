/*
    Provides an image which can be given different colours programmatically with tintColor
*/
import QtQuick
import QtQuick.Effects
Item {
    id: root
    property alias tintColor: img.overlayColor
    property alias colorization : img.colorization
    property alias source: img.source
    implicitHeight: img.implicitHeight
    implicitWidth: img.implicitWidth
    Image {
        id: img
        property color overlayColor: "black"
        property real colorization: 0
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(width,height)
        layer.enabled: true
        layer.effect: MultiEffect {
            colorizationColor: img.overlayColor
            colorization: img.colorization
        }
    }

}
