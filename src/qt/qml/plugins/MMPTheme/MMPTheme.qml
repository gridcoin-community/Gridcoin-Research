pragma Singleton

import QtQuick 2.15
import QtQml 2.15

QtObject {
    readonly property color cBlack: "#000000"
    readonly property color cMirage: "#171E26"
    readonly property color cSpaceBlack: "#1A2632"
    readonly property color cMobster: "#B3171B24"
    readonly property color cOxfordBlue: "#3A465D"
    readonly property color cOxfordOffBlue: "#17222C"
    readonly property color cLightSlateGray: "#7383A1"
    readonly property color cLilyWhite: "#EAEDED"
    readonly property color cFrostWhite: "#F4F7F9"
    readonly property color cWhite: "#FFFFFF"
    readonly property color cViolentViolet: "#360166"
    readonly property color cBluePurple: "#9013FE"
    readonly property color cHavelockBlue: "#4A90E2"
    readonly property color cDullLime: "#2AC940"
    readonly property color cCarminePink: "#ED5144"
    readonly property color cTextLightGrey: "#5c5f65"
    readonly property color cTextDarkGrey: "#b9bbbe"

    readonly property color backgroundColor: isLightTheme ? cLilyWhite : cSpaceBlack
    readonly property color textColor: isLightTheme ? cOxfordBlue : cWhite
    readonly property color lightTextColor: isLightTheme ? cTextLightGrey : cTextDarkGrey
    readonly property color borderColor: translucent(isLightTheme ? cOxfordBlue : cWhite, 0.7)
    readonly property color lightBorderColor: isLightTheme ? "#b0b5be" : "#74787d"
    readonly property color highlightColor: isLightTheme ? cBluePurple : cHavelockBlue
    readonly property color bodyColor: isLightTheme ? cWhite : cMirage
    readonly property color separatorColor: isLightTheme ? cLilyWhite : translucent(cLightSlateGray, 0.4)
    readonly property color secondaryBodyColor: isLightTheme ? cFrostWhite : cMirage
    readonly property color ternaryBodyColor: isLightTheme ? cFrostWhite : "#161b24"
    readonly property color bodySeparatorColor: isLightTheme ? cLilyWhite : cBlack
    readonly property color headerColor: isLightTheme ? cWhite : cSpaceBlack
    
    //Theme enumeration
    readonly property int lightTheme: 0
    readonly property int darkTheme: 1
    readonly property bool isLightTheme: theme === lightTheme
    property int theme: lightTheme

    readonly property font baseFont: Qt.font({
        family: "SF Pro Text",
        pixelSize: 12
    })

    function themeSelect (lightObj, darkObj ){
        return isLightTheme ? lightObj : darkObj
    }

    //Sets the alpha of a color, where translucency is a decimal [0,1]
    function translucent (color, translucency) {
        return Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, translucency)
    }
}
