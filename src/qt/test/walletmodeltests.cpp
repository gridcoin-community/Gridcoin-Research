// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/walletmodeltests.h"

#include "interfaces/wallet_coin_channel.h"
#include "interfaces/wallet_coin_source.h"
#include "qt/coinselectionmodel.h"
#include "qt/guiipcworker.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"
#include "util.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <chrono>
#include <future>
#include <memory>
#include <thread>

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
        // A held scan is let go, so the model's destruction does not wait it out.
        coin_source.m_release = true;
    }
};

//! The thread \p worker runs its jobs on.
std::thread::id ThreadOf(GuiIpcWorker& worker)
{
    std::thread::id id;
    const std::future<void> done = worker.Post([&id] { id = std::this_thread::get_id(); });
    if (!done.valid() || done.wait_for(std::chrono::seconds{10}) != std::future_status::ready) return {};
    return id;
}

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

// The wallet scan runs on the process's coin-load worker, and its completion
// is announced on the GUI thread.
void WalletModelTests::coinLoadRunsOnTheCoinLoadWorker()
{
    // Before the model, as in main(), so that it outlives the model.
    GuiIpcWorker worker(GuiIpcWorker::Role::CoinLoad);

    // Asked first, so that the worker's thread is running before the scan
    // starts: an id is unique only among threads alive at the same time.
    const std::thread::id worker_thread = ThreadOf(worker);
    QVERIFY(worker_thread != std::thread::id{});
    QVERIFY(worker_thread != std::this_thread::get_id());

    ModelOverFakeCoins fixture;

    QSignalSpy finished(fixture.model.get(), &WalletModel::coinSourceLoadFinished);
    QSignalSpy failed(fixture.model.get(), &WalletModel::coinSourceLoadFailed);

    fixture.model->ensureCoinSourceLoaded();

    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(failed.count(), 0);
    QVERIFY(fixture.model->isCoinSourceLoaded());
    QCOMPARE(fixture.coin_source.m_reloads.load(), 1);
    QVERIFY(fixture.coin_source.m_reload_thread.load() == worker_thread);
}

// A scan that throws, as one across IPC does when the core drops the
// connection, ends nothing but itself: the failure is reported on the GUI
// thread, nothing is marked loaded, and the next request scans again.
void WalletModelTests::coinLoadFailureIsContainedAndReported()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::CoinLoad);
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

    // Asked again, the model scans again, and this time the scan succeeds.
    fixture.coin_source.m_throw = false;
    fixture.model->ensureCoinSourceLoaded();

    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(fixture.coin_source.m_reloads.load(), 2);
    QVERIFY(fixture.model->isCoinSourceLoaded());
}

// A model destroyed while its scan runs waits for the scan, so the scan never
// outlives the model it reports to.
void WalletModelTests::destructionWaitsForTheCoinLoad()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::CoinLoad);
    ModelOverFakeCoins fixture;
    fixture.coin_source.m_hold = true;

    fixture.model->ensureCoinSourceLoaded();
    QTRY_VERIFY_WITH_TIMEOUT(fixture.coin_source.m_reload_waiting.load(), 10000);

    // The scan is let go only once the model is being destroyed.
    std::thread releaser([&fixture] {
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        fixture.coin_source.m_release = true;
    });

    fixture.model.reset();
    const int ended_when_destroyed = fixture.coin_source.m_reloads_ended;
    releaser.join();

    // What the scan posted to the destroyed model is dropped.
    QCoreApplication::processEvents();

    QCOMPARE(ended_when_destroyed, 1);
}

// A forced reload asked for while a scan runs is owed one more pass. When the
// scan fails, that pass still runs, once, and a failure of its own owes
// nothing more.
void WalletModelTests::failureWithAForcedReloadOwedRunsItOnce()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::CoinLoad);
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
    QCOMPARE(fixture.coin_source.m_reloads.load(), 2);
    QCOMPARE(failed.count(), 2);
    QCOMPARE(finished.count(), 0);
    QVERIFY(!fixture.model->isCoinSourceLoaded());
}

// A coin view loading when the scan fails is told why, and stays loading.
void WalletModelTests::coinSelectionModelReportsAFailedLoad()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::CoinLoad);
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
