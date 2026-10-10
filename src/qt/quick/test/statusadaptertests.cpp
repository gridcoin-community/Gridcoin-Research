// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "statusadaptertests.h"

#include "interfacefakes_qml.h"
#include "modelharness.h"
#include "qmltestutil.h"

#include <interfaces/node.h>
#include <qt/guiutil.h>
#include <qt/quick/clientstatus.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <util.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

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
