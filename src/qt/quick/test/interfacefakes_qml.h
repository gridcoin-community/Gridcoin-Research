// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_INTERFACEFAKES_QML_H
#define BITCOIN_QT_QUICK_TEST_INTERFACEFAKES_QML_H

#include <qt/guifrontend.h>
#include <qt/quick/qmlfrontend.h>

#include <QString>
#include <QUrl>

#include <atomic>
#include <functional>
#include <string>

namespace qml_test {

//! The seam fake these tests use: a GuiFrontEnd that queues modal core
//! messages and does nothing else. The interfaces:: fakes arrive with the first
//! test that needs one.
class QueuingFrontEnd : public GuiFrontEnd
{
public:

    QObject* construct() override { return nullptr; }
    void destroyMain() override {}
    QObject* createSplash() override { return nullptr; }
    void finishSplash() override {}
    void destroySplash() override {}
    void showBuildMismatchWarning(const QString&, const QString&) override {}
    void setIpcConnectionInfo(const GuiIpcInfo&) override {}
    void attachModels(const ModelBundle&) override {}
    void showMain(bool) override {}
    WId nativeWindowId() override { return 0; }
    void requestQuit() override {}
    bool queuesModalCoreMessages() const override { return true; }

protected:
    void hideMain() override {}
    void detachClient() override {}
    void detachWallet() override {}
    void detachMRC() override {}
    void detachResearcher() override {}
    void detachVoting() override {}
    void detachPSGT() override {}
};

//! The root the module tests load: an Item in the test-only FixtureTheme module.
inline const QUrl FIXTURE_ROOT_URL{QStringLiteral("qrc:/qt/qml/FixtureTheme/FixtureRoot.qml")};

//! Callbacks that treat both of libmultiprocess's disconnect messages as a lost
//! connection and count each notification into `disconnects`.
inline QmlFrontEnd::Callbacks CountingCallbacks(std::atomic<int>& disconnects)
{
    return QmlFrontEnd::Callbacks{
        [](const std::string& what) {
            return what.find("interrupted by disconnect") != std::string::npos ||
                   what.find("called after disconnect") != std::string::npos;
        },
        [&disconnects](const std::string&) { ++disconnects; }};
}

} // namespace qml_test

#endif // BITCOIN_QT_QUICK_TEST_INTERFACEFAKES_QML_H
