/*
    Provides an image which can be given different colours programmatically with tintColor
*/
import QtQuick
import Qt5Compat.GraphicalEffects
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
        // No layer at colorization 0, so an untinted image goes through no
        // effect.
        layer.enabled: img.colorization > 0
        // ColorOverlay paints the tint over the image at the image's alpha;
        // colorization scales the tint's own alpha.
        layer.effect: ColorOverlay {
            objectName: "tintOverlay"
            color: Qt.rgba(img.overlayColor.r, img.overlayColor.g, img.overlayColor.b,
                           img.overlayColor.a * img.colorization)
        }
    }

}
