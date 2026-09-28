// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/guifrontendtests.h"

#include "qt/bitcoingui.h"
#include "qt/guifrontend.h"
#include "qt/splashscreen.h"

#include <QMetaObject>
#include <QString>

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

//!
//! \file guifrontendtests.cpp
//! \brief The GuiFrontEnd seam's teardown contract (the detach order, its
//! idempotence, its resumption after a hook throws and the exception-path
//! guard) and the slot signatures the string-invoked core bridges in
//! bitcoin.cpp depend on.
//!

namespace {

//! Records each detach hook it receives, in call order. Its public virtuals are
//! trivial: the tests drive only the non-virtual detachModels() and the guard.
class FakeGuiFrontEnd : public GuiFrontEnd
{
public:
    std::vector<std::string> calls;
    //! Hooks named here record their call and then throw std::runtime_error, on
    //! their first call only.
    std::set<std::string> throw_in;

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
    void hideMain() override { Record("hideMain"); }
    void detachClient() override { Record("detachClient"); }
    void detachWallet() override { Record("detachWallet"); }
    void detachMRC() override { Record("detachMRC"); }
    void detachResearcher() override { Record("detachResearcher"); }
    void detachVoting() override { Record("detachVoting"); }
    void detachPSGT() override { Record("detachPSGT"); }

private:
    void Record(const std::string& name)
    {
        calls.emplace_back(name);
        if (throw_in.erase(name) > 0) throw std::runtime_error(name + " threw");
    }
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

void GUIFrontEndTests::detachResumesAfterThrowingHook()
{
    FakeGuiFrontEnd fake;
    fake.throw_in = {"detachClient"};

    {
        FrontEndDetachGuard guard{fake};
        bool threw = false;
        size_t calls_at_throw = 0;
        try {
            fake.detachModels();
        } catch (const std::runtime_error&) {
            // detachClient threw out of the explicit call. Leaving the block runs
            // the guard, which must resume at the next hook.
            threw = true;
            calls_at_throw = fake.calls.size();
        }
        // detachModels() lets a hook's exception propagate to its caller.
        QVERIFY(threw);
        // The explicit call stopped at the hook that threw: only hideMain and
        // detachClient ran before the exception left it.
        QCOMPARE(calls_at_throw, size_t{2});
    }

    CompareCalls(fake.calls, {"hideMain", "detachClient", "detachWallet", "detachMRC",
                              "detachResearcher", "detachVoting", "detachPSGT"});
}

void GUIFrontEndTests::guardRunsEveryHookPastTwoThrows()
{
    FakeGuiFrontEnd fake;
    fake.throw_in = {"detachClient", "detachResearcher"};

    try {
        FrontEndDetachGuard guard{fake};
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error&) {
    }

    // The guard alone, during unwinding, gets past both throwing hooks.
    CompareCalls(fake.calls, {"hideMain", "detachClient", "detachWallet", "detachMRC",
                              "detachResearcher", "detachVoting", "detachPSGT"});
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
