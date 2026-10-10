// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "statusadaptertests.h"

#include "interfacefakes_qml.h"
#include "modelharness.h"
#include "qmltestutil.h"

#include <amount.h>
#include <interfaces/node.h>
#include <interfaces/wallet_tx_channel.h>
#include <interfaces/wallet_tx_record.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/quick/clientstatus.h>
#include <qt/quick/clipboard.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/recenttransactions.h>
#include <qt/quick/researchersummary.h>
#include <qt/quick/walletsummary.h>
#include <qt/walletmodel.h>
#include <util.h>

#include <QChar>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QMetaObject>
#include <QMetaProperty>
#include <QModelIndex>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTranslator>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

//! The root the front ends here are built with, as gridcoinresearch-qml loads
//! it.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

//! A disconnect policy for a runner a test builds itself: it matches nothing,
//! so it notifies nothing.
QmlCallRunner::DisconnectPolicy TestPolicy()
{
    return QmlCallRunner::DisconnectPolicy{[](const std::string&) { return false; }, [](const std::string&) {}};
}

//! Posts a no-op on the serial lane and pumps until it is delivered, so every
//! serial read posted before it has been delivered too.
bool SerialProbe(QmlCallRunner& runner)
{
    auto delivered = std::make_shared<bool>(false);
    if (!runner.postSerial([] {}, [delivered] { *delivered = true; })) return false;
    return qml_test::PumpUntil([delivered] { return *delivered; }, 2s);
}

//! Two probes: a read in flight, and the re-post its delivery makes for a
//! trigger that arrived meanwhile, have both been delivered.
bool SettleSerial(QmlCallRunner& runner)
{
    return SerialProbe(runner) && SerialProbe(runner);
}

int64_t Now()
{
    return QDateTime::currentSecsSinceEpoch();
}

//! Joins the global pool, so no pooled read outlives the harness's fakes.
void ReapPool()
{
    QThreadPool::globalInstance()->waitForDone();
}

} // namespace

void QmlStatusAdapterTests::harnessBuildsModels()
{
    // (1) No harness is alive, so the throw can come only from the
    // organization check. The saved name is put back when the block ends.
    {
        const QString previous_org = QCoreApplication::organizationName();
        qml_test::ScopeExit restore_org([previous_org] { QCoreApplication::setOrganizationName(previous_org); });
        QCoreApplication::setOrganizationName(QStringLiteral("Gridcoin"));
        QVERIFY_THROWS_EXCEPTION(std::logic_error, qml_test::ModelHarness{});
    }

    // (2) A harness is built and destroyed.
    QVERIFY_THROWS_NO_EXCEPTION(qml_test::ModelHarness{});

    {
        // (3) The harness builds the five models, and their settings and
        // datadir stay under its own roots.
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());

        harness.options.setMaskValues(true);
        QVERIFY(harness.options.getMaskValues());
        const QString settings_file = QSettings().fileName();
        QVERIFY2(settings_file.startsWith(harness.isolation.settingsRoot()), qPrintable(settings_file));
        const QString data_dir = GUIUtil::boostPathToQString(GetDataDir(false));
        QVERIFY2(data_dir.startsWith(harness.isolation.dataDirRoot()), qPrintable(data_dir));

        // (4) One harness at a time: a second one throws, and leaves the live
        // harness's settings path and datadir as they were.
        QVERIFY_THROWS_EXCEPTION(std::logic_error, qml_test::ModelHarness{});
        QVERIFY2(QSettings().fileName().startsWith(harness.isolation.settingsRoot()),
                 qPrintable(QSettings().fileName()));
        QVERIFY(GUIUtil::boostPathToQString(GetDataDir(false)).startsWith(harness.isolation.dataDirRoot()));

        fe.detachModels();
    }
    QVERIFY(qml_test::g_rethrown.empty());

    // (5) A harness puts back the -datadir it found.
    {
        QTemporaryDir own;
        QVERIFY(own.isValid());
        const bool was_set = gArgs.IsArgSet("-datadir");
        const std::string was = gArgs.GetArg("-datadir", "");
        qml_test::ScopeExit restore_datadir([was_set, was] {
            if (was_set) {
                gArgs.ForceSetArg("-datadir", was);
            } else {
                gArgs.ClearForcedArg("-datadir");
            }
            gArgs.ClearPathCache();
        });
        gArgs.ForceSetArg("-datadir", own.path().toStdString());
        gArgs.ClearPathCache();
        {
            qml_test::ModelHarness harness;
        }
        QCOMPARE(QString::fromStdString(gArgs.GetArg("-datadir", "")), own.path());
    }
}

void QmlStatusAdapterTests::unitsFormatting()
{
    qml_test::ModelHarness harness;
    harness.researcher_context.m_snapshot.accrual = 123456789;
    harness.researcher.refresh();
    QmlCallRunner runner(TestPolicy());
    Clipboard clipboard(1);
    WalletSummary summary(1, clipboard);
    ResearcherSummary researcher(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        summary.detach();
        researcher.detach();
    });
    summary.attach(harness.wallet_model, &harness.options, runner);
    researcher.attach(harness.researcher, &harness.options);
    QVERIFY(SettleSerial(runner));

    const QString thin(QChar(0x2009));
    const auto set_unit = [&](int unit) {
        harness.options.setData(harness.options.index(OptionsModel::DisplayUnit), unit, Qt::EditRole);
    };

    // 2^53 + 1 halfords: exact only in integer arithmetic.
    emit harness.wallet_model.balanceChanged(9007199254740993, 0, 0, 0);
    QCOMPARE(summary.balanceText(),
             QStringLiteral("90") + thin + QStringLiteral("071") + thin + QStringLiteral("992.54740993 GRC"));

    emit harness.wallet_model.balanceChanged(123456789, 0, 0, 0);
    QCOMPARE(summary.balanceText(), QStringLiteral("1.23456789 GRC"));
    set_unit(BitcoinUnits::mBTC);
    QCOMPARE(summary.balanceText(), QStringLiteral("1") + thin + QStringLiteral("234.56789 mGRC"));
    set_unit(BitcoinUnits::uBTC);
    QCOMPARE(summary.balanceText(), QStringLiteral("1") + thin + QStringLiteral("234") + thin +
                                        QStringLiteral("567.89 ") + QChar(0x03BC) + QStringLiteral("GRC"));

    // The accrual follows the options model's unit too.
    set_unit(BitcoinUnits::mBTC);
    QVERIFY2(researcher.accrualText().endsWith(QStringLiteral("mGRC")), qPrintable(researcher.accrualText()));
}

void QmlStatusAdapterTests::syncRuleTruthTable()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    // (connections, tip age, height, peers) -> state, through the status read
    // each signal posts.
    const auto row = [&](int connections, int64_t tip_age, int height, int peers) {
        harness.node.m_node_count = connections;
        harness.node.m_last_block_time = Now() - tip_age;
        emit harness.client.numConnectionsChanged(connections);
        emit harness.client.numBlocksChanged(height, peers);
        return SettleSerial(runner);
    };
    QVERIFY(row(0, 60, 100, 100));
    QCOMPARE(status.syncState(), ClientStatus::NoConnections);
    QVERIFY(!status.inSync());
    QVERIFY(row(8, 20 * 60, 100, 100));
    QCOMPARE(status.syncState(), ClientStatus::UpToDate);
    QVERIFY(status.inSync());
    QVERIFY(row(8, 91 * 60, 100, 100));
    QCOMPARE(status.syncState(), ClientStatus::CatchingUp);
    QVERIFY(row(8, 60, 99, 100));
    QCOMPARE(status.syncState(), ClientStatus::CatchingUp);
    QVERIFY(row(8, 60, 100, 0));
    QCOMPARE(status.syncState(), ClientStatus::UpToDate);

    // Last: the tip age comes from ClientModel's cached tip time, which
    // updateNumBlocks stores before it emits numBlocksChanged.
    harness.node.m_node_count = 8;
    harness.node.m_last_block_time = Now() - 91 * 60;
    emit harness.client.numBlocksChanged(100, 0);
    QVERIFY2(qml_test::PumpUntil([&] { return status.syncState() == ClientStatus::CatchingUp; }, 2s),
             "a 91-minute-old tip did not read as catching up");
    QVERIFY(SettleSerial(runner));
    harness.client.updateNumBlocks(100, Now() - 20 * 60, 0);
    QVERIFY2(qml_test::PumpUntil([&] { return status.syncState() == ClientStatus::UpToDate; }, 2s),
             "the status read did not take the tip time updateNumBlocks cached");
}

void QmlStatusAdapterTests::blocksText()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    // No connections, so never in sync.
    const auto text = [&](int height, int peers) {
        emit harness.client.numBlocksChanged(height, peers);
        return SettleSerial(runner) ? status.blocksText() : QStringLiteral("<no delivery>");
    };
    QCOMPARE(text(90, 100), QStringLiteral("90 of 100 (90%)"));
    QCOMPARE(text(2, 3), QStringLiteral("2 of 3 (66%)"));
    QCOMPARE(text(100, 100), QStringLiteral("100"));
    QCOMPARE(text(5, 0), QStringLiteral("5"));
}

void QmlStatusAdapterTests::statusReadsOffGuiThreadAndCoalesce()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    // Before attach: ClientModel::getDifficulty keeps its first read, which the
    // hydrate's getStatusBarWarnings makes.
    harness.node.setWarnings("W");
    harness.node.m_difficulty = 0.05;
    harness.staking.m_staking = false;
    harness.staking.setErrors("E");

    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    qml_test::Latch latch;
    qml_test::ScopeExit unlatch([&] {
        latch.open = true;
        harness.node.m_warnings_latch = nullptr;
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    const std::thread::id gui_thread = std::this_thread::get_id();
    const int before = harness.node.m_warnings_calls;
    harness.node.m_warnings_latch = &latch;
    emit harness.client.numBlocksChanged(1, 0);
    emit harness.client.numBlocksChanged(2, 0);
    emit harness.client.numBlocksChanged(3, 0);
    latch.open = true;
    QVERIFY(qml_test::PumpUntil([&] { return harness.node.m_warnings_calls - before >= 2; }, 2s));
    QVERIFY(SettleSerial(runner));
    // Each status read reaches getWarnings once: the first read and one
    // re-post for the two signals that arrived while it was in flight.
    QCOMPARE(harness.node.m_warnings_calls - before, 2);
    QVERIFY(harness.node.m_warnings_thread.load() != gui_thread);

    // The parity call below reads getWarnings on the GUI thread itself.
    QCOMPARE(status.statusBarWarnings(), harness.client.getStatusBarWarnings());
    QCOMPARE(status.statusBarWarnings(), QStringLiteral("W; Low difficulty!; Miner: E"));
}

void QmlStatusAdapterTests::connectionAndStakingStates()
{
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        status.attach(harness.client, runner);
        QVERIFY(SettleSerial(runner));

        const int counts[] = {0, 1, 2, 3, 5, 6, 9, 10};
        const int levels[] = {0, 1, 1, 2, 2, 3, 3, 4};
        for (size_t i = 0; i < std::size(counts); ++i) {
            harness.node.m_node_count = counts[i];
            emit harness.client.numConnectionsChanged(counts[i]);
            QVERIFY(SettleSerial(runner));
            QCOMPARE(status.numConnections(), counts[i]);
            QCOMPARE(status.connectionLevel(), levels[i]);
        }
        harness.node.m_node_count = 0;
        emit harness.client.numConnectionsChanged(0);
        QVERIFY(SettleSerial(runner));
        QCOMPARE(status.connectionToolTip(),
                 QStringLiteral("No active connections to the Gridcoin network. If this persists more than a few "
                                "minutes, please check your configuration and your network connectivity."));

        emit harness.client.minerStatusChanged(true, 1000, 20, 86400);
        QVERIFY(SettleSerial(runner));
        QCOMPARE(status.stakingState(), ClientStatus::Staking);
        QVERIFY2(status.stakingToolTip().contains(QStringLiteral("1.01 times per day")),
                 qPrintable(status.stakingToolTip()));

        emit harness.client.minerStatusChanged(false, 1000, 0, 0);
        QVERIFY(SettleSerial(runner));
        QCOMPARE(status.stakingState(), ClientStatus::UnableToStake);
    }

    // The miner errors are node text in a rich-text tooltip: escaped, and
    // taken from the status read the signal posts.
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        status.attach(harness.client, runner);
        QVERIFY(SettleSerial(runner));

        harness.staking.setErrors("<b>x</b>&");
        emit harness.client.minerStatusChanged(false, 1000, 0, 0);
        QVERIFY2(qml_test::PumpUntil(
                     [&] { return status.stakingToolTip().contains(QStringLiteral("&lt;b&gt;x&lt;/b&gt;&amp;")); },
                     2s),
                 qPrintable(status.stakingToolTip()));
    }
}

void QmlStatusAdapterTests::scraperSnapshotPooled()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    interfaces::ScraperConvergenceSnapshot snapshot;
    snapshot.time = 1700000000;
    snapshot.excluded_projects = {"<i>p</i>"};
    harness.node.setScraperSnapshot(snapshot);

    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    qml_test::Latch latch;
    qml_test::ScopeExit unlatch([&] {
        latch.open = true;
        ReapPool();
        harness.node.m_scraper_latch = nullptr;
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    const std::thread::id gui_thread = std::this_thread::get_id();
    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Convergence), CT_NEW);
    QVERIFY2(qml_test::PumpUntil([&] { return status.scraperState() == ClientStatus::Converged; }, 2s),
             "the convergence event did not reach the scraper state");
    QVERIFY(harness.node.m_scraper_thread.load() != gui_thread);
    QVERIFY2(status.scraperToolTip().contains(QStringLiteral("&lt;i&gt;p&lt;/i&gt;")),
             qPrintable(status.scraperToolTip()));
    QVERIFY2(status.scraperToolTip().contains(QStringLiteral("<br>")) && !status.scraperToolTip().contains(QLatin1Char('\n')), qPrintable(status.scraperToolTip()));

    // A snapshot held on the pooled lane does not hold the serial lane.
    harness.node.m_scraper_latch = &latch;
    const int before = harness.node.m_scraper_calls;
    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Convergence), CT_UPDATED);
    QVERIFY2(SerialProbe(runner), "a held scraper snapshot delayed the serial lane");
    latch.open = true;
    QVERIFY(qml_test::PumpUntil([&] { return harness.node.m_scraper_calls > before; }, 2s));
}

void QmlStatusAdapterTests::hydrateOnAttachAndSignalWins()
{
    // (a) The miner state, with its estimated frequency, and the scraper state
    // on attach to a running node, with no signal emitted.
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.staking.m_staking = true;
        harness.staking.m_coin_weight = 42;
        harness.staking.m_network_weight = 400;
        harness.staking.m_etts = 86400;
        interfaces::ScraperConvergenceSnapshot snapshot;
        snapshot.time = 1700000000;
        snapshot.current_event_type = static_cast<int>(scrapereventtypes::Convergence);
        snapshot.current_event_status = CT_NEW;
        harness.node.setScraperSnapshot(snapshot);

        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        status.attach(harness.client, runner);
        QVERIFY(SettleSerial(runner));
        QVERIFY2(qml_test::PumpUntil([&] { return status.scraperState() == ClientStatus::Converged; }, 2s),
                 "the scraper snapshot's current event did not set the state");
        QCOMPARE(status.stakingState(), ClientStatus::Staking);
        QCOMPARE(status.coinWeight(), 42.0);
        // ClientModel::getNetWeight: the network weight / 80.
        QCOMPARE(status.networkWeight(), 5.0);
        QVERIFY2(status.stakingToolTip().contains(QStringLiteral("1.01 times per day")),
                 qPrintable(status.stakingToolTip()));
    }

    // (b) A balanceChanged received while the balance read is in flight wins.
    {
        qml_test::ModelHarness harness;
        harness.wallet().m_balance = 5 * COIN;
        QmlCallRunner runner(TestPolicy());
        Clipboard clipboard(1);
        WalletSummary summary(1, clipboard);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            summary.detach();
        });
        qt_test::FakeLatch latch;
        qml_test::ScopeExit unlatch([&] {
            latch.open = true;
            harness.wallet().m_balance_latch = nullptr;
        });
        harness.wallet().m_balance_latch = &latch;
        summary.attach(harness.wallet_model, &harness.options, runner);
        emit harness.wallet_model.balanceChanged(7 * COIN, 0, 0, 0);
        latch.open = true;
        QVERIFY(SettleSerial(runner));
        QCOMPARE(summary.balance(), qint64{700000000});
    }

    // (c) A snapshot that bows out falls back to the four getters.
    {
        qml_test::ModelHarness harness;
        harness.wallet().m_balance = 5 * COIN;
        harness.wallet().m_balances_bow_out = true;
        QmlCallRunner runner(TestPolicy());
        Clipboard clipboard(1);
        WalletSummary summary(1, clipboard);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            summary.detach();
        });
        summary.attach(harness.wallet_model, &harness.options, runner);
        QVERIFY(SettleSerial(runner));
        QCOMPARE(summary.balance(), qint64{500000000});
    }
}

void QmlStatusAdapterTests::totalIncludesImmatureAndCopy()
{
    qml_test::ModelHarness harness;
    QmlCallRunner runner(TestPolicy());
    Clipboard clipboard(1);
    WalletSummary summary(1, clipboard);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        summary.detach();
    });
    summary.attach(harness.wallet_model, &harness.options, runner);
    QVERIFY(SettleSerial(runner));

    emit harness.wallet_model.balanceChanged(1 * COIN, 2 * COIN, 3 * COIN, 4 * COIN);
    QCOMPARE(summary.totalText(), QStringLiteral("10.00 GRC"));
    QVERIFY(summary.immatureVisible());

    emit harness.wallet_model.balanceChanged(1234 * COIN, 0, 0, 0);
    QVERIFY(summary.copyAmount(WalletSummary::Balance));
    // The offscreen clipboard round-trips text (clipboardStrips).
    QCOMPARE(clipboard.text(), QStringLiteral("1234.00"));
}

void QmlStatusAdapterTests::encryptionStatusKnownAndDedup()
{
    {
        qml_test::ModelHarness harness;
        harness.wallet().m_lock_state.crypted = true;
        harness.wallet().m_lock_state.locked = true;
        QmlCallRunner runner(TestPolicy());
        Clipboard clipboard(1);
        WalletSummary summary(1, clipboard);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            summary.detach();
        });
        qt_test::FakeLatch latch;
        qml_test::ScopeExit unlatch([&] {
            latch.open = true;
            harness.wallet().m_lock_state_latch = nullptr;
        });
        QSignalSpy status_spy(&summary, &WalletSummary::encryptionStatusChanged);

        // (1) Not known while the lock-state read is held.
        harness.wallet().m_lock_state_latch = &latch;
        summary.attach(harness.wallet_model, &harness.options, runner);
        qml_test::PumpFor(50ms);
        QVERIFY(!summary.encryptionStatusKnown());
        QCOMPARE(status_spy.count(), 0);

        // (2) Known once it is delivered.
        latch.open = true;
        QVERIFY(SettleSerial(runner));
        QVERIFY(summary.encryptionStatusKnown());
        QCOMPARE(summary.encryptionStatus(), WalletSummary::Locked);

        // (3) The model repeats its signal with the current value: no NOTIFY.
        QSignalSpy repeat_spy(&summary, &WalletSummary::encryptionStatusChanged);
        emit harness.wallet_model.encryptionStatusChanged(WalletModel::Locked);
        emit harness.wallet_model.encryptionStatusChanged(WalletModel::Locked);
        QCOMPARE(repeat_spy.count(), 0);
        emit harness.wallet_model.encryptionStatusChanged(WalletModel::Unlocked);
        QCOMPARE(repeat_spy.count(), 1);
    }

    // (4) A balanceChanged during the seed reads blocks only the balances.
    {
        qml_test::ModelHarness harness;
        harness.wallet().m_balance = 5 * COIN;
        QmlCallRunner runner(TestPolicy());
        Clipboard clipboard(1);
        WalletSummary summary(1, clipboard);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            summary.detach();
        });
        qt_test::FakeLatch latch;
        qml_test::ScopeExit unlatch([&] {
            latch.open = true;
            harness.wallet().m_lock_state_latch = nullptr;
            harness.wallet().m_balance_latch = nullptr;
        });
        harness.wallet().m_lock_state_latch = &latch;
        harness.wallet().m_balance_latch = &latch;
        summary.attach(harness.wallet_model, &harness.options, runner);
        emit harness.wallet_model.balanceChanged(7 * COIN, 0, 0, 0);
        latch.open = true;
        QVERIFY(SettleSerial(runner));
        QCOMPARE(summary.balance(), qint64{700000000});
        QVERIFY(summary.encryptionStatusKnown());
        QCOMPARE(summary.encryptionStatus(), WalletSummary::Unencrypted);
    }

    // (5) An encryptionStatusChanged during the lock-state read blocks only
    // the lock state.
    {
        qml_test::ModelHarness harness;
        harness.wallet().m_lock_state.crypted = true;
        harness.wallet().m_lock_state.locked = true;
        harness.wallet().m_balance = 5 * COIN;
        QmlCallRunner runner(TestPolicy());
        Clipboard clipboard(1);
        WalletSummary summary(1, clipboard);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            summary.detach();
        });
        qt_test::FakeLatch latch;
        qml_test::ScopeExit unlatch([&] {
            latch.open = true;
            harness.wallet().m_lock_state_latch = nullptr;
        });
        harness.wallet().m_lock_state_latch = &latch;
        summary.attach(harness.wallet_model, &harness.options, runner);
        QSignalSpy status_spy(&summary, &WalletSummary::encryptionStatusChanged);
        emit harness.wallet_model.encryptionStatusChanged(WalletModel::Unlocked);
        QVERIFY(summary.encryptionStatusKnown());
        QCOMPARE(summary.encryptionStatus(), WalletSummary::Unlocked);
        latch.open = true;
        // The probe queues behind both seed reads.
        QVERIFY(SerialProbe(runner));
        QCOMPARE(summary.encryptionStatus(), WalletSummary::Unlocked);
        QCOMPARE(status_spy.count(), 1);
        QCOMPARE(summary.balance(), qint64{500000000});
    }
}

void QmlStatusAdapterTests::maskEveryAmount()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    interfaces::ResearcherSnapshot snapshot;
    snapshot.has_eligible_projects = true;
    snapshot.cpid = "0123456789abcdef0123456789abcdef";
    snapshot.magnitude_text = "12.5";
    snapshot.accrual = 1000;
    harness.researcher_context.m_snapshot = snapshot;
    harness.researcher.refresh();
    // A second model over the same context, masked through its own setter:
    // what the Widgets overview would show.
    ResearcherModel widgets(harness.researcher_context);

    QmlCallRunner runner(TestPolicy());
    Clipboard clipboard(1);
    WalletSummary summary(1, clipboard);
    ClientStatus status(1);
    RecentTransactions recent(1);
    ResearcherSummary researcher(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        summary.detach();
        status.detach();
        recent.detach();
        researcher.detach();
    });
    summary.attach(harness.wallet_model, &harness.options, runner);
    status.attach(harness.client, runner);
    recent.attach(harness.wallet_model, &harness.options, runner);
    researcher.attach(harness.researcher, &harness.options);
    QVERIFY(SettleSerial(runner));

    bool near_limit = false;
    const int unit = BitcoinUnits::BTC;
    summary.setMaskValues(true);
    QVERIFY(harness.options.getMaskValues());

    // Every WalletSummary text is the mask.
    const QMetaObject* meta = summary.metaObject();
    int texts = 0;
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        const QString name = QString::fromLatin1(meta->property(i).name());
        if (!name.endsWith(QStringLiteral("Text"))) continue;
        ++texts;
        const QString expected = name == QStringLiteral("balanceRoundedText")
                                     ? BitcoinUnits::formatOverviewRounded(0, true)
                                     : BitcoinUnits::formatWithPrivacy(unit, 0, true);
        QCOMPARE(meta->property(i).read(&summary).toString(), expected);
    }
    QCOMPARE(texts, 6);
    QCOMPARE(status.coinWeightText(), QStringLiteral("#.##"));
    QVERIFY(recent.masked());
    widgets.setMaskCpidMagnitudeAccrual(true);
    QCOMPARE(researcher.cpid(), widgets.formatCpid());
    QCOMPARE(researcher.magnitudeText(), widgets.formatMagnitude());
    QCOMPARE(researcher.accrualText(), widgets.formatAccrual(unit, near_limit));
    QVERIFY(!summary.copyAmount(WalletSummary::Balance));

    // The researcher values follow each toggle.
    summary.setMaskValues(false);
    widgets.setMaskCpidMagnitudeAccrual(false);
    QCOMPARE(researcher.cpid(), widgets.formatCpid());
    QCOMPARE(researcher.magnitudeText(), widgets.formatMagnitude());
    QCOMPARE(researcher.accrualText(), widgets.formatAccrual(unit, near_limit));
    QVERIFY(researcher.cpid() != QString(32, QLatin1Char('#')));
    summary.setMaskValues(true);
    widgets.setMaskCpidMagnitudeAccrual(true);
    QCOMPARE(researcher.cpid(), widgets.formatCpid());
    QCOMPARE(researcher.magnitudeText(), widgets.formatMagnitude());
    QCOMPARE(researcher.accrualText(), widgets.formatAccrual(unit, near_limit));

    // Out of sync shows "..." whether masked or not.
    snapshot.out_of_sync = true;
    harness.researcher_context.m_snapshot = snapshot;
    harness.researcher.refresh();
    widgets.refresh();
    for (const bool masked : {true, false}) {
        summary.setMaskValues(masked);
        widgets.setMaskCpidMagnitudeAccrual(masked);
        QCOMPARE(researcher.magnitudeText(), widgets.formatMagnitude());
        QCOMPARE(researcher.accrualText(), widgets.formatAccrual(unit, near_limit));
        QCOMPARE(researcher.magnitudeText(), QStringLiteral("..."));
    }
}

void QmlStatusAdapterTests::recentTransactionsRoles()
{
    qml_test::ModelHarness harness;

    // One row per transaction type, and one generated row per mined type.
    struct Expected {
        int type;
        int mined;
        QString icon;
    };
    const std::vector<Expected> expected{
        {TransactionRecord::Other, 0, QStringLiteral("qrc:/icons/events/ic_event_green_red.svg")},
        {TransactionRecord::SendToAddress, 0, QStringLiteral("qrc:/icons/events/ic_event_red.svg")},
        {TransactionRecord::SendToOther, 0, QStringLiteral("qrc:/icons/events/ic_event_red.svg")},
        {TransactionRecord::RecvWithAddress, 0, QStringLiteral("qrc:/icons/events/ic_event_green.svg")},
        {TransactionRecord::RecvFromOther, 0, QStringLiteral("qrc:/icons/events/ic_event_green.svg")},
        {TransactionRecord::SendToSelf, 0, QStringLiteral("qrc:/icons/events/ic_event_green_red.svg")},
        {TransactionRecord::BeaconAdvertisement, 0, QStringLiteral("qrc:/icons/events/ic_event_beacon.svg")},
        {TransactionRecord::Poll, 0, QStringLiteral("qrc:/icons/events/ic_event_voting.svg")},
        {TransactionRecord::Vote, 0, QStringLiteral("qrc:/icons/events/ic_event_voting.svg")},
        {TransactionRecord::Message, 0, QStringLiteral("qrc:/icons/events/ic_event_message.svg")},
        {TransactionRecord::MRC, 0, QStringLiteral("qrc:/icons/events/ic_event_mrc.svg")},
        {TransactionRecord::Generated, GRC::MinedType::UNKNOWN, QStringLiteral("qrc:/icons/generic/ic_help.svg")},
        {TransactionRecord::Generated, GRC::MinedType::POS, QStringLiteral("qrc:/icons/events/ic_event_yellow.svg")},
        {TransactionRecord::Generated, GRC::MinedType::POR, QStringLiteral("qrc:/icons/events/ic_event_purple.svg")},
        {TransactionRecord::Generated, GRC::MinedType::ORPHANED, QStringLiteral("qrc:/icons/generic/ic_alert.svg")},
        {TransactionRecord::Generated, GRC::MinedType::POS_SIDE_STAKE_RCV,
         QStringLiteral("qrc:/icons/events/ic_event_yellow_green.svg")},
        {TransactionRecord::Generated, GRC::MinedType::POR_SIDE_STAKE_RCV,
         QStringLiteral("qrc:/icons/events/ic_event_purple_green.svg")},
        {TransactionRecord::Generated, GRC::MinedType::POS_SIDE_STAKE_SEND,
         QStringLiteral("qrc:/icons/events/ic_event_yellow_red.svg")},
        {TransactionRecord::Generated, GRC::MinedType::POR_SIDE_STAKE_SEND,
         QStringLiteral("qrc:/icons/events/ic_event_purple_red.svg")},
        {TransactionRecord::Generated, GRC::MinedType::SUPERBLOCK, QStringLiteral("qrc:/icons/events/superblock.svg")},
        {TransactionRecord::Generated, GRC::MinedType::MRC_RCV,
         QStringLiteral("qrc:/icons/events/ic_event_purple_green.svg")},
        {TransactionRecord::Generated, GRC::MinedType::MRC_SEND,
         QStringLiteral("qrc:/icons/events/ic_event_purple_red.svg")},
    };
    std::vector<TransactionRecord> rows;
    for (size_t i = 0; i < expected.size(); ++i) {
        TransactionRecord record;
        record.type = static_cast<TransactionRecord::Type>(expected[i].type);
        record.status.generated_type = static_cast<GRC::MinedType>(expected[i].mined);
        record.address = (QStringLiteral("address") + QString::number(i)).toStdString();
        record.time = 1700000000 + static_cast<int64_t>(i);
        // Odd rows are spends.
        if (i % 2) {
            record.debit = -static_cast<int64_t>(i + 1) * COIN;
        } else {
            record.credit = static_cast<int64_t>(i + 1) * COIN;
        }
        rows.push_back(record);
    }
    harness.tx_source.setRows(GRC::VIEW_OVERVIEW, rows);

    QmlCallRunner runner(TestPolicy());
    RecentTransactions recent(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        recent.detach();
    });
    qt_test::FakeLatch latch;
    qml_test::ScopeExit unlatch([&] {
        latch.open = true;
        harness.tx_source.m_set_view_limit_latch = nullptr;
    });
    recent.attach(harness.wallet_model, &harness.options, runner);
    QCOMPARE(recent.rowCount(), static_cast<int>(expected.size()));

    // Exactly the seven names.
    QStringList names;
    for (const QByteArray& name : recent.roleNames()) names << QString::fromLatin1(name);
    names.sort();
    QCOMPARE(names, (QStringList{QStringLiteral("address"), QStringLiteral("amount"), QStringLiteral("amountText"),
                                 QStringLiteral("confirmed"), QStringLiteral("date"), QStringLiteral("iconSource"),
                                 QStringLiteral("negative")}));

    for (int row = 0; row < recent.rowCount(); ++row) {
        const QModelIndex index = recent.index(row, 0);
        const QString icon = recent.data(index, RecentTransactions::IconSourceRole).toString();
        QCOMPARE(icon, expected[row].icon);
        // The resource path: ":" and what follows "qrc:".
        QVERIFY2(QFile::exists(QStringLiteral(":") + icon.mid(4)), qPrintable(icon));
        const qint64 amount = rows[row].credit + rows[row].debit;
        QCOMPARE(recent.data(index, RecentTransactions::AmountRole).toLongLong(), amount);
        QCOMPARE(recent.data(index, RecentTransactions::NegativeRole).toBool(), amount < 0);
        QCOMPARE(recent.data(index, RecentTransactions::AmountTextRole).toString(),
                 BitcoinUnits::format(BitcoinUnits::BTC, amount) + QStringLiteral(" GRC"));
        QCOMPARE(recent.data(index, RecentTransactions::AddressRole).toString(),
                 QString::fromStdString(rows[row].address));
        // Every served role, read: none looks up an address-book label.
        for (const int role : recent.roleNames().keys()) (void)recent.data(index, role);
    }
    QCOMPARE(harness.wallet().m_address_label_calls.load(), 0);

    // The stairstep from the attach limit 21, on the serial lane.
    const std::thread::id gui_thread = std::this_thread::get_id();
    harness.tx_source.clearRecords();
    recent.setVisibleRows(8);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(harness.tx_source.setViewLimitCalls(), 0);
    recent.setVisibleRows(30);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(harness.tx_source.viewLimits(), std::vector<int>{90});
    QCOMPARE(harness.tx_source.viewLimitViews(), std::vector<int>{GRC::VIEW_OVERVIEW});
    QVERIFY(harness.tx_source.setViewLimitThread() != gui_thread);
    recent.setVisibleRows(2);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(harness.tx_source.viewLimits(), (std::vector<int>{90, 6}));

    // Coalesced: one in flight, and one re-post with the latest limit.
    harness.tx_source.clearRecords();
    harness.tx_source.m_set_view_limit_latch = &latch;
    recent.setVisibleRows(30);
    recent.setVisibleRows(2);
    recent.setVisibleRows(40);
    latch.open = true;
    QVERIFY(SettleSerial(runner));
    QCOMPARE(harness.tx_source.viewLimits(), (std::vector<int>{90, 120}));
}

void QmlStatusAdapterTests::researcherModeAndLimit()
{
    qml_test::ModelHarness harness;
    ResearcherSummary researcher(1);
    qml_test::ScopeExit detach([&] { researcher.detach(); });
    researcher.attach(harness.researcher, &harness.options);

    const auto set = [&](const interfaces::ResearcherSnapshot& snapshot) {
        harness.researcher_context.m_snapshot = snapshot;
        harness.researcher.refresh();
    };

    interfaces::ResearcherSnapshot solo;
    solo.has_eligible_projects = true;
    set(solo);
    QCOMPARE(researcher.mode(), ResearcherSummary::Solo);
    QCOMPARE(researcher.modeText(), QStringLiteral("CPID"));
    QVERIFY(researcher.showMagnitude());

    // An active beacon: the tooltip is rich text, so its lines break with
    // <br>.
    interfaces::ResearcherSnapshot active = solo;
    active.beacon_present = true;
    active.has_active_beacon = true;
    set(active);
    QVERIFY2(researcher.beaconToolTip().contains(QStringLiteral("<br>")) && !researcher.beaconToolTip().contains(QLatin1Char('\n')), qPrintable(researcher.beaconToolTip()));

    // Pool projects without the detected pool mode: the mode follows the
    // projects.
    interfaces::ResearcherSnapshot pool;
    pool.has_pool_projects = true;
    pool.detected_pool_mode = false;
    set(pool);
    QCOMPARE(researcher.mode(), ResearcherSummary::Pool);
    QCOMPARE(researcher.modeText(), QStringLiteral("Pool"));
    QVERIFY(!researcher.showMagnitude());

    set(interfaces::ResearcherSnapshot{});
    QCOMPARE(researcher.mode(), ResearcherSummary::StakingOnly);
    QCOMPARE(researcher.modeText(), QStringLiteral("Staking Only"));

    interfaces::ResearcherSnapshot near_limit;
    near_limit.accrual_near_limit = 100;
    near_limit.accrual = 150;
    set(near_limit);
    QVERIFY(researcher.accrualNearLimit());
    near_limit.accrual = 50;
    set(near_limit);
    QVERIFY(!researcher.accrualNearLimit());

    interfaces::ResearcherSnapshot noncruncher;
    noncruncher.configured_for_noncruncher_mode = true;
    set(noncruncher);
    QVERIFY(!researcher.beaconVisible());
}

void QmlStatusAdapterTests::clipboardStrips()
{
    Clipboard clipboard(1);
    clipboard.setText(QStringLiteral("1") + QChar(0x2009) + QStringLiteral("234,5"));
    QCOMPARE(clipboard.text(), QStringLiteral("1234,5"));
}

void QmlStatusAdapterTests::statusReadSetsConnections()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    harness.node.m_node_count = 3;
    emit harness.client.numConnectionsChanged(3);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(status.numConnections(), 3);
    QCOMPARE(status.connectionToolTip(), QStringLiteral("3 active connection(s) to the Gridcoin network"));

    // No connection signal here: the block signal carries no count, so the new
    // count can arrive only with the status read that signal posts.
    harness.node.m_node_count = 7;
    emit harness.client.numBlocksChanged(1, 0);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(status.numConnections(), 7);
    QCOMPARE(status.connectionLevel(), 3);
    QCOMPARE(status.connectionToolTip(), QStringLiteral("7 active connection(s) to the Gridcoin network"));
}

void QmlStatusAdapterTests::syncProgressAndToolTip()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    // (connections, tip age, height, peers), through the status read each
    // signal posts.
    const auto row = [&](int connections, int64_t tip_age, int height, int peers) {
        harness.node.m_node_count = connections;
        harness.node.m_last_block_time = Now() - tip_age;
        emit harness.client.numConnectionsChanged(connections);
        emit harness.client.numBlocksChanged(height, peers);
        return SettleSerial(runner);
    };

    // The progress step: the largest of 25, 50, 75 and 90 that the height has
    // reached as a percentage of the peers' height, else 0.
    QVERIFY(row(8, 60, 10, 100));
    QCOMPARE(status.syncProgress(), 0);
    QVERIFY(row(8, 60, 25, 100));
    QCOMPARE(status.syncProgress(), 25);
    QVERIFY(row(8, 60, 49, 100));
    QCOMPARE(status.syncProgress(), 25);
    QVERIFY(row(8, 60, 50, 100));
    QCOMPARE(status.syncProgress(), 50);
    QVERIFY(row(8, 60, 75, 100));
    QCOMPARE(status.syncProgress(), 75);
    QVERIFY(row(8, 60, 89, 100));
    QCOMPARE(status.syncProgress(), 75);
    QVERIFY(row(8, 60, 90, 100));
    QCOMPARE(status.syncProgress(), 90);
    QVERIFY(row(8, 60, 100, 100));
    QCOMPARE(status.syncProgress(), 90);
    QVERIFY(row(8, 60, 5, 0));
    QCOMPARE(status.syncProgress(), 0);

    // The three tooltip forms. Each tip age is well inside one unit of the age
    // text, so a second of drift changes nothing.
    QVERIFY(row(0, 60, 100, 100));
    QCOMPARE(status.syncToolTip(), QStringLiteral("Sync: no connections."));
    QVERIFY(row(8, 5 * 60, 100, 100));
    QCOMPARE(status.syncToolTip(),
             QStringLiteral("<nobr>Up to date.<br>Processed 100 block(s) of transaction history.<br>"
                            "Last received block was generated 5 minute(s) ago.</nobr>"));
    QVERIFY(row(8, 91 * 60, 100, 100));
    QCOMPARE(status.syncToolTip(),
             QStringLiteral("<nobr>Catching up...<br>Processed 100 block(s) of transaction history.<br>"
                            "Last received block was generated 1 hour(s) ago.</nobr>"));
}

void QmlStatusAdapterTests::notStakingArm()
{
    // Before any attach the adapter reports what BitcoinGUI::setClientModel
    // starts with: not staking, with no coin weight.
    {
        ClientStatus fresh(1);
        QCOMPARE(fresh.stakingState(), ClientStatus::UnableToStake);
    }

    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    harness.staking.setErrors("E");
    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    // Not staking with no coin weight. The estimated time to stake is 0 in
    // every step, so the frequency reads "not available".
    emit harness.client.minerStatusChanged(false, 1000, 0, 0);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(status.stakingState(), ClientStatus::UnableToStake);
    QCOMPARE(status.stakingToolTip(), QStringLiteral("Unable to stake: E"));

    // Not staking with a coin weight.
    emit harness.client.minerStatusChanged(false, 1000, 20, 0);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(status.stakingState(), ClientStatus::NotStaking);
    QCOMPARE(status.stakingToolTip(),
             QStringLiteral("Not staking currently: E, <b>Estimated</b> staking frequency is not available."));

    // Staking: the whole tooltip, with each weight in its own place.
    emit harness.client.minerStatusChanged(true, 1000, 20, 0);
    QVERIFY(SettleSerial(runner));
    QCOMPARE(status.stakingState(), ClientStatus::Staking);
    QCOMPARE(status.stakingToolTip(),
             QStringLiteral("Staking.<br>Your weight is 20<br>Network weight is 1000<br>"
                            "<b>Estimated</b> staking frequency is not available."));
}

void QmlStatusAdapterTests::scraperStates()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    // A convergence time and no current event: every state below comes from
    // an event.
    interfaces::ScraperConvergenceSnapshot snapshot;
    snapshot.time = 1700000000;
    harness.node.setScraperSnapshot(snapshot);

    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    // Each event's tooltip marks its delivery; the state is assigned with it.
    const auto tooltip_is = [&](const QString& text) {
        return qml_test::PumpUntil([&] { return status.scraperToolTip() == text; }, 2s);
    };

    // The convergence tooltip goes on with the date and the excluded
    // projects, so only its start is compared.
    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Convergence), CT_NEW);
    QVERIFY2(qml_test::PumpUntil(
                 [&] {
                     return status.scraperToolTip().startsWith(
                         QStringLiteral("Scraper: Convergence achieved, date/time "));
                 },
                 2s),
             qPrintable(status.scraperToolTip()));
    QCOMPARE(status.scraperState(), ClientStatus::Converged);

    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Sleep), CT_NEW);
    QVERIFY2(tooltip_is(QStringLiteral("Scraper: superblock not needed - inactive.")),
             qPrintable(status.scraperToolTip()));
    QCOMPARE(status.scraperState(), ClientStatus::Inactive);

    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Stats), CT_UPDATED);
    QVERIFY2(tooltip_is(QStringLiteral("Scraper: downloading and processing stats.")),
             qPrintable(status.scraperToolTip()));
    QCOMPARE(status.scraperState(), ClientStatus::Downloading);

    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::SBContract), CT_DELETED);
    QVERIFY2(tooltip_is(QStringLiteral("Scraper: No convergence able to be achieved. Will retry in a few minutes.")),
             qPrintable(status.scraperToolTip()));
    QCOMPARE(status.scraperState(), ClientStatus::NoConvergence);

    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::OutOfSync), CT_UPDATING);
    QVERIFY2(tooltip_is(QStringLiteral("Scraper: waiting on wallet to sync.")), qPrintable(status.scraperToolTip()));
    QCOMPARE(status.scraperState(), ClientStatus::Waiting);
}

void QmlStatusAdapterTests::scraperReadsCoalesce()
{
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    interfaces::ScraperConvergenceSnapshot snapshot;
    snapshot.time = 1700000000;
    snapshot.current_event_type = static_cast<int>(scrapereventtypes::Convergence);
    snapshot.current_event_status = CT_NEW;
    harness.node.setScraperSnapshot(snapshot);

    QmlCallRunner runner(TestPolicy());
    ClientStatus status(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        status.detach();
    });
    status.attach(harness.client, runner);
    QVERIFY(SettleSerial(runner));

    // The attach read has been delivered, so no snapshot read is in flight.
    QVERIFY2(qml_test::PumpUntil([&] { return status.scraperState() == ClientStatus::Converged; }, 2s),
             "the attach read did not deliver the snapshot's current event");
    const int before = harness.node.m_scraper_calls;

    // Three events with no event-loop pass between them: the first posts a
    // read, and its delivery re-posts once, with the last event.
    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Sleep), CT_NEW);
    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::OutOfSync), CT_UPDATING);
    emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Stats), CT_NEW);
    QVERIFY(qml_test::PumpUntil([&] { return harness.node.m_scraper_calls - before >= 2; }, 2s));
    ReapPool();
    QCOMPARE(harness.node.m_scraper_calls - before, 2);

    // The re-posted read carried the last event, and nothing was posted after
    // it.
    const QString downloading = QStringLiteral("Scraper: downloading and processing stats.");
    QVERIFY2(qml_test::PumpUntil([&] { return status.scraperToolTip() == downloading; }, 2s),
             qPrintable(status.scraperToolTip()));
    ReapPool();
    QCOMPARE(harness.node.m_scraper_calls - before, 2);
}

void QmlStatusAdapterTests::roundedBalanceText()
{
    qml_test::ModelHarness harness;
    QmlCallRunner runner(TestPolicy());
    Clipboard clipboard(1);
    WalletSummary summary(1, clipboard);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        summary.detach();
    });
    summary.attach(harness.wallet_model, &harness.options, runner);
    QVERIFY(SettleSerial(runner));

    // The available balance alone, cut to its leading digits as
    // BitcoinUnits::formatOverviewRounded cuts it. The other three amounts are
    // not zero, so the total or the stake would read differently.
    const QString thin(QChar(0x2009));
    emit harness.wallet_model.balanceChanged(123456789012, 1 * COIN, 2 * COIN, 3 * COIN);
    QCOMPARE(summary.balanceRoundedText(), QStringLiteral("1") + thin + QStringLiteral("234.5678"));

    // The mask state follows the options model.
    QVERIFY(!summary.maskValues());
    summary.setMaskValues(true);
    QVERIFY(summary.maskValues());
}

void QmlStatusAdapterTests::lockStatesAndToolTips()
{
    qml_test::ModelHarness harness;
    harness.wallet().m_lock_state.crypted = true;
    harness.wallet().m_lock_state.unlocked_for_staking_only = true;
    QmlCallRunner runner(TestPolicy());
    Clipboard clipboard(1);
    WalletSummary summary(1, clipboard);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        summary.detach();
    });
    summary.attach(harness.wallet_model, &harness.options, runner);
    QVERIFY(SettleSerial(runner));

    // BitcoinGUI::setEncryptionStatus's texts; the two unlocked ones end with
    // a space there too.
    const QString staking_only =
        QStringLiteral("Wallet is <b>encrypted</b> and currently <b>unlocked for staking only</b> ");
    const QString locked = QStringLiteral("Wallet is <b>encrypted</b> and currently <b>locked</b>");
    const QString unlocked = QStringLiteral("Wallet is <b>encrypted</b> and currently <b>unlocked</b> ");
    const QString unencrypted = QStringLiteral("Wallet is <b>not encrypted</b>!");

    // (a) The seed read's lock state.
    QVERIFY(summary.encryptionStatusKnown());
    QCOMPARE(summary.encryptionStatus(), WalletSummary::UnlockedForStakingOnly);
    QCOMPARE(summary.encryptionToolTip(), staking_only);

    // (b) The model's signal, for each of its four values.
    emit harness.wallet_model.encryptionStatusChanged(WalletModel::Locked);
    QCOMPARE(summary.encryptionStatus(), WalletSummary::Locked);
    QCOMPARE(summary.encryptionToolTip(), locked);
    emit harness.wallet_model.encryptionStatusChanged(WalletModel::UnlockedForStakingOnly);
    QCOMPARE(summary.encryptionStatus(), WalletSummary::UnlockedForStakingOnly);
    QCOMPARE(summary.encryptionToolTip(), staking_only);
    emit harness.wallet_model.encryptionStatusChanged(WalletModel::Unlocked);
    QCOMPARE(summary.encryptionStatus(), WalletSummary::Unlocked);
    QCOMPARE(summary.encryptionToolTip(), unlocked);
    emit harness.wallet_model.encryptionStatusChanged(WalletModel::Unencrypted);
    QCOMPARE(summary.encryptionStatus(), WalletSummary::Unencrypted);
    QCOMPARE(summary.encryptionToolTip(), unencrypted);
}

namespace {

//! A translator that puts "<x>" before every string of ResearcherModel's and
//! QObject's contexts, so a test can tell whether a translated string was
//! escaped before it reached rich text.
class MarkupTranslator : public QTranslator
{
public:
    QString translate(const char* context, const char* source_text, const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        const QLatin1String name(context);
        if (name != QLatin1String("ResearcherModel") && name != QLatin1String("QObject")) return QString();
        return QStringLiteral("<x>") + QString::fromUtf8(source_text);
    }

    bool isEmpty() const override { return false; }
};

} // namespace

void QmlStatusAdapterTests::beaconToolTipEscapes()
{
    // The beacon status and the three durations are translated strings, so a
    // translation is the only way markup can reach them. The translator is
    // installed before the harness and removed after it.
    MarkupTranslator translator;
    QVERIFY(QCoreApplication::installTranslator(&translator));
    qml_test::ScopeExit remove_translator([&translator] { QCoreApplication::removeTranslator(&translator); });

    qml_test::ModelHarness harness;
    ResearcherSummary researcher(1);
    qml_test::ScopeExit detach([&] { researcher.detach(); });
    researcher.attach(harness.researcher, &harness.options);

    const auto set = [&](const interfaces::ResearcherSnapshot& snapshot) {
        harness.researcher_context.m_snapshot = snapshot;
        harness.researcher.refresh();
    };
    // The tooltip's own line breaks are its only markup.
    const auto no_raw_markup = [](const QString& tip) {
        return !QString(tip).remove(QStringLiteral("<br>")).contains(QLatin1Char('<'));
    };

    // An active beacon: the CPID, the age, the time to expiration and the
    // status.
    interfaces::ResearcherSnapshot active;
    active.has_eligible_projects = true;
    active.cpid = "<b>c</b>&";
    active.beacon_present = true;
    active.has_active_beacon = true;
    active.beacon_status = interfaces::BeaconStatus::ACTIVE;
    active.beacon_timestamp = Now() - 100000;
    active.beacon_age = 100000;
    active.time_to_beacon_expiration = 500000;
    set(active);
    QString tip = researcher.beaconToolTip();
    QVERIFY2(tip.contains(QStringLiteral("CPID: &lt;b&gt;c&lt;/b&gt;&amp;")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("Beacon age: &lt;x&gt;")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("Expires: &lt;x&gt;")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("&lt;x&gt;Beacon is active.")), qPrintable(tip));
    QVERIFY2(no_raw_markup(tip), qPrintable(tip));

    // A pending beacon: the time left to activate.
    interfaces::ResearcherSnapshot pending = active;
    pending.has_active_beacon = false;
    pending.has_pending_beacon = true;
    pending.pending_beacon_present = true;
    pending.time_to_pending_beacon_expiration = 18060;
    pending.beacon_status = interfaces::BeaconStatus::PENDING;
    set(pending);
    tip = researcher.beaconToolTip();
    QVERIFY2(tip.contains(QStringLiteral("CPID: &lt;b&gt;c&lt;/b&gt;&amp;")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("Time left to activate: &lt;x&gt;")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("&lt;x&gt;Pending beacon is awaiting network confirmation.")),
             qPrintable(tip));
    QVERIFY2(no_raw_markup(tip), qPrintable(tip));

    // An expired beacon: the age again, in its own form.
    interfaces::ResearcherSnapshot expired = active;
    expired.has_active_beacon = false;
    expired.beacon_expired = true;
    expired.beacon_status = interfaces::BeaconStatus::NO_BEACON;
    set(expired);
    tip = researcher.beaconToolTip();
    QVERIFY2(tip.contains(QStringLiteral("Beacon age: &lt;x&gt;")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("Current beacon expired!")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("&lt;x&gt;No active beacon.")), qPrintable(tip));
    QVERIFY2(no_raw_markup(tip), qPrintable(tip));

    // No beacon: the status alone.
    interfaces::ResearcherSnapshot none;
    none.cpid = "<b>c</b>&";
    none.beacon_status = interfaces::BeaconStatus::NO_BEACON;
    set(none);
    QCOMPARE(researcher.beaconToolTip(), QStringLiteral("&lt;x&gt;No active beacon."));
}

void QmlStatusAdapterTests::unitChangeEmitsDataChanged()
{
    qml_test::ModelHarness harness;
    std::vector<TransactionRecord> rows(2);
    rows[0].address = "first";
    rows[0].credit = COIN;
    rows[1].address = "second";
    rows[1].credit = 2 * COIN;
    harness.tx_source.setRows(GRC::VIEW_OVERVIEW, rows);

    QmlCallRunner runner(TestPolicy());
    RecentTransactions recent(1);
    qml_test::ScopeExit detach([&] {
        runner.drain();
        recent.detach();
    });
    recent.attach(harness.wallet_model, &harness.options, runner);
    QCOMPARE(recent.rowCount(), 2);
    QCOMPARE(recent.data(recent.index(0, 0), RecentTransactions::AmountTextRole).toString(),
             QStringLiteral("1.00 GRC"));

    // A unit change re-reads the amount text of every row: one dataChanged
    // over all the rows, for that role alone.
    QSignalSpy changed(&recent, &QAbstractItemModel::dataChanged);
    harness.options.setData(harness.options.index(OptionsModel::DisplayUnit), BitcoinUnits::mBTC, Qt::EditRole);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toModelIndex(), recent.index(0, 0));
    QCOMPARE(changed.at(0).at(1).toModelIndex(), recent.index(1, 0));
    QCOMPARE(changed.at(0).at(2).value<QList<int>>(), QList<int>{RecentTransactions::AmountTextRole});
    const QString thin(QChar(0x2009));
    QCOMPARE(recent.data(recent.index(0, 0), RecentTransactions::AmountTextRole).toString(),
             QStringLiteral("1") + thin + QStringLiteral("000.00 mGRC"));
}

void QmlStatusAdapterTests::scraperStartsWaiting()
{
    const QString waiting = QStringLiteral("Scraper: waiting on wallet to sync.");

    // The fake node's snapshot as it starts: no scraper event since the node
    // started.
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        // Counts the scraper group's change signal. Declared before the
        // adapter, so it outlives every delivery.
        int changes = 0;
        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        QCOMPARE(status.scraperToolTip(), QString());
        QObject::connect(&status, &ClientStatus::scraperChanged, &status, [&changes] { ++changes; });
        status.attach(harness.client, runner);

        // The attach read finds no event and starts as
        // BitcoinGUI::setClientModel starts the scraper icon: waiting, with
        // the waiting tooltip. The state does not change, so the one change
        // signal is the tooltip's.
        QVERIFY2(qml_test::PumpUntil([&] { return status.scraperToolTip() == waiting; }, 2s),
                 qPrintable(status.scraperToolTip()));
        QCOMPARE(status.scraperState(), ClientStatus::Waiting);
        QCOMPARE(changes, 1);
    }

    // The node's last event at attach is the out-of-sync one a node signals
    // while its tip is old: the attach read starts the same way.
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        interfaces::ScraperConvergenceSnapshot snapshot;
        snapshot.current_event_type = static_cast<int>(scrapereventtypes::OutOfSync);
        snapshot.current_event_status = CT_NEW;
        harness.node.setScraperSnapshot(snapshot);
        int changes = 0;
        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        QObject::connect(&status, &ClientStatus::scraperChanged, &status, [&changes] { ++changes; });
        status.attach(harness.client, runner);
        QVERIFY2(qml_test::PumpUntil([&] { return status.scraperToolTip() == waiting; }, 2s),
                 qPrintable(status.scraperToolTip()));
        QCOMPARE(status.scraperState(), ClientStatus::Waiting);
        QCOMPARE(changes, 1);
    }

    // The same event as a signal, after the attach read, changes nothing, as
    // in BitcoinGUI::updateScraperIcon.
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        int changes = 0;
        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        QObject::connect(&status, &ClientStatus::scraperChanged, &status, [&changes] { ++changes; });
        status.attach(harness.client, runner);
        QVERIFY2(qml_test::PumpUntil([&] { return status.scraperToolTip() == waiting; }, 2s),
                 qPrintable(status.scraperToolTip()));

        const QString inactive = QStringLiteral("Scraper: superblock not needed - inactive.");
        emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Sleep), CT_NEW);
        QVERIFY2(qml_test::PumpUntil([&] { return status.scraperToolTip() == inactive; }, 2s),
                 qPrintable(status.scraperToolTip()));
        QCOMPARE(status.scraperState(), ClientStatus::Inactive);

        // Two signals with no event-loop pass between them: the out-of-sync
        // one posts a read whose delivery changes nothing, and that delivery
        // posts the second, which sets the downloading state. One change.
        const int before = changes;
        const QString downloading = QStringLiteral("Scraper: downloading and processing stats.");
        emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::OutOfSync), CT_NEW);
        emit harness.client.updateScraperStatus(static_cast<int>(scrapereventtypes::Stats), CT_NEW);
        QVERIFY2(qml_test::PumpUntil([&] { return status.scraperToolTip() == downloading; }, 2s),
                 qPrintable(status.scraperToolTip()));
        QCOMPARE(status.scraperState(), ClientStatus::Downloading);
        QCOMPARE(changes - before, 1);
    }
}

void QmlStatusAdapterTests::stakingToolTipFollowsMask()
{
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.staking.setErrors("E");
        // Counts the staking group's change signal. Declared before the
        // adapter, so it outlives every delivery.
        int changes = 0;
        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        QObject::connect(&status, &ClientStatus::stakingChanged, &status, [&changes] { ++changes; });
        status.attach(harness.client, runner);
        // With the mask off at attach nothing formats the tooltip before the
        // hydrate is delivered.
        QCOMPARE(status.stakingToolTip(), QString());
        QCOMPARE(changes, 0);
        QVERIFY(SettleSerial(runner));

        // An estimated time to stake of one day reads "1.01 times per day".
        const QString shown = QStringLiteral("Staking.<br>Your weight is 20<br>Network weight is 1000<br>"
                                             "<b>Estimated</b> staking frequency is 1.01 times per day.");
        const QString masked = QStringLiteral("Staking.<br>Your weight is #.##<br>Network weight is 1000<br>"
                                              "<b>Estimated</b> staking frequency is #.##.");
        emit harness.client.minerStatusChanged(true, 1000, 20, 86400);
        QVERIFY(SettleSerial(runner));
        QCOMPARE(status.stakingToolTip(), shown);

        // Each switch of the mask formats the tooltip again, with one change
        // signal.
        const int before = changes;
        harness.options.setMaskValues(true);
        QCOMPARE(status.stakingToolTip(), masked);
        QCOMPARE(changes - before, 1);
        harness.options.setMaskValues(false);
        QCOMPARE(status.stakingToolTip(), shown);
        QCOMPARE(changes - before, 2);

        // While masked, a new weight from the miner and the status read its
        // signal posts leave the tooltip as it was.
        harness.options.setMaskValues(true);
        emit harness.client.minerStatusChanged(true, 1000, 30, 86400);
        QVERIFY(SettleSerial(runner));
        QCOMPARE(status.coinWeight(), 30.0);
        QCOMPARE(status.stakingToolTip(), masked);
        harness.options.setMaskValues(false);
        QCOMPARE(status.stakingToolTip(),
                 QStringLiteral("Staking.<br>Your weight is 30<br>Network weight is 1000<br>"
                                "<b>Estimated</b> staking frequency is 1.01 times per day."));

        // Not staking with a coin weight: the estimate is masked there too.
        const QString idle_shown =
            QStringLiteral("Not staking currently: E, <b>Estimated</b> staking frequency is 1.01 times per day.");
        const QString idle_masked =
            QStringLiteral("Not staking currently: E, <b>Estimated</b> staking frequency is #.##.");
        emit harness.client.minerStatusChanged(false, 1000, 30, 86400);
        QVERIFY(SettleSerial(runner));
        QCOMPARE(status.stakingToolTip(), idle_shown);
        harness.options.setMaskValues(true);
        QCOMPARE(status.stakingToolTip(), idle_masked);
        harness.options.setMaskValues(false);
        QCOMPARE(status.stakingToolTip(), idle_shown);
    }

    // The mask already on at attach: the hydrate's tooltip is masked.
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.staking.m_staking = true;
        harness.staking.m_coin_weight = 42;
        harness.staking.m_network_weight = 400;
        harness.staking.m_etts = 86400;
        harness.options.setMaskValues(true);
        QmlCallRunner runner(TestPolicy());
        ClientStatus status(1);
        qml_test::ScopeExit detach([&] {
            runner.drain();
            status.detach();
        });
        status.attach(harness.client, runner);
        // With the mask on at attach the switch formats the tooltip once,
        // from the values the adapter starts with.
        QCOMPARE(status.stakingToolTip(), QStringLiteral("Unable to stake: "));
        QVERIFY(SettleSerial(runner));
        QCOMPARE(status.coinWeight(), 42.0);
        // ClientModel::getNetWeight: the network weight / 80.
        QCOMPARE(status.stakingToolTip(),
                 QStringLiteral("Staking.<br>Your weight is #.##<br>Network weight is 5<br>"
                                "<b>Estimated</b> staking frequency is #.##."));
    }
}
