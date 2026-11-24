// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/walletmodeltests.h"

#include "interfaces/wallet_coin_channel.h"
#include "interfaces/wallet_coin_source.h"
#include "logging.h"
#include "qt/coinselectionmodel.h"
#include "qt/guiipcworker.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"
#include "util.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

//! A WalletModel over fakes, its coin source a FakeCoinSource. The model can be
//! destroyed before the rest.
struct ModelOverFakeCoins
{
    qt_test::FakeWallet wallet;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel options{node, sidestakes};
    qt_test::FakeWalletTxSource tx_source;
    qt_test::FakeCoinSource coin_source;
    std::unique_ptr<WalletModel> model{std::make_unique<WalletModel>(wallet, tx_source, coin_source, &options)};

    ~ModelOverFakeCoins()
    {
        // A held scan is let go, so nothing waits it out.
        coin_source.m_release = true;
    }
};

//! Keeps the lines logged while it lives.
class LogCapture
{
public:
    LogCapture()
        : m_callback(LogInstance().PushBackCallback([this](const std::string& line) { m_lines.push_back(line); }))
    {
    }
    ~LogCapture() { LogInstance().DeleteCallback(m_callback); }

    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;

    //! How many lines contain \p text.
    int Count(const std::string& text) const
    {
        int count = 0;
        for (const std::string& line : m_lines) {
            if (line.find(text) != std::string::npos) ++count;
        }
        return count;
    }

private:
    std::vector<std::string> m_lines;
    std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

} // namespace

void WalletModelTests::initTestCase()
{
    // OptionsModel reads QSettings. Redirected as OptionsModelTests does, so a
    // run cannot reach or leave anything in a real store.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("walletmodeltests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());
}

void WalletModelTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void WalletModelTests::init()
{
    // A temporary datadir, so that nothing reached through the options model
    // creates the default one on the host.
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void WalletModelTests::cleanup()
{
    QSettings().clear();
    gArgs.ForceSetArg("-datadir", "");
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

// The model only asks the core for the rebuild, on the GUI thread, and the
// core runs it on a thread of its own. The rebuild's end is read on the drain
// ticks and announced on the GUI thread.
void WalletModelTests::coinLoadIsRequestedOfTheCore()
{
    ModelOverFakeCoins fixture;

    QSignalSpy finished(fixture.model.get(), &WalletModel::coinSourceLoadFinished);
    QSignalSpy failed(fixture.model.get(), &WalletModel::coinSourceLoadFailed);

    fixture.model->ensureCoinSourceLoaded();
    QCOMPARE(fixture.coin_source.m_reload_requests.load(), 1);

    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(failed.count(), 0);
    QVERIFY(fixture.model->isCoinSourceLoaded());
    QCOMPARE(fixture.coin_source.m_reloads.load(), 1);
    QVERIFY(fixture.coin_source.m_reload_thread.load() != std::thread::id{});
    QVERIFY(fixture.coin_source.m_reload_thread.load() != std::this_thread::get_id());
}

// A rebuild that fails, as one across IPC does when the core drops the
// connection, ends nothing but itself: the failure is reported on the GUI
// thread, nothing is marked loaded, and the next request rebuilds again.
void WalletModelTests::coinLoadFailureIsContainedAndReported()
{
    ModelOverFakeCoins fixture;
    fixture.coin_source.m_throw = true;

    QSignalSpy finished(fixture.model.get(), &WalletModel::coinSourceLoadFinished);
    QSignalSpy failed(fixture.model.get(), &WalletModel::coinSourceLoadFailed);

    fixture.model->ensureCoinSourceLoaded();

    QVERIFY(failed.wait(10000));
    QCOMPARE(failed.count(), 1);
    const QString message = failed.at(0).at(0).toString();
    QVERIFY2(message.contains("the core dropped the connection"), qPrintable(message));
    QCOMPARE(finished.count(), 0);
    QVERIFY(!fixture.model->isCoinSourceLoaded());

    // Asked again, the model asks again, and this time the rebuild succeeds.
    fixture.coin_source.m_throw = false;
    fixture.model->ensureCoinSourceLoaded();

    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(fixture.coin_source.m_reload_requests.load(), 2);
    QCOMPARE(fixture.coin_source.m_reloads.load(), 2);
    QVERIFY(fixture.model->isCoinSourceLoaded());
}

// A request that throws, as one across IPC does when the core drops the
// connection, escapes nothing and is reported as a failed load.
void WalletModelTests::aRequestThatThrowsIsReportedAsAFailedLoad()
{
    ModelOverFakeCoins fixture;
    fixture.coin_source.m_request_throws = true;

    QSignalSpy finished(fixture.model.get(), &WalletModel::coinSourceLoadFinished);
    QSignalSpy failed(fixture.model.get(), &WalletModel::coinSourceLoadFailed);

    fixture.model->ensureCoinSourceLoaded();

    QVERIFY(failed.wait(10000));
    QCOMPARE(failed.count(), 1);
    const QString message = failed.at(0).at(0).toString();
    QVERIFY2(message.contains("could not reach the core"), qPrintable(message));
    QCOMPARE(finished.count(), 0);
    QCOMPARE(fixture.coin_source.m_reload_requests.load(), 1);
    QCOMPARE(fixture.coin_source.m_reloads.load(), 0);
    QVERIFY(!fixture.model->isCoinSourceLoaded());
}

// A model destroyed while the core holds cs_main (here its stand-in), with
// a rebuild asked for and not yet run, does not wait for the core: nothing
// of the rebuild runs on this model's side. The worker is declared as the
// GUI's main() declares it, so a rebuild posted to it would run.
void WalletModelTests::destructionDoesNotWaitBehindTheCoreForTheCoinLoad()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::CoinLoad);
    ModelOverFakeCoins fixture;
    fixture.coin_source.m_defer_reload = true;
    fixture.coin_source.m_take_cs_main = true;

    qt_test::CsMainStandInHolder core_busy;

    fixture.model->ensureCoinSourceLoaded();

    QElapsedTimer timer;
    timer.start();
    fixture.model.reset();
    const qint64 elapsed_ms = timer.elapsed();

    core_busy.Release();

    QVERIFY2(elapsed_ms < 10000, qPrintable(QString::number(elapsed_ms)));
    QCOMPARE(fixture.coin_source.m_reloads.load(), 0);
    QCOMPARE(fixture.coin_source.m_reload_requests.load(), 1);
}

// A forced reload asked for while a rebuild runs is owed one more pass. When
// the rebuild fails, that pass still runs, once, and a failure of its own owes
// nothing more.
void WalletModelTests::failureWithAForcedReloadOwedRunsItOnce()
{
    ModelOverFakeCoins fixture;
    fixture.coin_source.m_throw = true;
    fixture.coin_source.m_hold = true;

    QSignalSpy finished(fixture.model.get(), &WalletModel::coinSourceLoadFinished);
    QSignalSpy failed(fixture.model.get(), &WalletModel::coinSourceLoadFailed);

    fixture.model->ensureCoinSourceLoaded();
    QTRY_VERIFY_WITH_TIMEOUT(fixture.coin_source.m_reload_waiting.load(), 10000);

    fixture.model->ensureCoinSourceLoaded(/*force=*/true);

    fixture.coin_source.m_hold = false;
    fixture.coin_source.m_release = true;

    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 2, 10000);

    // No third pass.
    QTest::qWait(300);
    QCOMPARE(fixture.coin_source.m_reload_requests.load(), 2);
    QCOMPARE(fixture.coin_source.m_reloads.load(), 2);
    QCOMPARE(failed.count(), 2);
    QCOMPARE(finished.count(), 0);
    QVERIFY(!fixture.model->isCoinSourceLoaded());
}

// A coin view loading when the rebuild fails is told why, and stays loading.
void WalletModelTests::coinSelectionModelReportsAFailedLoad()
{
    ModelOverFakeCoins fixture;
    fixture.coin_source.m_throw = true;

    interfaces::WalletCoinControl coin_control;
    CoinSelectionModel selection(fixture.model.get(), &coin_control, GRC::VIEW_COIN_CONTROL);
    QVERIFY(selection.isLoading());

    QSignalSpy failed(&selection, &CoinSelectionModel::loadingFailed);
    QSignalSpy finished(&selection, &CoinSelectionModel::loadingFinished);

    QVERIFY(failed.wait(10000));
    QCOMPARE(failed.count(), 1);
    const QString message = failed.at(0).at(0).toString();
    QVERIFY2(message.contains("the core dropped the connection"), qPrintable(message));
    QCOMPARE(finished.count(), 0);
    QVERIFY(selection.isLoading());
}

// The read of a rebuild's end clears it in the source, so a drain that throws
// after the read must not lose it: the load still finishes, and the model is
// no longer in flight, so a forced reload asks again.
void WalletModelTests::aThrowAfterTheEndIsReadLosesNoEnd()
{
    ModelOverFakeCoins fixture;
    fixture.coin_source.m_drain_throws_once = true;

    QSignalSpy finished(fixture.model.get(), &WalletModel::coinSourceLoadFinished);

    LogCapture log;
    fixture.model->ensureCoinSourceLoaded();

    QVERIFY(finished.wait(10000));
    QCOMPARE(log.Count("wallet event drain failed"), 1);

    fixture.model->ensureCoinSourceLoaded(/*force=*/true);
    QCOMPARE(fixture.coin_source.m_reload_requests.load(), 2);
}

//! The IPC slot holds one capability, the latest begun. An operation that
//! outlives its prompt (the MRC page's submit runs on a worker) must not end a
//! capability a later operation began, or that operation is refused after the
//! user typed the passphrase.
void WalletModelTests::anOlderContextDoesNotEndALaterOperationsCapability()
{
    qt_test::FakeLockStateWallet wallet;
    wallet.m_lock_state.crypted = true;
    wallet.m_lock_state.locked = false;
    wallet.m_lock_state.unlocked_for_staking_only = true;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel options{node, sidestakes};
    qt_test::FakeWalletTxSource tx_source;
    qt_test::FakeCoinSource coin_source;
    WalletModel model{wallet, tx_source, coin_source, &options};
    QObject::connect(&model, &WalletModel::requireUnlock, &model, [&](CapabilityKind kind) {
        if (wallet.beginElevation(SecureString(), kind)) model.setElevationBegun();
    });

    auto first = std::make_unique<WalletModel::UnlockContext>(model.requestUnlock(CapabilityKind::Contract));
    QVERIFY(first->isValid());
    {
        const WalletModel::UnlockContext second = model.requestUnlock(CapabilityKind::Send);
        QVERIFY(second.isValid());

        // The first operation finishes while the second still needs its capability.
        first.reset();
        QCOMPARE(wallet.m_end_elevation_calls, 0);
    }

    // The second ends its own.
    QCOMPARE(wallet.m_end_elevation_calls, 1);
}
