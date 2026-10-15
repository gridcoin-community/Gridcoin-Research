// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_BRIDGETESTS_H
#define BITCOIN_QT_QUICK_TEST_BRIDGETESTS_H

#include <QObject>
#include <QTest>

class QmlBridgeTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bridgeSlotSignatures();
    void splashParsesInitMessages();
    void splashMessageArrivesOnGuiThread();
    void showMainSetsShellShown();
    void requestCloseDecidesQuitOrHide();
    void throwingHideMainStillClosesRunner();
    void rootLoadErrorsReachStderr();
    void unloadableRootEndsTheGui();
    void requestQuitSetsQuitRequested();
    void modalMessageReachesShellWithoutWaiting();
    void serialRunnerThreadNeverWaits();
    void pooledRunnerThreadNeverWaits();
    void constructFailsWhenRootDoesNotLoad();
};

namespace qml_test {
//! The first argument that makes the test binary run GuiMain as the child of
//! QmlBridgeTests::unloadableRootEndsTheGui instead of running the tests.
inline constexpr const char* GUIMAIN_CHILD_ARG = "--guimain-child-unloadable-root";
//! Runs GuiMain with a QML front end whose root does not exist, closing each
//! message box it shows after printing the box's text, then prints GuiMain's
//! status and returns it.
int RunGuiMainChild(int argc, char* argv[]);
} // namespace qml_test

#endif // BITCOIN_QT_QUICK_TEST_BRIDGETESTS_H
