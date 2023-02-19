// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "main.h"
#include "util/threadnames.h"
#include "gridcoin/backup.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/gridcoin.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/support/block_finder.h"
#include "gridcoin/tally.h"
#include "gridcoin/upgrade.h"
#include "gridcoin/beacon.h"
#include "init.h"
#include "scheduler.h"
#include "node/ui_interface.h"

using namespace GRC;

extern CCriticalSection cs_ScraperGlobals;
extern bool fExplorer;
extern unsigned int nScraperSleep;
extern unsigned int nActiveBeforeSB;
extern bool fScraperActive;

void Scraper(bool bSingleShot = false);
void ScraperSubscriber();

namespace {
//!
//! \brief Display a message that warns about a corrupted blockchain database.
//!
//! For the GUI, this displays a modal dialog. It prints a message to the log
//! and to stderr on headless nodes.
//!
void ShowChainCorruptedMessage()
{
    uiInterface.ThreadSafeMessageBox(
        _("WARNING: Blockchain data may be corrupted.\n\n"
            "Gridcoin detected bad index entries. This may occur because of an "
            "unexpected exit, a power failure, or a late software upgrade.\n\n"
            "Please exit Gridcoin, open the data directory, and delete:\n"
            " - the blk****.dat files\n"
            " - the txleveldb folder\n\n"
            "Your wallet will re-download the blockchain. Your balance may "
            "appear incorrect until the synchronization finishes.\n" ),
        "Gridcoin",
        CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
}

//!
//! \brief Compare the block index to the hardened checkpoints and display a
//! warning message and exit if an entry does not match.
//!
//! \param pindexBest Block index entry for the tip of the chain.
//!
//! \return \c false if a checkpoint does not match its corresponding block
//! index entry.
//!
bool VerifyCheckpoints(const CBlockIndex* const pindexBest)
{
    LogPrintf("Gridcoin: verifying checkpoints...");
    uiInterface.InitMessage(_("Verifying checkpoints..."));

    for (const auto& checkpoint_pair : Params().Checkpoints().mapCheckpoints) {
        if (checkpoint_pair.first > pindexBest->nHeight) {
            return true;
        }

        const auto iter_pair = mapBlockIndex.find(checkpoint_pair.second);

        if (iter_pair == mapBlockIndex.end()
            || iter_pair->second == nullptr
            || !iter_pair->second->IsInMainChain()
            || iter_pair->second->nHeight != checkpoint_pair.first)
        {
            // We could rewind the blockchain, but doing so might take longer
            // than a genesis sync for deep reorganizations. For now, we only
            // show the message, exit, and let the user choose a resolution:
            //
            LogPrintf("WARNING: checkpoint mismatch at %d", checkpoint_pair.first);
            ShowChainCorruptedMessage();

            return false;
        }
    }

    return true;
}

//!
//! \brief Initialize the service that creates and validate superblocks.
//!
//! \param pindexBest Block index of the tip of the chain.
//!
void InitializeSuperblockQuorum(const CBlockIndex* pindexBest)
{
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

    if (!Tally::Initialize(BlockFinder::FindByHeight(start_height))) {
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
void InitializeContracts(CBlockIndex* pindexBest)
{
    LogPrintf("Gridcoin: Loading beacon history...");
    uiInterface.InitMessage(_("Loading beacon history..."));

    BeaconRegistry& beacons = GetBeaconRegistry();

    // If the clearbeaconhistory argument is provided, then clear everything from the beacon registry,
    // including the beacon_db and beacon key type elements from LevelDB.
    if (gArgs.GetBoolArg("-clearbeaconhistory", false))
    {
        beacons.Reset();
    }

    LogPrintf("Gridcoin: Initializing beacon registry from stored history...");
    uiInterface.InitMessage(_("Initializing beacon registry from stored history..."));
    int beacon_db_height = beacons.Initialize();

    if (beacon_db_height > 0)
    {
        LogPrintf("Gridcoin: beacon history loaded through height = %i.", beacon_db_height);
    }
    else
    {
        LogPrintf("Gridcoin: beacon history load not successful. Will initialize from contract replay.");
    }

    LogPrintf("Gridcoin: Loading scraper entry history...");
    uiInterface.InitMessage(_("Loading scraper entry history..."));

    ScraperRegistry& scrapers = GetScraperRegistry();

    // If the clearscraperhistory argument is provided, then clear everything from the beacon registry,
    // including the beacon_db and beacon key type elements from LevelDB.
    if (gArgs.GetBoolArg("-clearscraperentryhistory", false))
    {
        scrapers.Reset();
    }

    LogPrintf("Gridcoin: Initializing scraper entry from stored history...");
    uiInterface.InitMessage(_("Initializing scraper entry from stored history..."));
    int scraper_db_height = scrapers.Initialize();

    if (scraper_db_height > 0)
    {
        LogPrintf("Gridcoin: scraper entry history loaded through height = %i.", beacon_db_height);
    }
    else
    {
        LogPrintf("Gridcoin: scraper entry history load not successful. Will initialize from contract replay.");
    }

    LogPrintf("Gridcoin: replaying contracts...");
    uiInterface.InitMessage(_("Replaying contracts..."));

    CBlockIndex* pindex_start = GRC::BlockFinder::FindByMinTime(pindexBest->nTime - Beacon::MAX_AGE);

    const int& V11_height = Params().GetConsensus().BlockV11Height;
    const int& lookback_window_low_height = pindex_start->nHeight;

    // This tricky clamp ensures the correct start height for the contract replay. Note that the current
    // implementation will skip beacon and scraper entry contracts that overlap the already loaded history. See
    // ReplayContracts. The worst case replay is a window that starts at V11_height and extends to current height.
    // This is the replay that will be encountered when starting a wallet that was in sync with this code, and the
    // head of the chain is more than MAX AGE above the V11Height. When the contracts are replayed, the beacon db
    // and scraper entry db will then be initialized and the controlling window will be consistent with MAX_AGE
    // on restarts and reorgs for beacons and scraper entries.
    int min_db_height = std::min(beacon_db_height, scraper_db_height);
    const int& start_height = std::min(std::max(min_db_height, V11_height), lookback_window_low_height);

    LogPrintf("Gridcoin: Starting contract replay from height %i.", start_height);

    CBlockIndex* pblock_index = mapBlockIndex[hashBestChain];

    while (pblock_index->nHeight > start_height)
    {
        pblock_index = pblock_index->pprev;
    }

    // Reset pindex_start to the index for the block at start_height
    pindex_start = pblock_index;

    // The replay contract window here may overlap with the beacon db coverage. Logic is now included in
    // the ApplyContracts to ignore beacon contracts that have already been covered by the beacon db.
    ReplayContracts(pindexBest, pindex_start);
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
}

//!
//! \brief Run the scraper thread.
//!
//! \param parg Unused.
//!
void ThreadScraper(void* parg)
{
    RenameThread("grc-scraper");
    util::ThreadSetInternalName("grc-scraper");

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
    RenameThread("grc-scrapersubscriber");
    util::ThreadSetInternalName("grc-scrapersubscriber");

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
//! \param threads Used to start the scraper housekeeping threads.
//!
void InitializeScraper(ThreadHandlerPtr threads)
{
    {
        LOCK(cs_ScraperGlobals);

        // Default to 300 sec (5 min), clamp to 60 minimum, 600 maximum - converted to milliseconds.
        nScraperSleep = std::clamp<int64_t>(gArgs.GetArg("-scrapersleep", 300), 60, 600) * 1000;
        // Default to 14400 sec (4 hrs), clamp to 300 minimum, 86400 maximum (meaning active all of the time).
        nActiveBeforeSB = std::clamp<int64_t>(gArgs.GetArg("-activebeforesb", 14400), 300, 86400);
    }

    // Run the scraper or subscriber housekeeping thread, but not both. The
    // subscriber housekeeping thread checks if the flag for the scraper thread
    // is true, and basically becomes a no-op, but it is silly to run it if the
    // scraper thread is running. The scraper thread does all of the same
    // housekeeping functions as the subscriber housekeeping thread.
    //
    // For example. gridcoinresearch(d) with no args will run the subscriber
    // but not the scraper.
    // gridcoinresearch(d) -scraper will run the scraper but not the subscriber.
    if (gArgs.GetBoolArg("-scraper", false)) {
        LogPrintf("Gridcoin: scraper enabled");

        if (!threads->createThread(ThreadScraper, nullptr, "ThreadScraper")) {
            LogPrintf("ERROR: createThread(ThreadScraper) failed");
        }
    } else {
        LogPrintf("Gridcoin: scraper disabled");
        LogPrintf("Gridcoin: scraper subscriber housekeeping thread enabled");

        if (!threads->createThread(ThreadScraperSubscriber, nullptr, "ScraperSubscriber")) {
            LogPrintf("ERROR: createThread(ScraperSubscriber) failed");
        }
    }
}

//!
//! \brief Enable explorer functionality for the scraper if requested.
//!
void InitializeExplorerFeatures()
{
    LOCK(cs_ScraperGlobals);

    fExplorer = gArgs.GetBoolArg("-scraper", false) && gArgs.GetBoolArg("-explorer", false);
}

//!
//! \brief Check the block index for unusual entries.
//!
//! This runs a simple check on the entries in the block index to determine
//! whether the index contains invalid state caused by an unclean shutdown.
//! This condition was originally detected by an assertion in a routine for
//! stake modifier checksum verification. Because Gridcoin removed modifier
//! checksums and checkpoints, we reinstate that assertion here as a formal
//! inspection.
//!
//! This function checks that no block index entries contain a null pointer
//! to a previous block. The symptom may indicate a deeper problem that can
//! be resolved by tuning disk synchronization in LevelDB. Until then, this
//! heuristic has proven itself to be effective for identifying a corrupted
//! database.
//!
void CheckBlockIndexJob()
{
    LogPrintf("Gridcoin: checking block index...");

    bool corrupted = false;

    if (pindexGenesisBlock) {
        LOCK(cs_main);

        for (const auto& index_pair : mapBlockIndex) {
            const CBlockIndex* const pindex = index_pair.second;

            if (!pindex || !(pindex->pprev || pindex == pindexGenesisBlock)) {
                corrupted = true;
                break;
            }
        }
    }

    if (!corrupted) {
        LogPrintf("Gridcoin: block index is clean");
        return;
    }

    ShowChainCorruptedMessage();
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
    scheduler.scheduleEvery(RunBackupJob, std::chrono::seconds{GetBackupInterval() / 4});

    // Run the backup job on start-up in case the wallet started after a
    // long period of downtime. Some usage patterns may cause the wallet
    // to start and shutdown frequently without producing a backup if we
    // only create backups from the scheduler cycles. This is a no-op if
    // the wallet contains a stored backup timestamp later than the next
    // scheduled backup interval:
    //
    scheduler.scheduleFromNow(RunBackupJob, std::chrono::seconds{60});
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

    if (gArgs.GetBoolArg("-disableupdatecheck", false)) {
        LogPrintf("Gridcoin: update checks disabled by configuration");
        return;
    }

    int64_t hours = gArgs.GetArg("-updatecheckinterval", 5 * 24);

    if (hours < 1) {
        LogPrintf("ERROR: invalid -updatecheckinterval: %s. Using default...",
            gArgs.GetArg("-updatecheckinterval", ""));
        hours = 24;
    }

    LogPrintf("Gridcoin: checking for updates every %" PRId64 " hours", hours);

    scheduler.scheduleEvery([]{
        g_UpdateChecker->ScheduledUpdateCheck();
    }, std::chrono::hours{hours});

    // Schedule a start-up check one minute from now:
    scheduler.scheduleFromNow([]{
        g_UpdateChecker->ScheduledUpdateCheck();
    }, std::chrono::minutes{1});
}

void ScheduleBeaconDBPassivation(CScheduler& scheduler)
{
    // Run beacon database passivation every 5 minutes. This is a very thin call most of the time.
    // Please see the PassivateDB function and passivate_db.
    scheduler.scheduleEvery(BeaconRegistry::RunBeaconDBPassivation, std::chrono::minutes{5});
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Variables
// -----------------------------------------------------------------------------

std::unique_ptr<Upgrade> g_UpdateChecker;
bool fSnapshotRequest = false;
bool fResetBlockchainRequest = false;

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

bool GRC::Initialize(ThreadHandlerPtr threads, CBlockIndex* pindexBest)
{
    LogPrintf("Gridcoin: initializing...");

    if (!VerifyCheckpoints(pindexBest)) {
        return false;
    }

    InitializeSuperblockQuorum(pindexBest);

    // This has to be before InitializeResearchRewardAccounting, because we need the beacon registry
    // populated.
    InitializeContracts(pindexBest);

    if (!InitializeResearchRewardAccounting(pindexBest)) {
        return false;
    }

    InitializeResearcherContext();
    InitializeScraper(threads);
    InitializeExplorerFeatures();

    return true;
}

void GRC::CloseResearcherRegistryFile()
{
    Tally::CloseRegistryFile();
}

void GRC::ScheduleBackgroundJobs(CScheduler& scheduler)
{
    scheduler.schedule(CheckBlockIndexJob, std::chrono::system_clock::now());

    // Primitive, but this is what the scraper does in the scraper housekeeping
    // loop. It checks to see if the logs need to be archived by default every
    // 5 mins. Note that passing false to the archive function means that if we
    // have not crossed over the day boundary, it does nothing, so this is a
    // very inexpensive call. Also if -logarchivedaily is set to false, then
    // this will be a no-op.
    scheduler.scheduleEvery([]{
        fs::path plogfile_out;
        LogInstance().archive(false, plogfile_out);
    }, std::chrono::seconds{300});

    scheduler.scheduleEvery(Researcher::RunRenewBeaconJob, std::chrono::hours{4});

    ScheduleBackups(scheduler);
    ScheduleUpdateChecks(scheduler);
    ScheduleBeaconDBPassivation(scheduler);
}

bool GRC::CleanConfig() {
    fsbridge::ifstream orig_config(GetConfigFile());
    if (!orig_config.good()) {
        return false;
    }

    bool commit{false};
    std::vector<std::string> new_config_lines;

    std::string obsolete_keys[] = {"privatekey", "AutoUpgrade", "cpumining", "enablespeech",
                                   "NEURAL_", "PrimaryCPID", "publickey", "SessionGuid",
                                   "suppressupgrade", "tickers", "UpdatingLeaderboard"};

    std::string line;
    while (std::getline(orig_config, line)) {
        if (!commit) {
            commit |= line.rfind(obsolete_keys[0], 0) != std::string::npos;
        }

        for (const auto& key : obsolete_keys) {
            if (line.rfind(key, 0) != std::string::npos) {
                goto skip;
            }
        }
        new_config_lines.push_back(line);
skip:;
    }
    orig_config.close();

    if (commit) {
        if (!BackupConfigFile(GetBackupFilename("gridcoinresearch.conf"))) {
            return error("%s: Config backup failed.", __func__);
        }

        try {
            fs::path new_config_path(GetConfigFile().replace_extension("conf~"));
            fsbridge::ofstream new_config_file(new_config_path);

            std::ostream_iterator<std::string> out(new_config_file, "\n");
            std::copy(new_config_lines.begin(), new_config_lines.end(), out);

            new_config_file.close();
            fs::rename(new_config_path, GetConfigFile());
        } catch (const fs::filesystem_error& e) {
            return error("%s: Error saving config: %s", __func__, e.what());
        }
    }

    return true;
}

