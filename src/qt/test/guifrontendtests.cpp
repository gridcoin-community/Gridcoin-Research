// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/guifrontendtests.h"

#include "qt/bitcoingui.h"
#include "qt/guifrontend.h"
#include "qt/splashscreen.h"

#include <QMetaObject>
#include <QString>

#include <stdexcept>
#include <string>
#include <vector>

//!
//! \file guifrontendtests.cpp
//! \brief The GuiFrontEnd seam's teardown contract (the detach order, its
//! idempotence and the exception-path guard) and the slot signatures the
//! string-invoked core bridges in bitcoin.cpp depend on.
//!

namespace {

//! Records each detach hook it receives, in call order. Its public virtuals are
//! trivial: the tests drive only the non-virtual detachModels() and the guard.
class FakeGuiFrontEnd : public GuiFrontEnd
{
public:
    std::vector<std::string> calls;

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

protected:
    void hideMain() override { calls.emplace_back("hideMain"); }
    void detachClient() override { calls.emplace_back("detachClient"); }
    void detachWallet() override { calls.emplace_back("detachWallet"); }
    void detachMRC() override { calls.emplace_back("detachMRC"); }
    void detachResearcher() override { calls.emplace_back("detachResearcher"); }
    void detachVoting() override { calls.emplace_back("detachVoting"); }
    void detachPSGT() override { calls.emplace_back("detachPSGT"); }
};

//! Compare the recorded hooks element by element, so a failure names the first
//! hook that arrived out of order.
void CompareCalls(const std::vector<std::string>& calls, const std::vector<std::string>& expected)
{
    QCOMPARE(calls.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        QCOMPARE(QString::fromStdString(calls[i]), QString::fromStdString(expected[i]));
    }
}

} // namespace

void GUIFrontEndTests::detachSequence()
{
    FakeGuiFrontEnd fake;

    fake.detachModels();

    const std::vector<std::string> expected{
        "hideMain", "detachClient", "detachWallet", "detachMRC",
        "detachResearcher", "detachVoting", "detachPSGT"};
    CompareCalls(fake.calls, expected);
    if (QTest::currentTestFailed()) return;

    // Idempotent: the normal-path call and the exception-path guard can both run.
    fake.detachModels();
    CompareCalls(fake.calls, expected);
}

void GUIFrontEndTests::guardRunsDetachOnThrow()
{
    FakeGuiFrontEnd fake;

    try {
        FrontEndDetachGuard guard{fake};
        throw std::runtime_error("x");
    } catch (const std::runtime_error&) {
    }

    QCOMPARE(fake.calls.size(), size_t{7});
    QCOMPARE(QString::fromStdString(fake.calls.front()), QStringLiteral("hideMain"));
}

void GUIFrontEndTests::bridgeSlotSignatures()
{
    // These are the slots bitcoin.cpp's bridges invoke by name through
    // QMetaObject::invokeMethod. A rename or a parameter-type change compiles
    // cleanly and only fails at run time, so pin them here. A second front
    // end's targets must provide the same slots.
    const QMetaObject& gui = BitcoinGUI::staticMetaObject;
    for (const char* slot : {"error(QString,QString,bool)",
                             "update(QString,QString,int,QString)",
                             "handleURI(QString)"}) {
        QVERIFY2(gui.indexOfSlot(QMetaObject::normalizedSignature(slot)) >= 0, slot);
    }

    const QMetaObject& splash = SplashScreen::staticMetaObject;
    QVERIFY2(splash.indexOfSlot(QMetaObject::normalizedSignature("showMessage(QString,int)")) >= 0,
             "showMessage(QString,int)");
}
