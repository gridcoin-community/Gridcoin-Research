// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_CARRIEDQMLEXPECTED_H
#define BITCOIN_QT_QUICK_TEST_CARRIEDQMLEXPECTED_H

//! A carried QML file that cannot load below a Qt version, and why.
//!
//! The file set is the set that fails on Qt 6.4.2, the oldest Qt the module
//! builds with. Each version is derived, not measured: the carried types a
//! file instantiates or declares as a property type, followed transitively,
//! reach RectangularShadow (Qt 6.9) or MultiEffect (Qt 6.5). URLs given to
//! Qt.createComponent() load at run time and are not followed. Derive the list
//! again whenever the carried tree or the oldest supported Qt changes.
struct CarriedQmlExpectedFailure {
    const char* file;    //!< Resource path below qrc:/qt/qml/.
    int major;           //!< The failure is expected only below major.minor.
    int minor;
    const char* closure; //!< The chain of types that reaches the Qt type.
};

inline constexpr CarriedQmlExpectedFailure CARRIED_QML_EXPECTED_FAILURES[] = {
    {"Gridcoin/App/ColorizableImage.qml", 6, 5, "ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/FavoritesView.qml", 6, 5, "FavoritesView -> SearchBox -> ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/HelpHover.qml", 6, 9, "HelpHover -> InfoPopup -> RectangularShadow"},
    {"Gridcoin/App/HistoryView.qml", 6, 5, "HistoryView -> SearchBox -> ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/InfoPopup.qml", 6, 9, "InfoPopup -> RectangularShadow"},
    {"Gridcoin/App/MainWindow.qml", 6, 9, "MainWindow -> StatusFooter -> HelpHover -> InfoPopup -> RectangularShadow"},
    {"Gridcoin/App/MenuButton.qml", 6, 5, "MenuButton -> ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/OverviewView.qml", 6, 9, "OverviewView -> HelpHover -> InfoPopup -> RectangularShadow"},
    {"Gridcoin/App/PollView.qml", 6, 5, "PollView -> SearchBox -> ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/ReceiveView.qml", 6, 5, "ReceiveView -> SearchBox -> ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/SearchBox.qml", 6, 5, "SearchBox -> ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/SettingsView.qml", 6, 9, "SettingsView -> HelpHover -> InfoPopup -> RectangularShadow"},
    {"Gridcoin/App/StatusFooter.qml", 6, 9, "StatusFooter -> HelpHover -> InfoPopup -> RectangularShadow"},
    {"Gridcoin/App/TabMenu.qml", 6, 5, "TabMenu -> MenuButton -> ColorizableImage -> MultiEffect"},
    {"Gridcoin/App/WindowManager.qml", 6, 9,
     "WindowManager -> (property) MainWindow -> StatusFooter -> HelpHover -> InfoPopup -> RectangularShadow"},
    {"MMPTheme/MMPControls/GroupBox.qml", 6, 9, "GroupBox -> RectangularShadow"},
};

#endif // BITCOIN_QT_QUICK_TEST_CARRIEDQMLEXPECTED_H
