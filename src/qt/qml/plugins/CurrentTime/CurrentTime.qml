pragma Singleton

import QtQuick
import QtQml

QtObject {
    property date currentTime: new Date()
    property Timer timer: Timer {
        interval: 60 * 1000
        repeat: true
        running: true
        onTriggered: currentTime = new Date()
    }
}
