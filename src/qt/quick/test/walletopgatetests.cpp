// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "walletopgatetests.h"

#include "interfacefakes_qml.h"
#include "qmltestutil.h"

#include <chainparams.h>
#include <interfaces/wallet.h>
#include <interfaces/wallet_coin_source.h>
#include <key_io.h>
#include <pubkey.h>
#include <qt/optionsmodel.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/sendadapter.h>
#include <qt/quick/walletopgate.h>
#include <qt/quick/walletpromptlatch.h>
#include <qt/test/interfacefakes.h>
#include <qt/test/modelfakes.h>
#include <qt/walletmodel.h>
#include <uint256.h>
#include <util.h>

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using Method = qt_test::ScriptedWallet::Method;
using Owner = WalletPromptLatch::Owner;

//! The carried root's URL; these front ends never construct, so it is never
//! loaded.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

const std::string PASSPHRASE = "correct horse";

//! Posts a no-op on the serial lane and pumps until it is delivered.
bool SerialProbe(QmlCallRunner& runner)
{
    auto delivered = std::make_shared<bool>(false);
    if (!runner.postSerial([] {}, [delivered] { *delivered = true; })) return false;
    return qml_test::PumpUntil([delivered] { return *delivered; }, 2s);
}

//! Waits, without processing events, until `done()` holds or `bound` passes;
//! returns done().
bool WaitNoEvents(const std::function<bool()>& done, std::chrono::milliseconds bound)
{
    const auto deadline = std::chrono::steady_clock::now() + bound;
    while (std::chrono::steady_clock::now() < deadline) {
        if (done()) return true;
        std::this_thread::sleep_for(1ms);
    }
    return done();
}

//! Posts a no-op on the serial lane and waits, without processing events,
//! until it has run: every serial call posted before it has returned.
bool SerialProbeNoEvents(QmlCallRunner& runner)
{
    auto ran = std::make_shared<std::atomic<bool>>(false);
    if (!runner.postSerial([ran] { *ran = true; }, {})) return false;
    return WaitNoEvents([ran] { return ran->load(); }, 2s);
}

//! The logged calls of the methods in `methods`, in log order.
std::vector<Method> LogOrder(const qt_test::ScriptedWallet& wallet, std::initializer_list<Method> methods)
{
    std::vector<Method> order;
    for (const qt_test::ScriptedWallet::Call& call : wallet.log()) {
        for (Method method : methods) {
            if (call.method == method) order.push_back(call.method);
        }
    }
    return order;
}

//! The logged calls of `method`, in order.
std::vector<qt_test::ScriptedWallet::Call> CallsOf(const qt_test::ScriptedWallet& wallet, Method method)
{
    std::vector<qt_test::ScriptedWallet::Call> calls;
    for (const qt_test::ScriptedWallet::Call& call : wallet.log()) {
        if (call.method == method) calls.push_back(call);
    }
    return calls;
}

//! Posts the op: a closure that logs Op on the wallet it runs against.
bool PostOp(WalletOpGate& gate, quint64 id)
{
    return gate.post(
        id, [](interfaces::Wallet& wallet) { static_cast<qt_test::ScriptedWallet&>(wallet).recordOp(); }, {});
}

//! The ids the gate granted and refused.
struct Callbacks {
    std::vector<quint64> granted;
    std::vector<std::pair<quint64, WalletOpGate::Result>> refused;
};

//! Records the grant only.
WalletOpGate::Granted RecordGrant(Callbacks& calls)
{
    return [&calls](quint64 id) { calls.granted.push_back(id); };
}

//! Records the grant and posts the op.
WalletOpGate::Granted RecordGrantAndOp(WalletOpGate& gate, Callbacks& calls)
{
    return [&gate, &calls](quint64 id) {
        calls.granted.push_back(id);
        PostOp(gate, id);
    };
}

WalletOpGate::Refused RecordRefusal(Callbacks& calls)
{
    return [&calls](quint64 id, WalletOpGate::Result result) { calls.refused.emplace_back(id, result); };
}

//! Spies on every signal of the gate.
struct Spies {
    explicit Spies(WalletOpGate& gate)
        : requested(&gate, &WalletOpGate::unlockRequested),
          failed(&gate, &WalletOpGate::unlockFailed),
          accepted(&gate, &WalletOpGate::unlockAccepted),
          finished(&gate, &WalletOpGate::finished),
          busy(&gate, &WalletOpGate::busyChanged)
    {
    }
    QSignalSpy requested;
    QSignalSpy failed;
    QSignalSpy accepted;
    QSignalSpy finished;
    QSignalSpy busy;
};

quint64 IdOf(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(0).toULongLong();
}

QString UnlockTitle()
{
    return QCoreApplication::translate("AskPassphraseDialog", "Unlock wallet");
}

QString UnlockText()
{
    return QCoreApplication::translate("AskPassphraseDialog", "This operation needs your wallet passphrase. The wallet is unlocked for the operation and returns to its previous state afterwards.");
}

QString WrongText()
{
    return QCoreApplication::translate("AskPassphraseDialog",
                                       "The passphrase entered for the wallet decryption was incorrect.");
}

QString WrongNulText()
{
    return QCoreApplication::translate("AskPassphraseDialog",
                                       "The passphrase entered for the wallet decryption is incorrect. "
                                       "It contains a null character (ie - a zero byte). "
                                       "If the passphrase was set with a version of this software prior to 5.4.6, "
                                       "please try again with only the characters up to — but not including — "
                                       "the first null character. If this is successful, please set a new "
                                       "passphrase to avoid this issue in the future.");
}

QString ExpiredText()
{
    return QCoreApplication::translate("AskPassphraseDialog",
                                       "The wallet locked itself while you were entering your "
                                       "passphrase, because the unlock you had asked for ran out. "
                                       "Unlock the wallet and try again.");
}

QString Pass()
{
    return QString::fromStdString(PASSPHRASE);
}

QString MethodName(Method method)
{
    switch (method) {
    case Method::GetLockState: return QStringLiteral("getLockState");
    case Method::UnlockWallet: return QStringLiteral("unlockWallet");
    case Method::ElevateWallet: return QStringLiteral("elevateWallet");
    case Method::LockWallet: return QStringLiteral("lockWallet");
    case Method::RestrictToStakingOnly: return QStringLiteral("restrictToStakingOnly");
    case Method::SendCoins: return QStringLiteral("sendCoins");
    case Method::Op: return QStringLiteral("op");
    }
    return QString();
}

//! A valid address of the selected chain, distinct for each `b`.
QString Address(unsigned char b)
{
    return QString::fromStdString(EncodeDestination(CKeyID(uint160(std::vector<unsigned char>(20, b)))));
}

interfaces::SendCoinsResult SendResult(interfaces::SendCoinsStatus status, int64_t fee = 0)
{
    interfaces::SendCoinsResult result;
    result.status = status;
    result.fee = fee;
    if (status == interfaces::SendCoinsStatus::OK) result.txid_hex = "abcdef";
    return result;
}

} // namespace

void QmlWalletOpGateTests::initTestCase()
{
    // The classes that build OptionsModel by hand keep its settings out of
    // the user's store, as the Qt tests' OptionsModelTests does.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName(QStringLiteral("GridcoinTest"));
    QCoreApplication::setApplicationName(QStringLiteral("walletopgatetests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());

    SelectParams(CBaseChainParams::REGTEST);
}

void QmlWalletOpGateTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void QmlWalletOpGateTests::init()
{
    // OptionsModel::Init reaches GetDataDir(false), which creates a datadir:
    // point -datadir at a temporary directory.
    m_prev_datadir_set = gArgs.IsArgSet("-datadir");
    m_prev_datadir = gArgs.GetArg("-datadir", "");
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void QmlWalletOpGateTests::cleanup()
{
    QSettings().clear();
    if (m_prev_datadir_set) {
        gArgs.ForceSetArg("-datadir", m_prev_datadir);
    } else {
        gArgs.ClearForcedArg("-datadir");
    }
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

void QmlWalletOpGateTests::lockedUnlockRunsOpAndRelocks()
{
    const std::thread::id gui = std::this_thread::get_id();
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(gate.busy());
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QCOMPARE(IdOf(spies.requested, 0), id);
    QCOMPARE(spies.requested.at(0).at(1).value<WalletOpGate::Mode>(), WalletOpGate::Unlock);
    QCOMPARE(spies.requested.at(0).at(2).toString(), UnlockTitle());
    QCOMPARE(spies.requested.at(0).at(3).toString(), UnlockText());
    QCOMPARE(wallet.opCalls(), 0);

    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 1; }, 2s));
    QCOMPARE(spies.accepted.count(), 1);
    QCOMPARE(calls.granted, std::vector<quint64>{id});
    const std::vector<qt_test::ScriptedWallet::Call> unlocks = CallsOf(wallet, Method::UnlockWallet);
    QCOMPARE(unlocks.size(), size_t{1});
    QVERIFY(!unlocks[0].staking_only);
    QCOMPARE(unlocks[0].passphrase, PASSPHRASE);
    QVERIFY(unlocks[0].thread != gui);

    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(wallet.lockCalls(), 1);
    QCOMPARE(LogOrder(wallet, {Method::UnlockWallet, Method::Op, Method::LockWallet}),
             (std::vector<Method>{Method::UnlockWallet, Method::Op, Method::LockWallet}));
    QCOMPARE(spies.finished.count(), 1);
    QCOMPARE(IdOf(spies.finished, 0), id);
    QCOMPARE(spies.finished.at(0).at(1).value<WalletOpGate::Result>(), WalletOpGate::Completed);
    QVERIFY(!gate.busy());
    QVERIFY(calls.refused.empty());
}

void QmlWalletOpGateTests::stakingOnlyElevatesAndNarrows()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, false, true);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QCOMPARE(spies.requested.at(0).at(1).value<WalletOpGate::Mode>(), WalletOpGate::Elevate);
    QCOMPARE(spies.requested.at(0).at(2).toString(), UnlockTitle());
    QCOMPARE(spies.requested.at(0).at(3).toString(), UnlockText());

    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 1; }, 2s));
    QCOMPARE(wallet.elevateCalls(), 1);
    QCOMPARE(wallet.unlockCalls(), 0);

    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(wallet.restrictCalls(), 1);
    QCOMPARE(wallet.lockCalls(), 0);
    QCOMPARE(LogOrder(wallet, {Method::ElevateWallet, Method::Op, Method::RestrictToStakingOnly}),
             (std::vector<Method>{Method::ElevateWallet, Method::Op, Method::RestrictToStakingOnly}));
}

void QmlWalletOpGateTests::beginReturnsBeforeTheRead()
{
    qml_test::Watchdog watchdog("QmlWalletOpGateTests::beginReturnsBeforeTheRead");
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    qt_test::FakeLatch read_latch;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    qml_test::ScopeExit unlatch([&] {
        read_latch.open = true;
        wallet.m_read_latch = nullptr;
    });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    wallet.m_read_latch = &read_latch;
    std::thread opener([&read_latch] {
        std::this_thread::sleep_for(2s);
        read_latch.open = true;
    });
    qml_test::ScopeExit join_opener([&opener] { opener.join(); });

    const auto start = std::chrono::steady_clock::now();
    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    QVERIFY(id != 0);
    QVERIFY2(elapsed < 100ms, qPrintable(QStringLiteral("begin() took %1 ms").arg(elapsed.count())));
    QCOMPARE(spies.requested.count(), 0);

    // The request follows the read, which follows the latch.
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 5s));
    QVERIFY(read_latch.open);
    QCOMPARE(IdOf(spies.requested, 0), id);
}

void QmlWalletOpGateTests::cancelBeforeAnswer()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));

    QVERIFY(gate.cancelUnlock(id));
    QVERIFY(qml_test::PumpUntil([&] { return spies.finished.count() == 1; }, 2s));
    QCOMPARE(IdOf(spies.finished, 0), id);
    QCOMPARE(spies.finished.at(0).at(1).value<WalletOpGate::Result>(), WalletOpGate::Cancelled);
    QCOMPARE(calls.refused.size(), size_t{1});
    QCOMPARE(calls.refused[0].first, id);
    QCOMPARE(calls.refused[0].second, WalletOpGate::Cancelled);
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(wallet.unlockCalls(), 0);
    QCOMPARE(wallet.elevateCalls(), 0);
    QCOMPARE(wallet.opCalls(), 0);
    QVERIFY(!gate.busy());
}

void QmlWalletOpGateTests::disconnectDuringUnlock()
{
    const std::thread::id gui = std::this_thread::get_id();

    // The unlock call itself is cut off: the disconnect is counted, nothing is
    // rethrown, no failure is shown, and detach has nothing to restore.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setPassphrase(PASSPHRASE);
        wallet.setLockState(true, true, false);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        WalletOpGate& gate = fe.provider().walletOps();
        gate.attach(wallet, fe.callRunner());
        Spies spies(gate);
        Callbacks calls;

        const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
        wallet.setThrow(Method::UnlockWallet, qt_test::ScriptedWallet::ThrowKind::Runtime,
                        "IPC call interrupted by disconnect");
        QVERIFY(gate.answerUnlock(id, Pass()));
        QVERIFY(qml_test::PumpUntil([&] { return disconnects.load() == 1; }, 2s));
        QVERIFY(SerialProbe(fe.callRunner()));
        QCOMPARE(disconnects.load(), 1);
        QVERIFY(qml_test::g_rethrown.empty());
        QCOMPARE(spies.failed.count(), 0);
        QCOMPARE(spies.accepted.count(), 0);

        fe.detachModels();
        QCOMPARE(wallet.lockCalls(), 0);
        QCOMPARE(wallet.restrictCalls(), 0);
    }

    // The operation is cut off after a successful unlock: the wallet is
    // relocked on the worker before the disconnect is classified, and detach
    // makes no second restore.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setPassphrase(PASSPHRASE);
        wallet.setLockState(true, true, false);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        WalletOpGate& gate = fe.provider().walletOps();
        gate.attach(wallet, fe.callRunner());
        Spies spies(gate);
        Callbacks calls;

        const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
        QVERIFY(gate.answerUnlock(id, Pass()));
        QVERIFY(qml_test::PumpUntil([&] { return calls.granted.size() == 1; }, 2s));
        QVERIFY(gate.post(
            id, [](interfaces::Wallet&) { throw std::runtime_error("IPC call interrupted by disconnect"); }, {}));
        QVERIFY(qml_test::PumpUntil([&] { return disconnects.load() == 1; }, 2s));
        QVERIFY(SerialProbe(fe.callRunner()));
        const int locks_before_detach = wallet.lockCalls();

        fe.detachModels();
        const std::vector<qt_test::ScriptedWallet::Call> locks = CallsOf(wallet, Method::LockWallet);
        QCOMPARE(locks.size(), size_t{1});
        QVERIFY2(locks[0].thread != gui, "the wallet was relocked on the GUI thread");
        QCOMPARE(locks_before_detach, 1);
        QVERIFY(qml_test::g_rethrown.empty());
    }
}

void QmlWalletOpGateTests::secondBeginIsBusy()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;
    Callbacks second_calls;

    // While the first request waits for its answer.
    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QCOMPARE(gate.begin(RecordGrant(second_calls), RecordRefusal(second_calls)), quint64{0});
    qml_test::PumpFor(100ms);
    QCOMPARE(spies.requested.count(), 1);

    // Granted, with no op posted yet and no end.
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return calls.granted.size() == 1; }, 2s));
    QCOMPARE(gate.begin(RecordGrant(second_calls), RecordRefusal(second_calls)), quint64{0});

    QVERIFY(PostOp(gate, id));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(LogOrder(wallet, {Method::UnlockWallet, Method::Op, Method::LockWallet}),
             (std::vector<Method>{Method::UnlockWallet, Method::Op, Method::LockWallet}));
    QCOMPARE(wallet.lockCalls(), 1);
    QVERIFY(second_calls.granted.empty());
    QVERIFY(second_calls.refused.empty());
}

void QmlWalletOpGateTests::modeFromOneRead()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, false, true);
    // The read sees the wallet unlocked for staking; it locks right after.
    interfaces::WalletLockState locked;
    locked.crypted = true;
    locked.locked = true;
    wallet.flipAfterNextRead(locked);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QCOMPARE(spies.requested.at(0).at(1).value<WalletOpGate::Mode>(), WalletOpGate::Elevate);
    QVERIFY(wallet.lockState().locked);

    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return spies.failed.count() + spies.accepted.count() == 1; }, 2s));
    QCOMPARE(wallet.unlockCalls(), 0);
    QCOMPARE(wallet.elevateCalls(), 1);
    QCOMPARE(wallet.opCalls(), 0);
}

void QmlWalletOpGateTests::wrongPassphraseKeepsRequest()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));

    QVERIFY(gate.answerUnlock(id, QStringLiteral("wrong")));
    QVERIFY(qml_test::PumpUntil([&] { return spies.failed.count() == 1; }, 2s));
    QCOMPARE(IdOf(spies.failed, 0), id);
    QCOMPARE(spies.failed.at(0).at(1).value<WalletOpGate::Failure>(), WalletOpGate::WrongPassphrase);
    QCOMPARE(spies.failed.at(0).at(2).toString(), WrongText());
    QCOMPARE(wallet.opCalls(), 0);
    QCOMPARE(spies.finished.count(), 0);

    // The request still waits for an answer.
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 1; }, 2s));
    QCOMPARE(spies.accepted.count(), 1);
    QCOMPARE(IdOf(spies.accepted, 0), id);
    QCOMPARE(wallet.unlockCalls(), 2);
}

void QmlWalletOpGateTests::wrongPassphraseWithZeroByte()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));

    const QString with_zero = QStringLiteral("ab") + QString(QChar(0)) + QStringLiteral("cd");
    QVERIFY(gate.answerUnlock(id, with_zero));
    QVERIFY(qml_test::PumpUntil([&] { return spies.failed.count() == 1; }, 2s));
    QCOMPARE(spies.failed.at(0).at(1).value<WalletOpGate::Failure>(), WalletOpGate::WrongPassphraseNul);
    QCOMPARE(spies.failed.at(0).at(2).toString(), WrongNulText());
    const std::vector<qt_test::ScriptedWallet::Call> unlocks = CallsOf(wallet, Method::UnlockWallet);
    QCOMPARE(unlocks.size(), size_t{1});
    QCOMPARE(unlocks[0].passphrase, std::string("ab\0cd", 5));
}

void QmlWalletOpGateTests::elevateExpiredNeverUnlocks()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, false, true);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QCOMPARE(spies.requested.at(0).at(1).value<WalletOpGate::Mode>(), WalletOpGate::Elevate);

    // The unlock for staking runs out while the prompt is open.
    wallet.setLockState(true, true, false);
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return spies.failed.count() == 1; }, 2s));
    QCOMPARE(IdOf(spies.failed, 0), id);
    QCOMPARE(spies.failed.at(0).at(1).value<WalletOpGate::Failure>(), WalletOpGate::ExpiredDuringPrompt);
    QCOMPARE(spies.failed.at(0).at(2).toString(), ExpiredText());
    QCOMPARE(wallet.unlockCalls(), 0);
    QCOMPARE(wallet.elevateCalls(), 1);
    QCOMPARE(wallet.opCalls(), 0);
    QVERIFY(wallet.lockState().locked);
}

void QmlWalletOpGateTests::noStakingFlag()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    // A locked wallet unlocked for an operation.
    quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 1; }, 2s));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));

    // An unlock for staking widened for an operation.
    wallet.setLockState(true, false, true);
    id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 2; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 2; }, 2s));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));

    // A wrong passphrase, then the right one.
    wallet.setLockState(true, true, false);
    id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 3; }, 2s));
    QVERIFY(gate.answerUnlock(id, QStringLiteral("wrong")));
    QVERIFY(qml_test::PumpUntil([&] { return spies.failed.count() == 1; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 3; }, 2s));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));

    const std::vector<qt_test::ScriptedWallet::Call> unlocks = CallsOf(wallet, Method::UnlockWallet);
    QCOMPARE(unlocks.size(), size_t{3});
    for (const qt_test::ScriptedWallet::Call& call : unlocks) {
        QVERIFY2(!call.staking_only, "an operation's unlock asked for staking only");
    }
    QCOMPARE(wallet.elevateCalls(), 1);
}

void QmlWalletOpGateTests::pooledLaneDoesNotBlockGate()
{
    qml_test::Watchdog watchdog("QmlWalletOpGateTests::pooledLaneDoesNotBlockGate");
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    qml_test::Latch pool_latch;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    qml_test::ScopeExit unlatch([&pool_latch] {
        pool_latch.open = true;
        QThreadPool::globalInstance()->waitForDone();
    });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    // Every pooled thread held.
    const int threads = QThreadPool::globalInstance()->maxThreadCount();
    for (int i = 0; i < threads; ++i) {
        QVERIFY(fe.callRunner().postPooled([&pool_latch] { pool_latch.wait(); }, {}));
    }

    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY2(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s),
             "the gate's read waited for the pooled lane");

    pool_latch.open = true;
    QThreadPool::globalInstance()->waitForDone();
}

void QmlWalletOpGateTests::staleIdRefused()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QVERIFY(gate.cancelUnlock(id));
    QCOMPARE(spies.finished.count(), 1);

    // The finished request answers nothing.
    QVERIFY(!gate.answerUnlock(id, Pass()));
    QVERIFY(!gate.cancelUnlock(id));
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(wallet.unlockCalls(), 0);

    // Nor does it answer for the next request.
    const quint64 id2 = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id2 != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 2; }, 2s));
    QCOMPARE(IdOf(spies.requested, 1), id2);
    QVERIFY(id2 != id);
    QVERIFY(!gate.answerUnlock(id, Pass()));
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(wallet.unlockCalls(), 0);
    QVERIFY(calls.granted.empty());
}

void QmlWalletOpGateTests::heldLeaseReleasedAtDetach()
{
    // Granted, an op run, and no end: detach relocks.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setPassphrase(PASSPHRASE);
        wallet.setLockState(true, true, false);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        WalletOpGate& gate = fe.provider().walletOps();
        gate.attach(wallet, fe.callRunner());
        Spies spies(gate);
        Callbacks calls;

        const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
        QVERIFY(gate.answerUnlock(id, Pass()));
        QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 1; }, 2s));
        QVERIFY(SerialProbe(fe.callRunner()));
        QCOMPARE(wallet.lockCalls(), 0);

        fe.detachModels();
        QCOMPARE(wallet.lockCalls(), 1);
        QVERIFY(!gate.busy());
    }

    // Ended and restored already: detach restores nothing more.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setPassphrase(PASSPHRASE);
        wallet.setLockState(true, true, false);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        WalletOpGate& gate = fe.provider().walletOps();
        gate.attach(wallet, fe.callRunner());
        Spies spies(gate);
        Callbacks calls;

        const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
        QVERIFY(gate.answerUnlock(id, Pass()));
        QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 1; }, 2s));
        gate.end(id);
        QVERIFY(SerialProbe(fe.callRunner()));
        QCOMPARE(wallet.lockCalls(), 1);

        fe.detachModels();
        QCOMPARE(wallet.lockCalls(), 1);
    }
}

void QmlWalletOpGateTests::restoreRunsAfterLastOp()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return calls.granted.size() == 1; }, 2s));

    // The first op takes a while, so both ops are still queued or running
    // when end() is called.
    QVERIFY(gate.post(
        id,
        [](interfaces::Wallet& w) {
            std::this_thread::sleep_for(100ms);
            static_cast<qt_test::ScriptedWallet&>(w).recordOp();
        },
        {}));
    QVERIFY(PostOp(gate, id));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(LogOrder(wallet, {Method::Op, Method::LockWallet}),
             (std::vector<Method>{Method::Op, Method::Op, Method::LockWallet}));
}

void QmlWalletOpGateTests::droppedReleaseIsMadeUp()
{
    qml_test::Watchdog watchdog("QmlWalletOpGateTests::droppedReleaseIsMadeUp");
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    qml_test::Latch op_latch;
    std::atomic<bool> op_started{false};
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    qml_test::ScopeExit unlatch([&op_latch] { op_latch.open = true; });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return calls.granted.size() == 1; }, 2s));

    // The op holds the serial lane, so end()'s release is queued behind it;
    // the moved generation then drops the release.
    QVERIFY(gate.post(
        id,
        [&op_latch, &op_started](interfaces::Wallet& w) {
            op_started = true;
            op_latch.wait();
            static_cast<qt_test::ScriptedWallet&>(w).recordOp();
        },
        {}));
    QVERIFY(WaitNoEvents([&] { return op_started.load(); }, 2s));
    gate.end(id);
    QCOMPARE(spies.finished.count(), 1);
    fe.callRunner().bumpGeneration();
    op_latch.open = true;
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(wallet.opCalls(), 1);
    QCOMPARE(wallet.lockCalls(), 0);

    // The next request makes the release up before its own read, so the read
    // finds the wallet locked again.
    const quint64 id2 = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id2 != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 2 || calls.granted.size() == 2; }, 2s));
    QCOMPARE(LogOrder(wallet, {Method::GetLockState, Method::LockWallet}),
             (std::vector<Method>{Method::GetLockState, Method::LockWallet, Method::GetLockState}));
    QCOMPARE(spies.requested.count(), 2);
    QCOMPARE(IdOf(spies.requested, 1), id2);
    QCOMPARE(spies.requested.at(1).at(1).value<WalletOpGate::Mode>(), WalletOpGate::Unlock);

    fe.detachModels();
    QCOMPARE(wallet.lockCalls(), 1);
}

void QmlWalletOpGateTests::cancelDuringRunningAnswer()
{
    qml_test::Watchdog watchdog("QmlWalletOpGateTests::cancelDuringRunningAnswer");
    const std::thread::id gui = std::this_thread::get_id();
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    qt_test::FakeLatch unlock_latch;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    qml_test::ScopeExit unlatch([&] {
        unlock_latch.open = true;
        wallet.m_unlock_latch = nullptr;
    });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));

    wallet.m_unlock_latch = &unlock_latch;
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(WaitNoEvents([&] { return wallet.unlockCalls() == 1; }, 2s));
    QVERIFY(gate.cancelUnlock(id));
    auto probe_ran = std::make_shared<std::atomic<bool>>(false);
    QVERIFY(fe.callRunner().postSerial([probe_ran] { *probe_ran = true; }, {}));
    unlock_latch.open = true;

    // The worker read the cancel and gave the scope back itself: the relock
    // has run by the time the probe behind the answer has, before any event
    // is processed.
    QVERIFY(WaitNoEvents([probe_ran] { return probe_ran->load(); }, 2s));
    QCOMPARE(wallet.lockCalls(), 1);

    QVERIFY(qml_test::PumpUntil([&] { return spies.finished.count() == 1; }, 2s));
    QCOMPARE(wallet.unlockCalls(), 1);
    QCOMPARE(wallet.lockCalls(), 1);
    QVERIFY(CallsOf(wallet, Method::LockWallet)[0].thread != gui);
    QCOMPARE(wallet.opCalls(), 0);
    QCOMPARE(spies.accepted.count(), 0);
    QCOMPARE(IdOf(spies.finished, 0), id);
    QCOMPARE(spies.finished.at(0).at(1).value<WalletOpGate::Result>(), WalletOpGate::Cancelled);
    QCOMPARE(calls.refused.size(), size_t{1});
    QVERIFY(!gate.busy());
}

void QmlWalletOpGateTests::cancelDuringFailingAnswer()
{
    qml_test::Watchdog watchdog("QmlWalletOpGateTests::cancelDuringFailingAnswer");
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    qt_test::FakeLatch unlock_latch;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    qml_test::ScopeExit unlatch([&] {
        unlock_latch.open = true;
        wallet.m_unlock_latch = nullptr;
    });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));

    wallet.m_unlock_latch = &unlock_latch;
    QVERIFY(gate.answerUnlock(id, QStringLiteral("wrong")));
    QVERIFY(WaitNoEvents([&] { return wallet.unlockCalls() == 1; }, 2s));
    QVERIFY(gate.cancelUnlock(id));
    unlock_latch.open = true;

    QVERIFY2(qml_test::PumpUntil([&] { return spies.finished.count() == 1; }, 2s),
             "a cancel during a failing answer did not end the request");
    QCOMPARE(spies.finished.at(0).at(1).value<WalletOpGate::Result>(), WalletOpGate::Cancelled);
    QCOMPARE(spies.failed.count(), 0);
    QVERIFY(!gate.busy());
    QVERIFY(gate.begin(RecordGrant(calls), RecordRefusal(calls)) != 0);
    QVERIFY(SerialProbe(fe.callRunner()));
    QCOMPARE(wallet.lockCalls(), 0);
}

void QmlWalletOpGateTests::cancelAfterWorkerReadFlag()
{
    qml_test::Watchdog watchdog("QmlWalletOpGateTests::cancelAfterWorkerReadFlag");
    const std::thread::id gui = std::this_thread::get_id();
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));

    // The answer's call has returned on the worker, having read no cancel;
    // its delivery waits in the event queue.
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(SerialProbeNoEvents(fe.callRunner()));
    QCOMPARE(wallet.unlockCalls(), 1);
    QVERIFY(gate.cancelUnlock(id));

    qml_test::PumpUntil([&] { return spies.finished.count() == 1 && wallet.lockCalls() == 1; }, 2s);
    QCOMPARE(spies.accepted.count(), 0);
    QCOMPARE(wallet.opCalls(), 0);
    QCOMPARE(spies.finished.count(), 1);
    QCOMPARE(IdOf(spies.finished, 0), id);
    QCOMPARE(spies.finished.at(0).at(1).value<WalletOpGate::Result>(), WalletOpGate::Cancelled);
    QVERIFY(!gate.busy());
    const std::vector<qt_test::ScriptedWallet::Call> locks = CallsOf(wallet, Method::LockWallet);
    QCOMPARE(locks.size(), size_t{1});
    QVERIFY2(locks[0].thread != gui, "the cancelled answer's wallet was relocked on the GUI thread");
}

void QmlWalletOpGateTests::promptLatchShared()
{
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;
    WalletPromptLatch& latch = fe.provider().walletPromptLatch();

    // Another passphrase prompt holds the latch: the gate asks nothing.
    QVERIFY(latch.tryAcquire(Owner::Security));
    QCOMPARE(gate.begin(RecordGrant(calls), RecordRefusal(calls)), quint64{0});
    qml_test::PumpFor(100ms);
    QCOMPARE(spies.requested.count(), 0);
    QCOMPARE(wallet.lockStateCalls(), 0);
    QVERIFY(!gate.busy());
    latch.release(Owner::Security);
    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);

    // A gate request holds it against the other prompts until it finishes.
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QVERIFY(!latch.tryAcquire(Owner::Security));
    QVERIFY(gate.cancelUnlock(id));
    QCOMPARE(spies.finished.count(), 1);
    QVERIFY(latch.tryAcquire(Owner::Security));
    latch.release(Owner::Security);

    // Another owner's release does not free the gate's hold.
    QVERIFY(gate.begin(RecordGrant(calls), RecordRefusal(calls)) != 0);
    latch.release(Owner::Security);
    QVERIFY(latch.busy());
    QVERIFY(latch.heldBy(Owner::WalletOps));
    QVERIFY(gate.busy());
}

void QmlWalletOpGateTests::throwingOpReleasesBeforeFatal()
{
    QVERIFY2(qml_test::g_rethrown.empty(), "an earlier test left a rethrown exception");
    qml_test::ScopeExit clear_rethrown([] { qml_test::g_rethrown.clear(); });
    const std::thread::id gui = std::this_thread::get_id();
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    Spies spies(gate);
    Callbacks calls;

    const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return calls.granted.size() == 1; }, 2s));

    QVERIFY(gate.post(id, [](interfaces::Wallet&) { throw std::runtime_error("boom"); }, {}));
    QVERIFY(qml_test::PumpUntil([] { return qml_test::g_rethrown.size() == 1; }, 2s));
    QCOMPARE(wallet.lockCalls(), 1);
    QVERIFY(CallsOf(wallet, Method::LockWallet)[0].thread != gui);
    QCOMPARE(QString::fromStdString(qml_test::g_rethrown.front().first), QStringLiteral("boom"));
    QCOMPARE(disconnects.load(), 0);
}

void QmlWalletOpGateTests::walletCallsOffGuiThread()
{
    qml_test::Watchdog watchdog("QmlWalletOpGateTests::walletCallsOffGuiThread");
    const std::thread::id gui = std::this_thread::get_id();
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager side_stakes;
    OptionsModel options(node, side_stakes);
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source = interfaces::MakeSyntheticCoinSource(0, 1);
    WalletModel model(wallet, tx_source, *coin_source, &options);
    qt_test::FakeLatch unlock_latch;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    qml_test::ScopeExit unlatch([&] {
        unlock_latch.open = true;
        wallet.m_unlock_latch = nullptr;
    });
    WalletOpGate& gate = fe.provider().walletOps();
    gate.attach(wallet, fe.callRunner());
    SendAdapter& adapter = fe.provider().sendAdapter();
    adapter.attach(model, nullptr, fe.callRunner());
    Spies spies(gate);
    QSignalSpy sent(&adapter, &SendAdapter::finished);
    QSignalSpy fee(&adapter, &SendAdapter::feeConfirmationRequested);
    Callbacks calls;

    // A locked wallet unlocked for an operation.
    quint64 id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 1; }, 2s));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));

    // An unlock for staking widened for an operation.
    wallet.setLockState(true, false, true);
    id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 2; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 2; }, 2s));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));

    // A wrong passphrase, then the right one.
    wallet.setLockState(true, true, false);
    id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 3; }, 2s));
    QVERIFY(gate.answerUnlock(id, QStringLiteral("wrong")));
    QVERIFY(qml_test::PumpUntil([&] { return spies.failed.count() == 1; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.opCalls() == 3; }, 2s));
    gate.end(id);
    QVERIFY(SerialProbe(fe.callRunner()));

    // Cancelled while the answer runs.
    id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 4; }, 2s));
    const int unlocks_before = wallet.unlockCalls();
    wallet.m_unlock_latch = &unlock_latch;
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(WaitNoEvents([&] { return wallet.unlockCalls() == unlocks_before + 1; }, 2s));
    QVERIFY(gate.cancelUnlock(id));
    unlock_latch.open = true;
    QVERIFY(qml_test::PumpUntil([&] { return spies.finished.count() == 4; }, 2s));
    wallet.m_unlock_latch = nullptr;
    QVERIFY(SerialProbe(fe.callRunner()));

    // Cancelled after the answer's call returned.
    const int locks_before = wallet.lockCalls();
    id = gate.begin(RecordGrantAndOp(gate, calls), RecordRefusal(calls));
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 5; }, 2s));
    QVERIFY(gate.answerUnlock(id, Pass()));
    QVERIFY(SerialProbeNoEvents(fe.callRunner()));
    QVERIFY(gate.cancelUnlock(id));
    QVERIFY(qml_test::PumpUntil(
        [&] { return spies.finished.count() == 5 && wallet.lockCalls() == locks_before + 1; }, 2s));
    QVERIFY(SerialProbe(fe.callRunner()));

    // A send on a locked wallet.
    wallet.scriptSend({SendResult(interfaces::SendCoinsStatus::OK)});
    QVERIFY(adapter.updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), Address(1)},
                                                   {QStringLiteral("amount"), QStringLiteral("1")}}));
    quint64 send_id = adapter.send();
    QVERIFY(send_id != 0);
    QVERIFY(adapter.confirmSend(send_id, true));
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 6; }, 2s));
    QVERIFY(gate.answerUnlock(IdOf(spies.requested, 5), Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return sent.count() == 1; }, 2s));
    QVERIFY(SerialProbe(fe.callRunner()));

    // A send with a fee question, the wallet unlocked across it.
    wallet.scriptSend({SendResult(interfaces::SendCoinsStatus::FeeConfirmationRequired, 123456789),
                       SendResult(interfaces::SendCoinsStatus::OK)});
    QVERIFY(adapter.updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), Address(1)},
                                                   {QStringLiteral("amount"), QStringLiteral("1")}}));
    send_id = adapter.send();
    QVERIFY(send_id != 0);
    QVERIFY(adapter.confirmSend(send_id, true));
    QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 7; }, 2s));
    QVERIFY(gate.answerUnlock(IdOf(spies.requested, 6), Pass()));
    QVERIFY(qml_test::PumpUntil([&] { return fee.count() == 1; }, 2s));
    QVERIFY(adapter.confirmFee(send_id, true));
    QVERIFY(qml_test::PumpUntil([&] { return sent.count() == 2; }, 2s));
    QVERIFY(SerialProbe(fe.callRunner()));

    // Read before the detach, whose restore is the gate's one call on the GUI
    // thread.
    const std::vector<qt_test::ScriptedWallet::Call> log = wallet.log();
    QVERIFY(wallet.sendCalls() == 3);
    QVERIFY(wallet.lockCalls() >= 5);
    for (const qt_test::ScriptedWallet::Call& call : log) {
        QVERIFY2(call.thread != gui,
                 qPrintable(QStringLiteral("a %1 call ran on the GUI thread").arg(MethodName(call.method))));
    }
}

void QmlWalletOpGateTests::unlockPassphraseInSecureMemory()
{
    const QString passphrase = QString::fromUtf8("pässwörd");
    const std::string utf8 = passphrase.toUtf8().toStdString();
    QCOMPARE(utf8.size(), size_t{10});

    // An unlock: the SecureString the wallet receives was reserved before
    // the passphrase went in, and holds its UTF-8 bytes.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setPassphrase(utf8);
        wallet.setLockState(true, true, false);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        WalletOpGate& gate = fe.provider().walletOps();
        gate.attach(wallet, fe.callRunner());
        Spies spies(gate);
        Callbacks calls;

        const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
        QVERIFY(gate.answerUnlock(id, passphrase));
        qml_test::PumpUntil([&] { return spies.accepted.count() + spies.failed.count() == 1; }, 2s);
        const std::vector<qt_test::ScriptedWallet::Call> unlocks = CallsOf(wallet, Method::UnlockWallet);
        QCOMPARE(unlocks.size(), size_t{1});
        QVERIFY2(unlocks[0].capacity >= 1024,
                 qPrintable(QStringLiteral("capacity %1").arg(static_cast<qulonglong>(unlocks[0].capacity))));
        QCOMPARE(unlocks[0].passphrase, utf8);
        QCOMPARE(spies.accepted.count(), 1);
        gate.end(id);
    }

    // An elevation: the same.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setPassphrase(utf8);
        wallet.setLockState(true, false, true);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        WalletOpGate& gate = fe.provider().walletOps();
        gate.attach(wallet, fe.callRunner());
        Spies spies(gate);
        Callbacks calls;

        const quint64 id = gate.begin(RecordGrant(calls), RecordRefusal(calls));
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return spies.requested.count() == 1; }, 2s));
        QCOMPARE(spies.requested.at(0).at(1).value<WalletOpGate::Mode>(), WalletOpGate::Elevate);
        QVERIFY(gate.answerUnlock(id, passphrase));
        qml_test::PumpUntil([&] { return spies.accepted.count() + spies.failed.count() == 1; }, 2s);
        const std::vector<qt_test::ScriptedWallet::Call> elevations = CallsOf(wallet, Method::ElevateWallet);
        QCOMPARE(elevations.size(), size_t{1});
        QVERIFY2(elevations[0].capacity >= 1024,
                 qPrintable(QStringLiteral("capacity %1").arg(static_cast<qulonglong>(elevations[0].capacity))));
        QCOMPARE(elevations[0].passphrase, utf8);
        QCOMPARE(spies.accepted.count(), 1);
        gate.end(id);
    }
}
