// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "main.h"
#include "gridcoin/backup.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/gridcoin.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/support/block_finder.h"
#include "gridcoin/tally.h"
#include "gridcoin/upgrade.h"
#include "init.h"
#include "scheduler.h"
#include "ui_interface.h"

using namespace GRC;

extern bool fExplorer;
extern unsigned int nScraperSleep;
extern unsigned int nActiveBeforeSB;
extern bool fScraperActive;

extern ThreadHandler* netThreads;

void Scraper(bool bSingleShot = false);
void ScraperSubscriber();

namespace {
//!
//! \brief Initialize the service that creates and validate superblocks.
//!
//! \param pindexBest Block index of the tip of the chain.
//!
void InitializeSuperblockQuorum(const CBlockIndex* pindexBest)
{
    LogPrintf("Gridcoin: quorum is %sactive", Quorum::Active() ? "" : "in");

    if (IsV9Enabled(pindexBest->nHeight)) {
        LogPrintf("Gridcoin: Loading superblock cache...");
        uiInterface.InitMessage(_("Loading superblock cache..."));

        Quorum::LoadSuperblockIndex(pindexBest);
    }
}

//!
//! \brief Initialize the service that tracks research rewards.
//!
//! \param pindexBest Block index of the tip of the chain.
//!
//! \return \c false if a problem occurs while loading the stored accrual state.
//!
bool InitializeResearchRewardAccounting(CBlockIndex* pindexBest)
{
    LogPrintf("Gridcoin: initializing research reward accounting...");
    uiInterface.InitMessage(_("Initializing research reward accounting..."));

    const int64_t start_height = Params().GetConsensus().ResearchAgeHeight;

    if (!Tally::Initialize(BlockFinder().FindByHeight(start_height))) {
        return error("Failed to initialize tally.");
    }

    if (pindexBest->nVersion <= 10) {
        LogPrint(BCLog::LogFlags::TALLY, "Gridcoin: loading network averages...");
        uiInterface.InitMessage(_("Loading Network Averages..."));

        Tally::LegacyRecount(Tally::FindLegacyTrigger(pindexBest));
    }

    return true;
}

//!
//! \brief Reload historical contract state from the blockchain.
//!
//! \param pindexBest Block index of the tip of the chain.
//!
void InitializeContracts(const CBlockIndex* pindexBest)
{
    LogPrintf("Gridcoin: replaying contracts...");
    uiInterface.InitMessage(_("Replaying contracts..."));

    ReplayContracts(pindexBest);
}

//!
//! \brief Set up the user's CPID for participation in the research reward
//! protocol.
//!
void InitializeResearcherContext()
{
    LogPrintf("Gridcoin: initializing local researcher context...");
    uiInterface.InitMessage(_("Initializing local researcher context..."));

    Researcher::Initialize();

    if (!pwalletMain->IsLocked()) {
        Researcher::Get()->ImportBeaconKeysFromConfig(pwalletMain);
    }
}

//!
//! \brief Run the scraper thread.
//!
//! \param parg Unused.
//!
void ThreadScraper(void* parg)
{
    LogPrint(BCLog::LogFlags::NOISY, "ThreadSraper starting");

    try {
        fScraperActive = true;
        Scraper(false);
    } catch (std::exception& e) {
        fScraperActive = false;
        PrintException(&e, "ThreadScraper()");
    } catch(boost::thread_interrupted&) {
        fScraperActive = false;
        LogPrintf("ThreadScraper exited (interrupt)");
        return;
    } catch (...) {
        fScraperActive = false;
        PrintException(nullptr, "ThreadScraper()");
    }

    fScraperActive = false;
    LogPrintf("ThreadScraper exited");
}

//!
//! \brief Run the scraper subscriber thread.
//!
//! \param parg Unused.
//!
void ThreadScraperSubscriber(void* parg)
{
    LogPrint(BCLog::LogFlags::NOISY, "ThreadScraperSubscriber starting");

    try {
        ScraperSubscriber();
    } catch (std::exception& e) {
        PrintException(&e, "ThreadScraperSubscriber()");
    } catch(boost::thread_interrupted&) {
        LogPrintf("ThreadScraperSubscriber exited (interrupt)");
        return;
    } catch (...) {
        PrintException(nullptr, "ThreadScraperSubscriber()");
    }

    LogPrintf("ThreadScraperSubscriber exited");
}

//!
//! \brief Configure and initialize the scraper system.
//!
void InitializeScraper()
{
    // Default to 300 sec (5 min), clamp to 60 minimum, 600 maximum - converted to milliseconds.
    nScraperSleep = std::min<int64_t>(std::max<int64_t>(GetArg("-scrapersleep", 300), 60), 600) * 1000;
    // Default to 7200 sec (4 hrs), clamp to 300 minimum, 86400 maximum (meaning active all of the time).
    nActiveBeforeSB = std::min<int64_t>(std::max<int64_t>(GetArg("-activebeforesb", 14400), 300), 86400);

    // Run the scraper or subscriber housekeeping thread, but not both. The
    // subscriber housekeeping thread checks if the flag for the scraper thread
    // is true, and basically becomes a no-op, but it is silly to run it if the
    // scraper thread is running. The scraper thread does all of the same
    // housekeeping functions as the subscriber housekeeping thread.
    //
    // For example. gridcoinresearch(d) with no args will run the subscriber
    // but not the scraper.
    // gridcoinresearch(d) -scraper will run the scraper but not the subscriber.
    // gridcoinresearch(d) -scraper -usenewnn will run both the scraper and the
    // subscriber.
    // -disablenn overrides the -usenewnn directive.
    if (GetBoolArg("-scraper", false)) {
        LogPrintf("Gridcoin: scraper enabled");

        if (!netThreads->createThread(ThreadScraper, nullptr, "ThreadScraper")) {
            LogPrintf("ERROR: createThread(ThreadScraper) failed");
        }
    } else {
        LogPrintf("Gridcoin: scraper disabled");
        LogPrintf("Gridcoin: scraper subscriber housekeeping thread enabled");

        if (!netThreads->createThread(ThreadScraperSubscriber, nullptr, "ScraperSubscriber")) {
            LogPrintf("ERROR: createThread(ScraperSubscriber) failed");
        }
    }
}

//!
//! \brief Enable explorer functionality for the scraper if requested.
//!
void InitializeExplorerFeatures()
{
    // If -disablenn is NOT specified or set to false...
    if (!GetBoolArg("-disablenn", false)) {
        // Then if -scraper is specified (set to true)...
        if (GetBoolArg("-scraper", false)) {
            // Activate explorer extended features if -explorer is set
            if (GetBoolArg("-explorer", false)) fExplorer = true;
        }
    }
}

//!
//! \brief Set up the background job that creates backup files.
//!
//! \param scheduler Scheduler instance to register jobs with.
//!
void ScheduleBackups(CScheduler& scheduler)
{
    if (!BackupsEnabled()) {
        return;
    }

    // Run the backup job at a rate of 4x the configured backup interval
    // in case the wallet becomes busy when the job runs. This job skips
    // a cycle when it encounters lock contention or when a cycle occurs
    // sooner than the requested interval:
    //
    scheduler.scheduleEvery(RunBackupJob, GetBackupInterval() * 1000 / 4);

    // Run the backup job on start-up in case the wallet started after a
    // long period of downtime. Some usage patterns may cause the wallet
    // to start and shutdown frequently without producing a backup if we
    // only create backups from the scheduler cycles. This is a no-op if
    // the wallet contains a stored backup timestamp later than the next
    // scheduled backup interval:
    //
    scheduler.scheduleFromNow(RunBackupJob, 60 * 1000);
}

//!
//! \brief Set up the background job that checks for new Gridcoin versions.
//!
//! \param scheduler Scheduler instance to register jobs with.
//!
void ScheduleUpdateChecks(CScheduler& scheduler)
{
    if (fTestNet) {
        LogPrintf("Gridcoin: update checks disabled for testnet");
        return;
    }

    if (GetBoolArg("-disableupdatecheck", false)) {
        LogPrintf("Gridcoin: update checks disabled by configuration");
        return;
    }

    int64_t hours = GetArg("-updatecheckinterval", 24);

    if (hours < 1) {
        LogPrintf("ERROR: invalid -updatecheckinterval: %s. Using default...",
            GetArg("-updatecheckinterval", ""));
        hours = 24;
    }

    LogPrintf("Gridcoin: checking for updates every %" PRId64 " hours", hours);

    scheduler.scheduleEvery([]{
        g_UpdateChecker->CheckForLatestUpdate();
    }, hours * 60 * 60 * 1000);

    // Schedule a start-up check one minute from now:
    scheduler.scheduleFromNow([]{
        g_UpdateChecker->CheckForLatestUpdate();
    }, 60 * 1000);
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Variables
// -----------------------------------------------------------------------------

std::unique_ptr<Upgrade> g_UpdateChecker;
bool fSnapshotRequest = false;

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

bool GRC::Initialize(CBlockIndex* pindexBest)
{
    LogPrintf("Gridcoin: initializing...");

    InitializeSuperblockQuorum(pindexBest);

    if (!InitializeResearchRewardAccounting(pindexBest)) {
        return false;
    }

    InitializeContracts(pindexBest);
    InitializeResearcherContext();

    // The scraper is run on the netThreads group, because it shares data structures
    // with scraper_net, which is run as part of the network node threads.
    InitializeScraper();
    InitializeExplorerFeatures();

    return true;
}

void GRC::ScheduleBackgroundJobs(CScheduler& scheduler)
{
    // Primitive, but this is what the scraper does in the scraper housekeeping
    // loop. It checks to see if the logs need to be archived by default every
    // 5 mins. Note that passing false to the archive function means that if we
    // have not crossed over the day boundary, it does nothing, so this is a
    // very inexpensive call. Also if -logarchivedaily is set to false, then
    // this will be a no-op.
    scheduler.scheduleEvery([]{
        fs::path plogfile_out;
        LogInstance().archive(false, plogfile_out);
    }, 300 * 1000);

    scheduler.scheduleEvery(Researcher::RunRenewBeaconJob, 4 * 60 * 60 * 1000);

    ScheduleBackups(scheduler);
    ScheduleUpdateChecks(scheduler);
}
