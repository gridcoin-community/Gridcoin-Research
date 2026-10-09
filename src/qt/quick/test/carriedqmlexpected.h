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
    {"MMPTheme/MMPControls/GroupBox.qml", 6, 9, "GroupBox -> RectangularShadow"},
};

#endif // BITCOIN_QT_QUICK_TEST_CARRIEDQMLEXPECTED_H
