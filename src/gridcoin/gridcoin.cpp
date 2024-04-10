// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "main.h"
#include "util/threadnames.h"
#include "gridcoin/backup.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
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
extern bool fQtActive;

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
    fResetBlockchainRequest = true;

    if (fQtActive) {
        uiInterface.ThreadSafeMessageBox(
            _("ERROR: Checkpoint mismatch: Blockchain data may be corrupted.\n\n"
              "Gridcoin detected bad index entries. This may occur because of a "
              "late software upgrade, unexpected exit, or a power failure. "
              "Your blockchain data is being reset and your wallet will resync "
              "from genesis when you restart. Your balance may appear incorrect "
              "until the synchronization finishes."),
            "Gridcoin",
            CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
    } else {
        uiInterface.ThreadSafeMessageBox(
            _("ERROR: Checkpoint mismatch: Blockchain data may be corrupted.\n\n"
              "Gridcoin detected bad index entries. This may occur because of a "
              "late software upgrade, unexpected exit, or a power failure. "
              "Please run gridcoinresearchd with the -resetblockchaindata "
              "parameter. Your wallet will re-download the blockchain. Your "
              "balance may appear incorrect until the synchronization finishes." ),
            "Gridcoin",
            CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
    }
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
    // This loop initializes the registry for each contract type in CONTRACT_TYPES_WITH_REG_DB.
    for (const auto& contract_type : RegistryBookmarks::CONTRACT_TYPES_WITH_REG_DB) {
        Registry& registry = RegistryBookmarks::GetRegistryWithDB(contract_type);

        std::string contract_type_string = Contract::Type::ToString(contract_type);
        std::string tr_contract_type_string = Contract::Type::ToTranslatedString(contract_type);

        LogPrintf("INFO: %s: Loading stored history for contract type %s...",
                  __func__,
                  contract_type_string);

        uiInterface.InitMessage(_("Loading history for contract type ") + tr_contract_type_string + "...");

        std::string history_arg = "-clear" + GRC::Contract::Type::ToString(contract_type) + "history";
        if (gArgs.GetBoolArg(history_arg, false)) {
            registry.Reset();
        }

        LogPrintf("INFO: %s: Initializing registry from stored history for contract type %s...",
                  __func__,
                  contract_type_string);

        uiInterface.InitMessage(_("Initializing registry from stored history for contract type ")
                                  + tr_contract_type_string + "...");

        int db_height = registry.Initialize();

        if (db_height > 0) {
            LogPrintf("INFO: %s: History loaded through height %i for contract type %s",
                      __func__,
                      db_height,
                      contract_type_string);
        } else {
            LogPrintf("INFO: %s: History load not successful for contract type %s. Will initialize "
                      "from contract replay.",
                      __func__,
                      contract_type_string);
        }
    }

    LogPrintf("Gridcoin: replaying contracts...");
    uiInterface.InitMessage(_("Replaying contracts..."));

    CBlockIndex* pindex_start = GRC::BlockFinder::FindByMinTime(pindexBest->nTime
                                                                - Params().GetConsensus().StandardContractReplayLookback);

    const int& V11_height = Params().GetConsensus().BlockV11Height;
    const int& lookback_window_low_height = pindex_start->nHeight;

    // Gets a registry db height bookmark object with the heights loaded now that the registries are loaded.
    RegistryBookmarks db_heights;

    // This tricky clamp ensures the correct start height for the contract replay. Note that the current
    // implementation will skip beacon, scraper entry, project, poll and vote contracts that overlap the already loaded
    // history. See ReplayContracts. The worst case replay is a window that starts at V11_height and extends to current
    // height. This is the replay that will be encountered when starting a wallet that was in sync with this code but has
    // uninitialized registry dbs, and the head of the chain is more than MAX AGE above the V11_height, because
    // GetLowestRegistryBlockHeight() is 0, and then the maximum of V11_height and GetLowestRegistryBlockHeight() will be
    // V11_height and the minimum of V11_height and lookback_window_low_height will be V11_height. When the contracts are
    // replayed, the beacon db and scraper entry db will then be initialized and the controlling window will be either the
    // GetLowestRegistryBlockHeight() or the lookback_window_low_height, whichever is higher. The MAX_AGE (i.e.
    // lookback_window_low_height) condition once the contract types that have a backing db are initialized is now driven
    // by the following remaining contract types which have no registry (backing) db:
    //
    // CONTRACT type                      Wallet startup replay requirement           Block reorg replay requirement
    // POLL/VOTE (polls and voting)               V11 or std lookback                             false
    //
    // Note that the handler reset and contract replay forwards from lookback_window_low_height no longer is required
    // for polls and votes. The reason for this is quite simple. Polls and votes are UNIQUE. The reversion of an add
    // is simply to delete them. The wallet startup replay requirement is still required for polls and votes, because
    // the Poll/Vote classes do not have a backing registry db yet.
    int start_height = std::min(std::max(db_heights.GetLowestRegistryBlockHeight(), V11_height),
                                lookback_window_low_height);

    LogPrintf("Gridcoin: Starting contract replay from height %i.", start_height);

    CBlockIndex* pblock_index = pindexBest;

    while (pblock_index->nHeight > start_height)
    {
        pblock_index = pblock_index->pprev;
    }

    // Reset pindex_start to the index for the block at start_height
    pindex_start = pblock_index;

    // The replay contract window here may overlap with the registry db coverage for the various registries. Logic
    // is now included in the ApplyContracts to ignore contracts that have already been covered by the registry dbs.
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
//! checksums, we reinstate that assertion here as a formal inspection done
//! at initialization before the VerifyCheckpoints.
//!
//! This function checks that no block index entries contain a null pointer
//! to a previous block. The symptom may indicate a deeper problem that can
//! be resolved by tuning disk synchronization in LevelDB. Until then, this
//! heuristic has proven itself to be effective for identifying a corrupted
//! database. This type of error has not been seen in the wild in several
//! years as of Gridcoin 5.4.7.0, but is retained for thoroughness.
//!
bool CheckBlockIndex()
{
    LogPrintf("Gridcoin: checking block index...");
    uiInterface.InitMessage(_("Checking block index..."));

    // Block index integrity status
    bool status = true;

    if (pindexGenesisBlock) {
        LOCK(cs_main);

        for (const auto& index_pair : mapBlockIndex) {
            const CBlockIndex* const pindex = index_pair.second;

            if (!pindex || !(pindex->pprev || pindex == pindexGenesisBlock)) {
                status = false;
                break;
            }
        }
    }

    if (status) {
        LogPrintf("Gridcoin: block index is clean");
        return status;
    }

    ShowChainCorruptedMessage();

    return status;
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

void ScheduleRegistriesPassivation(CScheduler& scheduler)
{
    // Run registry database passivation every 5 minutes. This is a very thin call most of the time.
    // Please see the PassivateDB function and passivate_db.

    scheduler.scheduleEvery(RunDBPassivation, std::chrono::minutes{5});
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

    if (!CheckBlockIndex()) {
        return false;
    }
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
    ScheduleRegistriesPassivation(scheduler);
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

void GRC::RunDBPassivation()
{
    LOCK(cs_main);

    for (const auto& contract_type : RegistryBookmarks::CONTRACT_TYPES_WITH_REG_DB) {
        Registry& registry = RegistryBookmarks::GetRegistryWithDB(contract_type);

        registry.PassivateDB();
    }
}
