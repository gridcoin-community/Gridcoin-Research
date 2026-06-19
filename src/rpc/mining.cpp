// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "init.h"
#include <key_io.h>
#include "main.h"
#include "miner.h"
#include "gridcoin/accrual/snapshot.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "gridcoin/superblock.h"
#include "gridcoin/tally.h"
#include "gridcoin/voting/fwd.h"
#include "protocol.h"
#include "server.h"
#include <rpc/util.h>

#include <map>
#include <stdexcept>

using namespace std;

static const RPCHelpMan getstakinginfo_help{
    "getstakinginfo",
    "Returns an object containing staking-related information.\n"
    "Note: `getmininginfo` is a dispatch-table alias for this command.",
    {},
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "blocks", "Current best block height."},
            {RPCResult::Type::OBJ, "stakeweight", "",
                {{RPCResult::Type::ELISION, "", "Stake-weight detail (minimum/maximum/combined/valuesum/legacy)."}}},
            {RPCResult::Type::NUM, "netstakeweight", "Estimated network stake weight."},
            {RPCResult::Type::NUM, "netstakingGRCvalue", "Network stake value in GRC."},
            {RPCResult::Type::BOOL, "staking", "Whether the miner is actively staking."},
            {RPCResult::Type::STR, "mining-error", "Aggregated miner error string."},
            {RPCResult::Type::NUM, "time-to-stake_days", "Estimated time-to-stake in days."},
            {RPCResult::Type::NUM, "expectedtime", "Estimated time-to-stake in seconds."},
            {RPCResult::Type::NUM, "mining-version", "Block version most recently attempted."},
            {RPCResult::Type::NUM, "mining-created", "Number of blocks created in this run."},
            {RPCResult::Type::NUM, "mining-accepted", "Number of blocks accepted by the network."},
            {RPCResult::Type::NUM, "mining-kernels-found", "Total kernels found."},
            {RPCResult::Type::NUM, "masked_time_intervals_covered", "Mask intervals covered."},
            {RPCResult::Type::NUM, "masked_time_intervals_elapsed", "Mask intervals elapsed."},
            {RPCResult::Type::NUM, "staking_loop_efficiency", "Fraction of time spent in productive staking loops."},
            {RPCResult::Type::NUM, "actual_cumulative_weight", "Cumulative effective weight observed."},
            {RPCResult::Type::NUM, "ideal_cumulative_weight", "Cumulative ideal weight."},
            {RPCResult::Type::NUM, "staking_efficiency", "Overall staking efficiency."},
            {RPCResult::Type::OBJ, "stake-splitting", "",
                {{RPCResult::Type::ELISION, "", "Stake-splitting enabled flag and (when enabled) parameters."}}},
            {RPCResult::Type::OBJ, "side_staking", "",
                {{RPCResult::Type::ELISION, "", "Local side-staking enabled flag and active side-stake allocations."}}},
            {RPCResult::Type::OBJ, "difficulty", "",
                {
                    {RPCResult::Type::NUM, "current", "Current difficulty."},
                    {RPCResult::Type::NUM, "target", "Target difficulty."},
                    {RPCResult::Type::NUM, "last-search-interval", "Timestamp of last search."},
                }},
            {RPCResult::Type::STR, "errors", "Any warnings or errors."},
            {RPCResult::Type::NUM, "pooledtx", "Number of pooled transactions."},
            {RPCResult::Type::BOOL, "testnet", "Whether this node is on testnet."},
            {RPCResult::Type::STR, "CPID", "Researcher CPID."},
            {RPCResult::Type::NUM, "current_magnitude", /*optional=*/true,
                "Current magnitude for the active CPID (omitted if no CPID is configured)."},
            {RPCResult::Type::NUM, "Magnitude Unit", /*optional=*/true,
                "Magnitude unit (omitted if no CPID is configured)."},
            {RPCResult::Type::STR_AMOUNT, "BoincRewardPending", /*optional=*/true,
                "Pending research subsidy (omitted if no CPID is configured)."},
            {RPCResult::Type::STR, "researcher_status", "Aggregated researcher status string."},
            {RPCResult::Type::STR, "current_poll", "Title of the current active poll, if any."},
        }},
    RPCExamples{
        HelpExampleCli("getstakinginfo", "") +
        HelpExampleRpc("getstakinginfo", "")},
};
const RPCHelpMan& getstakinginfo_helpman() { return getstakinginfo_help; }

UniValue getstakinginfo(const UniValue& params)
{
    UniValue obj(UniValue::VOBJ);
    UniValue diff(UniValue::VOBJ);
    UniValue weight(UniValue::VOBJ);
    UniValue stakesplitting(UniValue::VOBJ);
    UniValue stakesplittingparam(UniValue::VOBJ);
    UniValue sidestaking(UniValue::VOBJ);
    UniValue sidestakingalloc(UniValue::VOBJ);
    UniValue vsidestakingalloc(UniValue::VARR);

    int64_t nTime = GetAdjustedTime();
    uint64_t nWeight = 0;
    double nNetworkWeight = 0;
    double nCurrentDiff = 0;
    double nTargetDiff = 0;
    uint64_t nExpectedTime = 0;
    int best_height = 0;
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        nWeight = GRC::GetStakeWeight(*pwalletMain);
        nNetworkWeight = GRC::GetEstimatedNetworkWeight();
        nCurrentDiff = GRC::GetCurrentDifficulty();
        nTargetDiff = GRC::GetTargetDifficulty();
        nExpectedTime = GRC::GetEstimatedTimetoStake();
        best_height = nBestHeight;
    }

    obj.pushKV("blocks", best_height);
    diff.pushKV("current", nCurrentDiff);
    diff.pushKV("target", nTargetDiff);

    const MinerStatus::SearchReport search = g_miner_status.GetSearchReport();
    diff.pushKV("last-search-interval", search.m_timestamp);
    weight.pushKV("minimum", search.m_weight_min);
    weight.pushKV("maximum", search.m_weight_max);
    weight.pushKV("combined", search.m_weight_sum);
    weight.pushKV("valuesum", search.m_value_sum);
    weight.pushKV("legacy", nWeight / (double)COIN);
    obj.pushKV("stakeweight", weight);

    obj.pushKV("netstakeweight", nNetworkWeight);
    obj.pushKV("netstakingGRCvalue", nNetworkWeight / 80.0);
    obj.pushKV("staking", g_miner_status.StakingActive());
    obj.pushKV("mining-error", g_miner_status.FormatErrors());
    obj.pushKV("time-to-stake_days", nExpectedTime/86400.0);
    obj.pushKV("expectedtime", nExpectedTime);
    obj.pushKV("mining-version", search.m_block_version);
    obj.pushKV("mining-created", search.m_blocks_created);
    obj.pushKV("mining-accepted", search.m_blocks_accepted);
    obj.pushKV("mining-kernels-found", search.m_kernels_found);

    const MinerStatus::EfficiencyReport efficiency = g_miner_status.GetEfficiencyReport();
    obj.pushKV("masked_time_intervals_covered", efficiency.masked_time_intervals_covered);
    obj.pushKV("masked_time_intervals_elapsed", efficiency.masked_time_intervals_elapsed);
    obj.pushKV("staking_loop_efficiency", efficiency.StakingLoopEfficiency());
    obj.pushKV("actual_cumulative_weight", efficiency.actual_cumulative_weight);
    obj.pushKV("ideal_cumulative_weight", efficiency.ideal_cumulative_weight);
    obj.pushKV("staking_efficiency", efficiency.StakingEfficiency());

    int64_t nMinStakeSplitValue = 0;
    double dEfficiency = 0;
    int64_t nDesiredStakeSplitValue = 0;
    SideStakeAlloc vSideStakeAlloc;

    LOCK(cs_main);

    // nMinStakeSplitValue, dEfficiency, and nDesiredStakeSplitValue are out parameters.
    bool fEnableStakeSplit = GetStakeSplitStatusAndParams(nMinStakeSplitValue, dEfficiency, nDesiredStakeSplitValue);

    bool fEnableSideStaking = gArgs.GetBoolArg("-enablesidestaking");

    stakesplitting.pushKV("stake-splitting-enabled", fEnableStakeSplit);
    if (fEnableStakeSplit)
    {
        stakesplittingparam.pushKV("min-stake-split-value", nMinStakeSplitValue / COIN);
        stakesplittingparam.pushKV("efficiency", dEfficiency);
        stakesplittingparam.pushKV("stake-split-UTXO-size-for-target-efficiency", nDesiredStakeSplitValue / COIN);
        stakesplitting.pushKV("stake-splitting-params", stakesplittingparam);
    }
    obj.pushKV("stake-splitting", stakesplitting);

    // This is what the miner sees...
    vSideStakeAlloc = GRC::GetSideStakeRegistry().ActiveSideStakeEntries(GRC::SideStake::FilterFlag::ALL, false);

    sidestaking.pushKV("local_side_staking_enabled", fEnableSideStaking);

    // Note that if local_side_staking_enabled is true, then local sidestakes will be applicable and shown. Mandatory
    // sidestakes are always included.
    for (const auto& alloc : vSideStakeAlloc)
    {
        sidestakingalloc.pushKV("address", EncodeDestination(alloc->GetDestination()));
        sidestakingalloc.pushKV("allocation_pct", alloc->GetAllocation().ToPercent());
        sidestakingalloc.pushKV("status", alloc->StatusToString());

        vsidestakingalloc.push_back(sidestakingalloc);
    }
    sidestaking.pushKV("side_staking_allocations", vsidestakingalloc);
    obj.pushKV("side_staking", sidestaking);

    obj.pushKV("difficulty",    diff);
    obj.pushKV("errors",        GetWarnings("statusbar"));
    obj.pushKV("pooledtx",      (uint64_t)mempool.size());

    obj.pushKV("testnet",       fTestNet);

    const GRC::MiningId mining_id = GRC::Researcher::Get()->Id();
    obj.pushKV("CPID", mining_id.ToString());

    if (const GRC::CpidOption cpid = mining_id.TryCpid())
    {
        const GRC::AccrualComputer calc = GRC::Tally::GetComputer(*cpid, nTime, pindexBest);

        GRC::Magnitude magnitude = GRC::Quorum::GetMagnitude(mining_id);

        obj.pushKV("current_magnitude", magnitude.Floating());
        obj.pushKV("Magnitude Unit", calc->MagnitudeUnit());
        obj.pushKV("BoincRewardPending", ValueFromAmount(calc->Accrual()));
    }

    std::string current_poll;

    std::string researcher_status;
    {
        LOCK(cs_msMiningErrors);
        researcher_status = msMiningErrors;
    }

    obj.pushKV("researcher_status", researcher_status);
    obj.pushKV("current_poll", GRC::GetCurrentPollTitle());

    return obj;
}

static const RPCHelpMan getlaststake_help{
    "getlaststake",
    "Fetch information about this wallet's last staked block.",
    {
        {"ignored", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
            "Accepted for compatibility with the legacy 0-1 arg surface; value is unused."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR_HEX, "block", "Hash of the block in which the stake was mined."},
            {RPCResult::Type::NUM, "height", "Block height."},
            {RPCResult::Type::NUM, "confirmations", "Confirmations since the stake block."},
            {RPCResult::Type::BOOL, "immature", "Whether the stake is still immature."},
            {RPCResult::Type::STR_HEX, "txid", "Stake transaction id."},
            {RPCResult::Type::NUM_TIME, "time", "Block timestamp."},
            {RPCResult::Type::NUM, "elapsed_seconds", "Seconds elapsed since the stake."},
            {RPCResult::Type::NUM, "elapsed_days", "Days elapsed since the stake."},
            {RPCResult::Type::STR_AMOUNT, "mint", "Amount minted to the wallet."},
            {RPCResult::Type::STR_AMOUNT, "research_reward", "Research subsidy paid."},
            {RPCResult::Type::STR_AMOUNT, "side_stake", "Side-stake amount paid out."},
            {RPCResult::Type::STR, "address", "Stake output destination address (empty if undecodable)."},
            {RPCResult::Type::STR, "label", "Source-account label of the stake transaction."},
        }},
    RPCExamples{
        HelpExampleCli("getlaststake", "") +
        HelpExampleRpc("getlaststake", "")},
};
const RPCHelpMan& getlaststake_helpman() { return getlaststake_help; }

UniValue getlaststake(const UniValue& params)
{
    const std::optional<CWalletTx> stake_tx = g_miner_status.GetLastStake(*pwalletMain);

    if (!stake_tx) {
        throw JSONRPCError(RPC_WALLET_ERROR, "No prior staked blocks found.");
    }

    const auto* stake_conf = stake_tx->state<TxStateConfirmed>();
    if (!stake_conf) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Last stake transaction is not in confirmed state.");
    }

    int64_t height;
    int64_t timestamp;
    int64_t confirmations;

    int64_t mint_amount = 0;
    int64_t side_stake_amount = 0;
    int64_t research_reward_amount;

    {
        LOCK(cs_main);

        const CBlockIndex* const pindex = mapBlockIndex[stake_conf->m_confirmed_block_hash];

        height = pindex->nHeight;
        timestamp = pindex->nTime;
        research_reward_amount = pindex->ResearchSubsidy();
        confirmations = stake_tx->GetDepthInMainChain();
    }

    for (const auto& txo : stake_tx->vout) {
        if (pwalletMain->IsMine(txo)) {
            mint_amount += txo.nValue;
        } else {
            side_stake_amount += txo.nValue;
        }
    }

    const int64_t elapsed_seconds = GetAdjustedTime() - timestamp;
    UniValue json(UniValue::VOBJ);

    json.pushKV("block", stake_conf->m_confirmed_block_hash.ToString());
    json.pushKV("height", height);
    json.pushKV("confirmations", confirmations);
    json.pushKV("immature", confirmations < nCoinbaseMaturity);
    json.pushKV("txid", stake_tx->GetHash().ToString());
    json.pushKV("time", timestamp);
    json.pushKV("elapsed_seconds", elapsed_seconds);
    json.pushKV("elapsed_days", elapsed_seconds / 86400.0);
    json.pushKV("mint", ValueFromAmount(mint_amount - stake_tx->GetDebit()));
    json.pushKV("research_reward", ValueFromAmount(research_reward_amount));
    json.pushKV("side_stake", ValueFromAmount(side_stake_amount));

    CTxDestination dest;

    if (ExtractDestination(stake_tx->vout[1].scriptPubKey, dest)) {
        json.pushKV("address", EncodeDestination(dest));
    } else {
        json.pushKV("address", "");
    }

    json.pushKV("label", stake_tx->strFromAccount);

    return json;
}

extern double CoinToDouble(double surrogate);

static const RPCHelpMan auditsnapshotaccrual_help{
    "auditsnapshotaccrual",
    "Report accrual snapshot deltas for the specified CPID.",
    {
        {"cpid", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
            "External CPID to audit. Defaults to this researcher's CPID."},
        {"report_details", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
            "If true, include per-snapshot detail in the report. Default: false."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {{RPCResult::Type::ELISION, "", "Snapshot accrual audit object; see source for shape."}}},
    RPCExamples{
        HelpExampleCli("auditsnapshotaccrual", "") +
        HelpExampleCli("auditsnapshotaccrual", "\"<cpid>\" true") +
        HelpExampleRpc("auditsnapshotaccrual", "\"<cpid>\", true")},
};
const RPCHelpMan& auditsnapshotaccrual_helpman() { return auditsnapshotaccrual_help; }

namespace {
//!
//! \brief One chain block captured during an accrual audit walk.
//!
//! Captured under cs_main; consumed lock-free. CBlockIndex* pointers are
//! pool-allocated and never freed, so they remain valid after the lock is
//! released and across reorgs. Only pnext is mutated under cs_main, and it is
//! followed only during the (locked) capture phase.
//!
struct AuditBlockRecord
{
    //!
    //! \brief How the block contributes to the audit.
    //!
    //! Mirrors the original else-if short-circuit: a non-empty m_mrc_researchers
    //! consumes the block whether or not it contains an entry for the audited
    //! CPID, so a block carrying only other CPIDs' MRCs (MRC_NO_MATCH) must NOT
    //! fall through to the superblock branch. The superblock flip that selects the
    //! governing magnitude is tracked independently of this classification.
    //!
    enum class Kind { NONE, STAKE, MRC_PAYMENT, MRC_NO_MATCH, SUPERBLOCK };

    const CBlockIndex* pindex = nullptr;
    int64_t time = 0;       //!< pindex->nTime
    int64_t prev_time = 0;  //!< pindex->pprev->nTime (mrc period high_time)
    uint32_t height = 0;
    bool is_superblock = false;
    Kind kind = Kind::NONE;
    int64_t claimed = 0;    //!< ResearchSubsidy (stake) or mrc->m_research_subsidy
    const CBlockIndex* governing_superblock = nullptr; //!< superblock for MagnitudeOf
};

//!
//! \brief A captured, lock-free-replayable plan for one audit pass.
//!
struct AuditPassPlan
{
    const CBlockIndex* pindex_start_superblock = nullptr; //!< first in-scope superblock
    int64_t seeded_accrual = 0;                           //!< accrual from the snapshot file
    bool have_result = false;                             //!< false => return empty for this CPID
    const CBlockIndex* tip_governing_superblock = nullptr;//!< governing superblock at the tip
    std::vector<AuditBlockRecord> records;
};

//!
//! \brief Walk the chain for one audit pass and capture everything the lock-free
//! compute phase needs.
//!
//! Performs no superblock block-file reads (those happen lock-free in the compute
//! phase against immutable, append-only block files). The single small accrual
//! snapshot .dat file IS read here, under the lock, because it is deleted or
//! truncated under cs_main during reorgs.
//!
//! \param retry_from_baseline Start at the first superblock after the Fern
//! baseline rather than the first within the beacon chain's scope.
//!
AuditPassPlan CaptureAuditPass(
    const bool retry_from_baseline,
    const GRC::Cpid& cpid,
    const GRC::Beacon_ptr& beacon_ptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AuditPassPlan plan;

    const CBlockIndex* pindex_baseline = GRC::Tally::GetBaseline();

    if (!pindex_baseline) {
        // No baseline yet; degrade to "no result" rather than dereferencing null.
        return plan;
    }

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: pindex_baseline->nHeight = %i", __func__, pindex_baseline->nHeight);

    const CBlockIndex* pindex_superblock = nullptr;

    // Find the first superblock after the baseline within scope of the beacon chain for the given CPID as the starting
    // point for the audit. If the second pass, where a difference was found because someone may have missed a renewal and
    // therefore have multiple beacon chains, then start from the first superblock after the baseline. This is much more
    // time-consuming, and so is only done if there is a difference found in the first pass. Even in the second pass, the starting
    // point of the first superblock after the transition height doesn't allow us to verify the accrual between the actual
    // transition height and the first snapshot afterwards, but it drastically reduces the complexity of the audit.
    for (const CBlockIndex* p = pindex_baseline; p; p = p->pnext) {
        if (p->IsSuperblock()
                && (retry_from_baseline || p->nTime >= beacon_ptr->m_timestamp)) {
            pindex_superblock = p;
            break;
        }
    }

    if (!pindex_superblock) {
        // No in-scope superblock found; degrade to "no result" rather than crashing.
        return plan;
    }

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: First in scope superblock nHeight = %i", __func__,
             pindex_superblock->nHeight);

    plan.pindex_start_superblock = pindex_superblock;

    // The governing superblock mirrors the original `superblock` variable: it is
    // pre-loaded to the starting superblock and flipped to each subsequent
    // superblock AFTER that block's accrual period would be recorded.
    const CBlockIndex* governing = pindex_superblock;

    for (const CBlockIndex* pindex = pindex_superblock; pindex; pindex = pindex->pnext) {
        AuditBlockRecord rec;
        rec.pindex = pindex;
        rec.height = pindex->nHeight;
        rec.time = pindex->nTime;
        rec.prev_time = pindex->pprev ? pindex->pprev->nTime : 0;
        rec.is_superblock = pindex->IsSuperblock();
        rec.governing_superblock = governing;

        if (pindex->ResearchSubsidy() > 0 && pindex->GetMiningId() == cpid) {
            rec.kind = AuditBlockRecord::Kind::STAKE;
            rec.claimed = pindex->ResearchSubsidy();
        } else if (!pindex->m_mrc_researchers.empty()) {
            // A non-empty MRC list consumes this block whether or not it pays the audited CPID. Because
            // m_mrc_researchers is derived from a map that is keyed by CPID, the CPID must be unique (i.e. there
            // will only be one match). A block paying only other CPIDs must NOT fall through to the superblock branch.
            rec.kind = AuditBlockRecord::Kind::MRC_NO_MATCH;

            for (const auto& mrc : pindex->m_mrc_researchers) {
                if (mrc->m_cpid == cpid) {
                    rec.kind = AuditBlockRecord::Kind::MRC_PAYMENT;
                    rec.claimed = mrc->m_research_subsidy;
                    break;
                }
            }
        } else if (pindex->IsSuperblock()) {
            rec.kind = AuditBlockRecord::Kind::SUPERBLOCK;
        }

        plan.records.push_back(rec);

        // The superblock flip is orthogonal to the classification above (the original updates it in a
        // separate if after the else-if chain), so a staked/MRC block that is also a superblock still flips it.
        if (pindex->IsSuperblock()) {
            governing = pindex;
        }
    }

    plan.tip_governing_superblock = governing;

    // The accrual snapshot .dat file is deleted/truncated under cs_main on reorg, so read it here under the
    // lock rather than in the lock-free compute phase. This is a single small file, not the per-block reads.
    const fs::path snapshot_path = SnapshotPath(pindex_superblock->nHeight);
    AccrualSnapshotReader reader(snapshot_path);

    if (reader.IsNull()) {
        return plan;
    }

    try {
        const AccrualSnapshot snapshot = reader.Read();
        plan.seeded_accrual = snapshot.GetAccrual(cpid);
        plan.have_result = true;
    } catch (const std::exception& e) {
        LogPrint(BCLog::LogFlags::ACCRUAL, "ERROR: %s: failed to read accrual snapshot at height %i: %s",
                 __func__, pindex_superblock->nHeight, e.what());
    }

    return plan;
}
} // anonymous namespace

UniValue auditsnapshotaccrual(const UniValue& params)
{
    const GRC::MiningId mining_id = params.size() > 0
        ? GRC::MiningId::Parse(params[0].get_str())
        : GRC::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    bool report_details = false;

    if (params.size() > 1) {
        report_details = params[1].get_bool();
    }

    const GRC::CpidOption cpid = mining_id.TryCpid();

    if (!cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for non-cruncher.");
    }

    UniValue result(UniValue::VOBJ);
    UniValue audit(UniValue::VARR);

    // --- Snapshot phase: capture a consistent slice of chain state under a brief
    // cs_main, then release the lock before performing per-superblock block-file
    // reads and accrual arithmetic. Holding cs_main across that I/O froze the node
    // for minutes, especially via the auditsnapshotaccruals (plural) caller. See
    // GH #2978.
    const int64_t now = GetAdjustedTime();
    bool accrual_account_exists = true;
    int64_t computed = 0;
    int64_t newbie_correction = 0;
    int64_t renewals = 0;
    double magnitude_unit = 0.0;
    CAmount max_reward = 0;
    UniValue beacon_chain(UniValue::VARR);
    GRC::Beacon_ptr beacon_ptr;
    AuditPassPlan plan;

    {
        LOCK(cs_main);

        if (!pindexBest) {
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Invalid chain.");
        }

        if (!IsV11Enabled(nBestHeight + 1)) {
            throw JSONRPCError(RPC_INVALID_REQUEST, "Wait for block v11 protocol");
        }

        const GRC::ResearchAccount& account = GRC::Tally::GetAccount(*cpid);
        computed = GRC::Tally::GetAccrual(*cpid, now, pindexBest);
        newbie_correction = Tally::GetNewbieSuperblockAccrualCorrection(*cpid, GRC::Quorum::CurrentSuperblock());

        //This indicates the account actually points to m_new_account.
        if (account.m_accrual == 0
                && account.m_total_research_subsidy == 0
                && account.m_total_magnitude== 0
                && account.m_accuracy == 0
                && account.m_first_block_ptr == nullptr
                && account.m_last_block_ptr == nullptr
                )
        {
            // The account effectively does not really exist.
            accrual_account_exists = false;
        }

        GRC::BeaconRegistry& beacons = GRC::GetBeaconRegistry();

        LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: Number of beacons in registry = %u", __func__, beacons.Beacons().size());

        GRC::BeaconOption beacon_try = beacons.Try(*cpid);

        if (!beacon_try)
        {
            LogPrint(BCLog::LogFlags::ACCRUAL, "ERROR: %s: No beacon present for cpid = %s.", __func__, cpid->ToString());
            return result;
        }

        beacon_ptr = beacon_try;

        auto beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>();

        try {
            beacon_ptr = beacons.GetBeaconChainletRoot(beacon_ptr, beacon_chain_out_ptr);
        } catch (std::runtime_error& e) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, e.what());
        }

        for (const auto& iter : *beacon_chain_out_ptr) {
            UniValue beacon_chain_entry(UniValue::VOBJ);

            beacon_chain_entry.pushKV("ctx_hash", iter.first.GetHex());
            beacon_chain_entry.pushKV("timestamp",  iter.second);
            beacon_chain.push_back(beacon_chain_entry);
        }

        renewals = beacon_chain_out_ptr->size() - 1;

        // MaxReward() and MagnitudeUnit() depend only on the current superblock, so they are constant for the
        // entire audit (across both passes, every block, and the tip). Resolve them once here rather than
        // reconstructing a snapshot computer per block under the lock.
        const AccrualComputer computer = GRC::Tally::GetSnapshotComputer(
            *cpid, pindexBest->GetBlockTime(), pindexBest);
        magnitude_unit = computer->MagnitudeUnit();
        max_reward = computer->MaxReward();

        plan = CaptureAuditPass(false, *cpid, beacon_ptr);
    } // cs_main released

    if (!plan.have_result) {
        return result;
    }

    // --- Compute phase: replay a captured pass lock-free. The per-superblock
    // block-file reads (for MagnitudeOf) and the accrual arithmetic happen here
    // with no lock held.
    struct ComputeOutput {
        int64_t accrual = 0;
        int64_t period = 0;
        UniValue result{UniValue::VOBJ};
    };

    const auto compute_pass = [&](const AuditPassPlan& pass) -> ComputeOutput {
        ComputeOutput out;

        // Read each governing superblock from disk at most once per pass. Block files are append-only and
        // immutable, so these reads are safe without cs_main.
        std::map<const CBlockIndex*, SuperblockPtr> sb_cache;

        const auto magnitude_of = [&](const CBlockIndex* sb_index) -> GRC::Magnitude {
            auto it = sb_cache.find(sb_index);

            if (it == sb_cache.end()) {
                it = sb_cache.emplace(sb_index, SuperblockPtr::ReadFromDisk(sb_index)).first;
            }

            return it->second->m_cpids.MagnitudeOf(*cpid);
        };

        int64_t accrual = pass.seeded_accrual;

        const auto tally_accrual_period = [&](
                const std::string& boundary,
                const uint64_t height,
                const int64_t low_time,
                const int64_t high_time,
                const int64_t claimed,
                const Allocation magnitude_unit,
                const CAmount max_reward,
                const CBlockIndex* governing)
        {
            const GRC::Magnitude magnitude = magnitude_of(governing);

            int64_t time_interval = high_time - low_time;
            int64_t abs_time_interval = time_interval;

            int sign = (time_interval >= 0) ? 1 : -1;

            if (sign < 0) {
                abs_time_interval = -time_interval;
            }

            // This is the same way that AccrualDelta calculates accruals in the snapshot calculator. Here
            // we use the absolute value of the time interval to ensure negative values are carried through
            // correctly in the bignumber calculations.
            const uint64_t base_accrual = abs_time_interval
                    * magnitude.Scaled()
                    * magnitude_unit.GetNumerator();

            int64_t period = 0;

            if (base_accrual > std::numeric_limits<uint64_t>::max() / COIN) {
                arith_uint256 accrual_bn(base_accrual);
                accrual_bn *= COIN;
                accrual_bn /= 86400;
                accrual_bn /= Magnitude::SCALE_FACTOR;
                accrual_bn /= magnitude_unit.GetDenominator();

                period = accrual_bn.GetLow64() * (int64_t) sign;
            }
            else
            {
                period = base_accrual * (int64_t) sign
                        * COIN
                        / 86400
                        / Magnitude::SCALE_FACTOR
                        / magnitude_unit.GetDenominator();
            }

            accrual += period;

            if (accrual > max_reward)
            {
                int64_t overage = accrual - max_reward;
                // Cap accrual at max_reward;
                accrual = max_reward;
                // Remove overage from period, because you can't have a period accrual to over the max.
                period -= overage;
            }

            if (report_details) {
                UniValue accrual_out(UniValue::VOBJ);
                accrual_out.pushKV("period", period);
                accrual_out.pushKV("accumulated", accrual);
                accrual_out.pushKV("claimed", claimed);

                UniValue delta(UniValue::VOBJ);
                delta.pushKV("boundary", boundary);
                delta.pushKV("low_time", low_time);
                delta.pushKV("high_height", height ? height : NullUniValue);
                delta.pushKV("high_time", high_time);
                delta.pushKV("magnitude_at_low", magnitude.Floating());
                delta.pushKV("accrual", accrual_out);

                audit.push_back(delta);
            }

            return period;
        };

        const CBlockIndex* pindex_low = pass.pindex_start_superblock;

        for (const auto& rec : pass.records) {
            switch (rec.kind) {
            case AuditBlockRecord::Kind::STAKE:
                tally_accrual_period(
                            "stake",
                            rec.height,
                            pindex_low->nTime,
                            rec.time,
                            rec.claimed,
                            magnitude_unit,
                            max_reward,
                            rec.governing_superblock);

                accrual = 0;
                pindex_low = rec.pindex;
                break;
            case AuditBlockRecord::Kind::MRC_PAYMENT:
                // mrc payments are on the block previous to the staked block (the head of the chain when the
                // mrc was submitted).
                tally_accrual_period(
                            "mrc payment",
                            rec.height,
                            pindex_low->nTime,
                            rec.prev_time,
                            rec.claimed,
                            magnitude_unit,
                            max_reward,
                            rec.governing_superblock);

                accrual = 0;
                pindex_low = rec.pindex->pprev;
                break;
            case AuditBlockRecord::Kind::SUPERBLOCK:
                tally_accrual_period(
                            "superblock",
                            rec.height,
                            pindex_low->nTime,
                            rec.time,
                            0,
                            magnitude_unit,
                            max_reward,
                            rec.governing_superblock);

                pindex_low = rec.pindex;
                break;
            case AuditBlockRecord::Kind::MRC_NO_MATCH:
            case AuditBlockRecord::Kind::NONE:
                // No accrual period: no reset, no pindex_low advance (the governing superblock flip already
                // happened during capture).
                break;
            }
        }

        // The final period is from the last event till "now".
        int64_t period = tally_accrual_period(
            "tip", 0, pindex_low->nTime, now, 0, magnitude_unit, max_reward, pass.tip_governing_superblock);

        out.result.pushKV("cpid", cpid->ToString());
        out.result.pushKV("accrual_account_exists", accrual_account_exists);
        out.result.pushKV("latest_beacon_timestamp", beacon_chain[0]);
        out.result.pushKV("original_beacon_timestamp", beacon_chain[beacon_chain.size() - 1]);
        out.result.pushKV("renewals", renewals);
        out.result.pushKV("accrual_by_audit", accrual);
        out.result.pushKV("accrual_by_GetAccrual", computed);
        out.result.pushKV("newbie_correction", newbie_correction);
        out.result.pushKV("accrual_last_period", period);

        if (report_details) {
            out.result.pushKV("beacon_chain", beacon_chain);
            out.result.pushKV("audit", audit);
        }

        out.accrual = accrual;
        out.period = period;

        return out;
    };

    const ComputeOutput first_pass = compute_pass(plan);

    // The second part of this if statement condition is to deal with the 1 Halford difference that crops up between
    // this audit calculation and the newbie_correction. The two calculations are very similar, but the newbie correction
    // goes backwards in the chain, and this one goes forward. Somewhere there is a 1 Halford difference. Not worth tracking
    // down, and the consensus critical newbie correction algorithm gives consistent values across all nodes.
    if (first_pass.accrual == computed
            || first_pass.accrual - (newbie_correction + first_pass.period) <= 1) {
        return first_pass.result;
    }

    LogPrintf("WARNING: %s: Doing second pass on auditsnapshotaccrual loop because of mismatch after the first pass. "
              "This can be expected if someone let their beacon expire and there are multiple beacon chains with the "
              "same CPID since the Fern baseline.", __func__);

    // Second pass from the Fern baseline. Re-acquire cs_main only for the (rare) re-capture; the previously
    // captured pointers remain valid because the block index pool never frees entries. The accumulated `audit`
    // array is intentionally NOT cleared, so the report concatenates both passes exactly as before.
    AuditPassPlan baseline_plan;
    {
        LOCK(cs_main);
        baseline_plan = CaptureAuditPass(true, *cpid, beacon_ptr);
    }

    if (!baseline_plan.have_result) {
        return result;
    }

    return compute_pass(baseline_plan).result;
}

static const RPCHelpMan auditsnapshotaccruals_help{
    "auditsnapshotaccruals",
    "Report accrual audit for entire population of CPIDs.",
    {
        {"report_only_mismatches", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
            "If true, omit matching CPIDs from the report. Default: false."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "number_of_CPIDs", "Total CPIDs audited."},
            {RPCResult::Type::NUM, "number_of_matches", "Matching audit results."},
            {RPCResult::Type::NUM, "number_of_mismatches", "Mismatching audit results."},
            {RPCResult::Type::NUM, "number_of_mismatches_last_period_only", "Mismatches only in the last accrual period."},
            {RPCResult::Type::NUM, "number_accrual_accounts_not_present", "CPIDs without an accrual account."},
            {RPCResult::Type::NUM, "number_not_present", "CPIDs the audit could not produce a result for."},
            {RPCResult::Type::ARR, "accrual_mismatch_details", "",
                {{RPCResult::Type::ELISION, "", "Per-CPID match/mismatch detail object."}}},
        }},
    RPCExamples{
        HelpExampleCli("auditsnapshotaccruals", "") +
        HelpExampleCli("auditsnapshotaccruals", "true") +
        HelpExampleRpc("auditsnapshotaccruals", "true")},
};
const RPCHelpMan& auditsnapshotaccruals_helpman() { return auditsnapshotaccruals_help; }

UniValue auditsnapshotaccruals(const UniValue& params)
{
    bool report_only_mismatches = false;

    if (params.size() > 0)
    {
        report_only_mismatches = params[0].get_bool();
    }

    UniValue result(UniValue::VOBJ);

    // Hold cs_main only long enough to copy the SuperblockPtr (a refcounted
    // handle to immutable data). The per-CPID iteration below calls
    // auditsnapshotaccrual, which takes its own LOCK(cs_main) for each call.
    // Holding cs_main across the whole loop would freeze the node for minutes
    // on a full network (one full audit can take that long).
    SuperblockPtr superblock;
    {
        LOCK(cs_main);
        superblock = GRC::Quorum::CurrentSuperblock();
    }

    UniValue entries(UniValue::VARR);
    int number_of_cpids = 0;
    int number_of_matches = 0;
    int number_of_mismatches = 0;
    int number_of_mismatches_last_period_only = 0;
    int number_accrual_accounts_not_present = 0;
    int number_not_present = 0;

    for (const auto& iter : superblock->m_cpids) {
        std::vector<UniValue> v_params {iter.Cpid().ToString(), false};

        UniValue internal_params(UniValue::VARR);

        internal_params.push_backV(v_params);

        UniValue match_status(UniValue::VOBJ);

        UniValue audit(auditsnapshotaccrual(internal_params));

        if (!audit.empty()) {
            const CAmount& accrual_by_audit = find_value(audit, "accrual_by_audit").get_int64();
            const CAmount& accrual_by_GetAccrual = find_value(audit, "accrual_by_GetAccrual").get_int64();
            const CAmount& newbie_correction = find_value(audit, "newbie_correction").get_int64();
            const CAmount& accrual_last_period = find_value(audit, "accrual_last_period").get_int64();
            const bool accrual_account_exists = find_value(audit, "accrual_account_exists").get_bool();

            // The second part of this if statement condition is to deal with the 1 Halford difference that crops up between
            // this audit calculation and the newbie_correction. The two calculations are very similar, but the newbie correction
            // goes backwards in the chain, and this one goes forward. Somewhere there is a 1 Halford difference. Not worth tracking
            // down, and the consensus critical newbie correction algorithm gives consistent values across all nodes.
            if (accrual_by_audit == accrual_by_GetAccrual
                    || accrual_by_audit - (newbie_correction + accrual_last_period) <= 1) {
                if (!report_only_mismatches)
                {
                    match_status.pushKV("CPID", iter.Cpid().ToString());
                    match_status.pushKV("match", audit);
                    entries.push_back(match_status);
                }
                ++number_of_matches;
            }
            else {
                match_status.pushKV("CPID", iter.Cpid().ToString());

                if (accrual_last_period == accrual_by_GetAccrual)
                {
                    match_status.pushKV("mismatch_accrual_last_period_only", audit);
                    ++number_of_mismatches_last_period_only;
                }
                else
                {
                    match_status.pushKV("mismatch_other", audit);
                }
                entries.push_back(match_status);
                ++number_of_mismatches;
            }

            if (!accrual_account_exists) ++number_accrual_accounts_not_present;
        }
        else {
            ++number_not_present;
        }

        ++number_of_cpids;
    }

    result.pushKV("number_of_CPIDs", number_of_cpids);
    result.pushKV("number_of_matches", number_of_matches);
    result.pushKV("number_of_mismatches", number_of_mismatches);
    result.pushKV("number_of_mismatches_last_period_only", number_of_mismatches_last_period_only);
    result.pushKV("number_accrual_accounts_not_present", number_accrual_accounts_not_present);
    result.pushKV("number_not_present", number_not_present);

    result.pushKV("accrual_mismatch_details", entries);

    return result;
}

static const RPCHelpMan listresearcheraccounts_help{
    "listresearcheraccounts",
    "List researcher accounts in the accrual system and their current accruals.",
    {},
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "number_of_accounts", "Total researcher accounts tracked."},
            {RPCResult::Type::ARR, "details", "",
                {
                    {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR, "cpid", "Researcher CPID."},
                            {RPCResult::Type::NUM, "accrual_as_of_last_superblock", "Accrual recorded at last superblock."},
                            {RPCResult::Type::NUM, "current_accrual", "Accrual as of the chain tip."},
                        }},
                }},
        }},
    RPCExamples{
        HelpExampleCli("listresearcheraccounts", "") +
        HelpExampleRpc("listresearcheraccounts", "")},
};
const RPCHelpMan& listresearcheraccounts_helpman() { return listresearcheraccounts_help; }

UniValue listresearcheraccounts(const UniValue& params)
{
    UniValue result(UniValue::VOBJ);
    UniValue entries(UniValue::VARR);

    const int64_t now = GetAdjustedTime();

    LOCK(cs_main);

    for (const auto& iter : GRC::Tally::Accounts())
    {
        UniValue entry(UniValue::VOBJ);

        const GRC::Cpid& cpid = iter.first;
        const GRC::ResearchAccount& account = iter.second;
        const int64_t accrual = GRC::Tally::GetAccrual(cpid, now, pindexBest);

        entry.pushKV("cpid", cpid.ToString());
        entry.pushKV("accrual_as_of_last_superblock", account.m_accrual);
        entry.pushKV("current_accrual", accrual);

        entries.push_back(entry);
    }

    result.pushKV("number_of_accounts", (int) GRC::Tally::Accounts().size());
    result.pushKV("details", entries);

    return result;
}

static const RPCHelpMan inspectaccrualsnapshot_help{
    "inspectaccrualsnapshot",
    "Display the contents of an accrual snapshot from the accrual repository on disk.",
    {
        {"height", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Block height (and file name) of the snapshot."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "version", "Snapshot version."},
            {RPCResult::Type::NUM, "height", "Snapshot block height."},
            {RPCResult::Type::OBJ_DYN, "records", "Mapping of CPID to accrual amount",
                {{RPCResult::Type::NUM, "cpid", "Accrual value at the snapshot for this CPID."}}},
        }},
    RPCExamples{
        HelpExampleCli("inspectaccrualsnapshot", "5000000") +
        HelpExampleRpc("inspectaccrualsnapshot", "5000000")},
};
const RPCHelpMan& inspectaccrualsnapshot_helpman() { return inspectaccrualsnapshot_help; }

UniValue inspectaccrualsnapshot(const UniValue& params)
{
    const fs::path snapshot_path = SnapshotPath(params[0].get_int());
    const AccrualSnapshot snapshot = AccrualSnapshotReader(snapshot_path).Read();

    UniValue result(UniValue::VOBJ);

    result.pushKV("version", (uint64_t)snapshot.m_version);
    result.pushKV("height", snapshot.m_height);

    const AccrualSnapshot::AccrualMap& records = snapshot.m_records;
    const std::map<Cpid, int64_t> sorted_records(records.begin(), records.end());

    UniValue records_out(UniValue::VOBJ);

    for (const auto& record_pair : sorted_records) {
        const Cpid& cpid = record_pair.first;
        const int64_t accrual = record_pair.second;

        records_out.pushKV(cpid.ToString(), ValueFromAmount(accrual));
    }

    result.pushKV("records", records_out);

    return result;
}

// Regtest-only block-production RPCs. Refuse on mainnet / testnet so the
// names cannot be invoked accidentally in non-test contexts.

namespace {
UniValue MineNBlocks(int nblocks, const std::string& dest_address /*advisory*/)
{
    if (!Params().IsMockableChain()) {
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "generate/generatetoaddress only available on -regtest");
    }
    if (nblocks < 1 || nblocks > 1000) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nblocks out of range (1..1000)");
    }
    if (!pwalletMain) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is not loaded");
    }

    // Best-effort decode for address-shape validation; we don't currently
    // route the reward to it (PoS coinstake destination is wallet-controlled),
    // but malformed input should surface as an error before we start mining.
    if (!dest_address.empty()) {
        CTxDestination dest = DecodeDestination(dest_address);
        if (!IsValidDestination(dest)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
        }
    }

    UniValue hashes(UniValue::VARR);
    for (int i = 0; i < nblocks; ++i) {
        CBlock block;
        std::string err;
        // Retry transient CreateCoinStake failures (kernel hash too high, etc.)
        // a few times before giving up. Under trivial powLimit + nStakeMinAge=0
        // the kernel passes on first try, but UTXO selection can race with
        // mempool / wallet flushes briefly.
        bool ok = false;
        for (int attempt = 0; attempt < 5 && !ok; ++attempt) {
            err.clear();
            ok = TryMineRegtestBlock(pwalletMain, block, err);
            if (!ok) {
                LogPrintf("generatetoaddress: attempt %d failed: %s", attempt, err);
            }
        }
        if (!ok) {
            throw JSONRPCError(RPC_INTERNAL_ERROR,
                strprintf("Failed to mine block %d of %d: %s", i + 1, nblocks, err));
        }
        hashes.push_back(block.GetHash(true).ToString());
    }
    return hashes;
}
} // namespace

static const RPCHelpMan generatetoaddress_help{
    "generatetoaddress",
    "Mine blocks immediately (regtest only).\n"
    "The address is currently only shape-checked; the PoS coinstake reward\n"
    "goes to wallet-controlled keys (Gridcoin has no direct PoW-style\n"
    "coinbase payout).",
    {
        {"nblocks", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Number of blocks to generate (1..1000)."},
        {"address", RPCArg::Type::STR, RPCArg::Optional::NO,
            "Address to shape-check (advisory; reward routing is wallet-controlled)."},
    },
    RPCResult{RPCResult::Type::ARR, "", "Hashes of the blocks generated",
        {{RPCResult::Type::STR_HEX, "", "Block hash."}}},
    RPCExamples{
        HelpExampleCli("generatetoaddress", "11 \"myaddress\"") +
        HelpExampleRpc("generatetoaddress", "11, \"myaddress\"")},
};
const RPCHelpMan& generatetoaddress_helpman() { return generatetoaddress_help; }

UniValue generatetoaddress(const UniValue& params)
{
    const int nblocks = params[0].get_int();
    const std::string addr = params[1].get_str();
    return MineNBlocks(nblocks, addr);
}

static const RPCHelpMan generate_help{
    "generate",
    "Mine blocks immediately (regtest only).\n"
    "Convenience wrapper around generatetoaddress with the address check\n"
    "skipped; the PoS coinstake reward goes to wallet-controlled keys.",
    {
        {"nblocks", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Number of blocks to generate (1..1000)."},
    },
    RPCResult{RPCResult::Type::ARR, "", "Hashes of the blocks generated",
        {{RPCResult::Type::STR_HEX, "", "Block hash."}}},
    RPCExamples{
        HelpExampleCli("generate", "11") +
        HelpExampleRpc("generate", "11")},
};
const RPCHelpMan& generate_helpman() { return generate_help; }

UniValue generate(const UniValue& params)
{
    const int nblocks = params[0].get_int();
    return MineNBlocks(nblocks, /*dest_address=*/"");
}

static const RPCHelpMan generatesuperblock_help{
    "generatesuperblock",
    "Mine one regtest block carrying the current local superblock contract\n"
    "(built from scraper convergence or, on -regtest where scrapers are\n"
    "disabled, from any local quorum state). Auto-attach is disabled under\n"
    "-regtest; this RPC is the only path.\n"
    "\n"
    "TODO: accept JSON params to construct a synthetic superblock directly\n"
    "rather than relying on Quorum::CreateSuperblock() state. For now, this\n"
    "mines one block and the regtest miner code path skips superblock attach\n"
    "(Phase 2A gate) -- so this is a stub until the 2B follow-up wires the\n"
    "explicit-attach path.",
    {},
    RPCResult{RPCResult::Type::ARR, "", "Hashes of the blocks generated",
        {{RPCResult::Type::STR_HEX, "", "Block hash."}}},
    RPCExamples{
        HelpExampleCli("generatesuperblock", "") +
        HelpExampleRpc("generatesuperblock", "")},
};
const RPCHelpMan& generatesuperblock_helpman() { return generatesuperblock_help; }

UniValue generatesuperblock(const UniValue& params)
{
    if (!Params().IsMockableChain()) {
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "generatesuperblock only available on -regtest");
    }
    // TODO(2B): Construct a GRC::Superblock from caller-supplied JSON, bypass
    // the IsMockableChain() short-circuit in AddSuperblockContractOrVote for
    // this one call, and attach. For now, just mines a plain block so the
    // RPC surface is reachable.
    return MineNBlocks(1, "");
}

static const RPCHelpMan parseaccrualsnapshotfile_help{
    "parseaccrualsnapshotfile",
    "Parses an accrual snapshot from a valid snapshot file on disk.",
    {
        {"filespec", RPCArg::Type::STR, RPCArg::Optional::NO, "Path to the snapshot file."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "version", "Snapshot version."},
            {RPCResult::Type::NUM, "height", "Snapshot block height."},
            {RPCResult::Type::OBJ_DYN, "records", "Mapping of CPID to accrual amount",
                {{RPCResult::Type::STR_AMOUNT, "cpid", "Accrual amount at the snapshot for this CPID."}}},
        }},
    RPCExamples{
        HelpExampleCli("parseaccrualsnapshotfile", "\"/path/to/accrual/5000000\"") +
        HelpExampleRpc("parseaccrualsnapshotfile", "\"/path/to/accrual/5000000\"")},
};
const RPCHelpMan& parseaccrualsnapshotfile_helpman() { return parseaccrualsnapshotfile_help; }

UniValue parseaccrualsnapshotfile(const UniValue& params)
{
    UniValue res(UniValue::VOBJ);

    const fs::path snapshot_path = params[0].get_str();

    if (!fs::is_regular_file(snapshot_path))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid snapshot file specified.");
    }

    const AccrualSnapshot snapshot = AccrualSnapshotReader(snapshot_path).Read();
    const AccrualSnapshot::AccrualMap& records = snapshot.m_records;
    const std::map<Cpid, int64_t> sorted_records(records.begin(), records.end());

    UniValue accruals(UniValue::VOBJ);

    for (const auto& iter : sorted_records)
    {
        UniValue entry(UniValue::VOBJ);

        accruals.pushKV(iter.first.ToString(), ValueFromAmount(iter.second));
    }

    res.pushKV("version", (uint64_t) snapshot.m_version);
    res.pushKV("height", snapshot.m_height);
    res.pushKV("records", accruals);

    return res;
}
