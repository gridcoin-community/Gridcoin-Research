// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
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

using namespace std;

UniValue getmininginfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\n"
            "Returns an object containing mining-related information\n");

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
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        nWeight = GRC::GetStakeWeight(*pwalletMain);
        nNetworkWeight = GRC::GetEstimatedNetworkWeight();
        nCurrentDiff = GRC::GetCurrentDifficulty();
        nTargetDiff = GRC::GetTargetDifficulty();
        nExpectedTime = GRC::GetEstimatedTimetoStake();
    }

    obj.pushKV("blocks", nBestHeight);
    diff.pushKV("current", nCurrentDiff);
    diff.pushKV("target", nTargetDiff);

    { LOCK(g_miner_status.lock);
        // not using real weight to not break calculation
        bool staking = g_miner_status.nLastCoinStakeSearchInterval && g_miner_status.WeightSum;
        diff.pushKV("last-search-interval", g_miner_status.nLastCoinStakeSearchInterval);
        weight.pushKV("minimum",    g_miner_status.WeightMin);
        weight.pushKV("maximum",    g_miner_status.WeightMax);
        weight.pushKV("combined",   g_miner_status.WeightSum);
        weight.pushKV("valuesum",   g_miner_status.ValueSum);
        weight.pushKV("legacy",   nWeight/(double)COIN);
        obj.pushKV("stakeweight", weight);
        obj.pushKV("netstakeweight", nNetworkWeight);
        obj.pushKV("netstakingGRCvalue", nNetworkWeight / 80.0);
        obj.pushKV("staking", staking);
        obj.pushKV("mining-error", g_miner_status.ReasonNotStaking);
        obj.pushKV("time-to-stake_days", nExpectedTime/86400.0);
        obj.pushKV("expectedtime", nExpectedTime);
        obj.pushKV("mining-version", g_miner_status.Version);
        obj.pushKV("mining-created", g_miner_status.CreatedCnt);
        obj.pushKV("mining-accepted", g_miner_status.AcceptedCnt);
        obj.pushKV("mining-kernels-found", g_miner_status.KernelsFound);

        obj.pushKV("masked_time_intervals_covered", g_miner_status.masked_time_intervals_covered);
        obj.pushKV("masked_time_intervals_elapsed", g_miner_status.masked_time_intervals_elapsed);

        double staking_loop_efficiency = 0.0;
        if (g_miner_status.masked_time_intervals_elapsed > 0)
        {
            staking_loop_efficiency = g_miner_status.masked_time_intervals_covered * 100.0
                    / (double) g_miner_status.masked_time_intervals_elapsed;
        }

        obj.pushKV("staking_loop_efficiency", staking_loop_efficiency);

        obj.pushKV("actual_cumulative_weight", g_miner_status.actual_cumulative_weight);
        obj.pushKV("ideal_cumulative_weight", g_miner_status.ideal_cumulative_weight);

        double staking_efficiency = 0.0;
        if (g_miner_status.ideal_cumulative_weight > 0.0)
        {
            staking_efficiency = g_miner_status.actual_cumulative_weight * 100.0
                                 / g_miner_status.ideal_cumulative_weight;
        }
        obj.pushKV("staking_efficiency", staking_efficiency);
    }

    int64_t nMinStakeSplitValue = 0;
    double dEfficiency = 0;
    int64_t nDesiredStakeSplitValue = 0;
    SideStakeAlloc vSideStakeAlloc;

    LOCK(cs_main);

    // nMinStakeSplitValue, dEfficiency, and nDesiredStakeSplitValue are out parameters.
    bool fEnableStakeSplit = GetStakeSplitStatusAndParams(nMinStakeSplitValue, dEfficiency, nDesiredStakeSplitValue);

    // vSideStakeAlloc is an out parameter.
    bool fEnableSideStaking = GetSideStakingStatusAndAlloc(vSideStakeAlloc);

    stakesplitting.pushKV("stake-splitting-enabled", fEnableStakeSplit);
    if (fEnableStakeSplit)
    {
        stakesplittingparam.pushKV("min-stake-split-value", nMinStakeSplitValue / COIN);
        stakesplittingparam.pushKV("efficiency", dEfficiency);
        stakesplittingparam.pushKV("stake-split-UTXO-size-for-target-efficiency", nDesiredStakeSplitValue / COIN);
        stakesplitting.pushKV("stake-splitting-params", stakesplittingparam);
    }
    obj.pushKV("stake-splitting", stakesplitting);

    sidestaking.pushKV("side-staking-enabled", fEnableSideStaking);
    if (fEnableSideStaking)
    {
        for (const auto& alloc : vSideStakeAlloc)
        {
            sidestakingalloc.pushKV("address", alloc.first);
            sidestakingalloc.pushKV("allocation-pct", alloc.second * 100);

            vsidestakingalloc.push_back(sidestakingalloc);
        }
        sidestaking.pushKV("side-staking-allocations", vsidestakingalloc);
    }
    obj.pushKV("side-staking", sidestaking);

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

    obj.pushKV("researcher_status", msMiningErrors);
    obj.pushKV("current_poll", GRC::GetCurrentPollTitle());

    return obj;
}

UniValue getlaststake(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getlaststake\n"
            "\n"
            "Fetch information about this wallet's last staked block.\n");

    const boost::optional<CWalletTx> stake_tx = GetLastStake(*pwalletMain);

    if (!stake_tx) {
        throw JSONRPCError(RPC_WALLET_ERROR, "No prior staked blocks found.");
    }

    int64_t height;
    int64_t timestamp;
    int64_t confirmations;

    int64_t mint_amount = 0;
    int64_t side_stake_amount = 0;
    int64_t research_reward_amount;

    {
        LOCK(cs_main);

        const CBlockIndex* const pindex = mapBlockIndex[stake_tx->hashBlock];

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

    json.pushKV("block", stake_tx->hashBlock.ToString());
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
        json.pushKV("address", CBitcoinAddress(dest).ToString());
    } else {
        json.pushKV("address", "");
    }

    json.pushKV("label", stake_tx->strFromAccount);

    return json;
}

extern double CoinToDouble(double surrogate);

UniValue auditsnapshotaccrual(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "auditsnapshotaccrual [CPID] [report details]\n"
                "\n"
                "Report accrual snapshot deltas for the specified CPID.\n");

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
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
    }

    if (!pindexBest) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Invalid chain.");
    }

    UniValue result(UniValue::VOBJ);
    UniValue audit(UniValue::VARR);

    LOCK(cs_main);

    if (!IsV11Enabled(nBestHeight + 1)) {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Wait for block v11 protocol");
    }

    const int64_t now = GetAdjustedTime();
    const GRC::ResearchAccount& account = GRC::Tally::GetAccount(*cpid);
    const int64_t computed = GRC::Tally::GetAccrual(*cpid, now, pindexBest);
    const int64_t newbie_correction = Tally::GetNewbieSuperblockAccrualCorrection(*cpid, GRC::Quorum::CurrentSuperblock());

    bool accrual_account_exists = true;

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

    GRC::Beacon_ptr beacon_ptr = beacon_try;

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: active beacon: timestamp = %" PRId64 ", ctx_hash = %s,"
                                       " prev_beacon_ctx_hash = %s",
             __func__,
             beacon_ptr->m_timestamp,
             beacon_ptr->m_hash.GetHex(),
             beacon_ptr->m_prev_beacon_hash.GetHex());

    UniValue beacon_chain(UniValue::VARR);
    UniValue beacon_chain_entry(UniValue::VOBJ);

    beacon_chain_entry.pushKV("ctx_hash", beacon_ptr->m_hash.GetHex());
    beacon_chain_entry.pushKV("timestamp",  beacon_ptr->m_timestamp);
    beacon_chain.push_back(beacon_chain_entry);

    // This walks back the entries in the historical beacon map linked by renewal prev tx hash until the first
    // beacon in the renewal chain is found (the original advertisement). The accrual starts no earlier than here.
    uint64_t renewals = 0;
    while (beacon_ptr->Renewed() && renewals <= 100)
    {
        auto iter = beacons.GetBeaconDB().find(beacon_ptr->m_prev_beacon_hash);

        beacon_ptr = std::make_shared<Beacon>(iter->second);

        LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: renewal %u beacon: timestamp = %" PRId64 ", ctx_hash = %s,"
                                           " prev_beacon_ctx_hash = %s.",
                 __func__,
                 renewals,
                 beacon_ptr->m_timestamp,
                 beacon_ptr->m_hash.GetHex(),
                 beacon_ptr->m_prev_beacon_hash.GetHex());

        beacon_chain_entry.pushKV("ctx_hash", beacon_ptr->m_hash.GetHex());
        beacon_chain_entry.pushKV("timestamp", beacon_ptr->m_timestamp);
        beacon_chain.push_back(beacon_chain_entry);

        ++renewals;
    }

    GRC::SuperblockPtr superblock;

    const CBlockIndex* pindex_baseline = GRC::Tally::GetBaseline();

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: pindex_baseline->nHeight = %i", __func__, pindex_baseline->nHeight);

    const CBlockIndex* pindex_superblock;

    // Find the first superblock after the baseline within scope of the beacon chain for the given CPID as the starting
    // point for the audit.
    for (pindex_superblock = pindex_baseline;
        pindex_superblock;
        pindex_superblock = pindex_superblock->pnext)
    {
        if (pindex_superblock->IsSuperblock() && pindex_superblock->nTime >= beacon_ptr->m_timestamp) {
            superblock = SuperblockPtr::ReadFromDisk(pindex_superblock);
            break;
        }
    }

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: First in scope superblock nHeight = %i", __func__,
             pindex_superblock->nHeight);

    // Set the pindex_low to the pindex_superblock. For right now, we are going to take the accrual at the first snapshot
    // after the flip to v11 (the second snapshot in the accrual directory recorded at the first SB after the transition
    // height) as gospel. This doesn't allow us to verify the accrual between the transition height and the first snapshot
    // afterwards, but it drastically reduces the complexity of the audit.
    const CBlockIndex* pindex = pindex_superblock;
    const CBlockIndex* pindex_low = pindex_superblock;

    const fs::path snapshot_path = SnapshotPath(pindex_superblock->nHeight);
    const AccrualSnapshot snapshot = AccrualSnapshotReader(snapshot_path).Read();

    int64_t accrual = 0;
    auto entry = snapshot.m_records.find(cpid.get());

    if (entry != snapshot.m_records.end())
    {
        accrual = entry->second;
    }

    const auto tally_accrual_period = [&](
        const std::string& boundary,
        const uint64_t height,
        const int64_t low_time,
        const int64_t high_time,
        const int64_t claimed)
    {
        const GRC::Magnitude magnitude = superblock->m_cpids.MagnitudeOf(*cpid);

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
            * MAG_UNIT_NUMERATOR;

        int64_t period = 0;

        if (base_accrual > std::numeric_limits<uint64_t>::max() / COIN) {
            arith_uint256 accrual_bn(base_accrual);
            accrual_bn *= COIN;
            accrual_bn /= 86400;
            accrual_bn /= Magnitude::SCALE_FACTOR;
            accrual_bn /= MAG_UNIT_DENOMINATOR;

            period = accrual_bn.GetLow64() * (int64_t) sign;
        }
        else
        {
            period = base_accrual * (int64_t) sign
                    * COIN
                    / 86400
                    / Magnitude::SCALE_FACTOR
                    / MAG_UNIT_DENOMINATOR;
        }

        accrual += period;

        // TODO: Change this to refer to MaxReward() from the snapshot computer.
        int64_t max_reward = 16384 * COIN;

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

    for (; pindex; pindex = pindex->pnext) {
        if (pindex->ResearchSubsidy() > 0 && pindex->GetMiningId() == *cpid) {
            tally_accrual_period(
                "stake",
                pindex->nHeight,
                pindex_low->nTime,
                pindex->nTime,
                pindex->ResearchSubsidy());

            accrual = 0;
            pindex_low = pindex;
        } else if (pindex->IsSuperblock()) {
            tally_accrual_period(
                "superblock",
                pindex->nHeight,
                pindex_low->nTime,
                pindex->nTime,
                0);

            pindex_low = pindex;
        }

        if (pindex->IsSuperblock()) {
            superblock = SuperblockPtr::ReadFromDisk(pindex);
        }
    }

    // The final period is from the last event till "now".
    int64_t period = tally_accrual_period("tip", 0, pindex_low->nTime, now, 0);

    result.pushKV("cpid", cpid->ToString());
    result.pushKV("accrual_account_exists", accrual_account_exists);
    result.pushKV("latest_beacon_timestamp", beacon_chain[0]);
    result.pushKV("original_beacon_timestamp", beacon_chain[beacon_chain.size() - 1]);
    result.pushKV("renewals", renewals);
    result.pushKV("accrual_by_audit", accrual);
    result.pushKV("accrual_by_GetAccrual", computed);
    result.pushKV("newbie_correction", newbie_correction);
    result.pushKV("accrual_last_period", period);

    if (report_details) {
        result.pushKV("beacon_chain", beacon_chain);
        result.pushKV("audit", audit);
    }

        return result;
}

UniValue auditsnapshotaccruals(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "auditsnapshotaccruals [report only mismatches]\n"
                "\n"
                "Report accrual audit for entire population of CPIDs.\n");

    bool report_only_mismatches = false;

    if (params.size() > 0)
    {
        report_only_mismatches = params[0].get_bool();
    }

    UniValue result(UniValue::VOBJ);

    SuperblockPtr superblock = GRC::Quorum::CurrentSuperblock();

    UniValue entries(UniValue::VARR);
    int number_of_cpids = 0;
    int number_of_matches = 0;
    int number_of_mismatches = 0;
    int number_of_mismatches_last_period_only = 0;
    int number_accrual_accounts_not_present = 0;
    int number_not_present = 0;

    for (const auto& iter : superblock->m_cpids)
    {
        std::vector<UniValue> v_params {iter.Cpid().ToString(), false};

        UniValue internal_params(UniValue::VARR);

        internal_params.push_backV(v_params);

        UniValue match_status(UniValue::VOBJ);

        UniValue audit(auditsnapshotaccrual(internal_params, false));

        if (!audit.empty())
        {
            const CAmount& accrual_by_audit = find_value(audit, "accrual_by_audit").get_int64();
            const CAmount& accrual_by_GetAccrual = find_value(audit, "accrual_by_GetAccrual").get_int64();
            const CAmount& accrual_last_period = find_value(audit, "accrual_last_period").get_int64();
            const bool accrual_account_exists = find_value(audit, "accrual_account_exists").get_bool();

            if (accrual_by_audit == accrual_by_GetAccrual)
            {
                if (!report_only_mismatches)
                {
                    match_status.pushKV("CPID", iter.Cpid().ToString());
                    match_status.pushKV("match", audit);
                    entries.push_back(match_status);
                }
                ++number_of_matches;
            }
            else
            {
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
        else
        {
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

UniValue listresearcheraccounts(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "listresearcheraccrounts\n"
            "\n"
            "List researcher accounts in the accrual system and their current accruals.\n");

    UniValue result(UniValue::VOBJ);
    UniValue entries(UniValue::VARR);

    const int64_t now = GetAdjustedTime();

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

UniValue comparesnapshotaccrual(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "comparesnapshotaccrual\n"
            "\n"
            "Compare snapshot and legacy accrual for active CPIDs.\n");

    const int64_t now = GetAdjustedTime();

    size_t active_account_count = 0;
    int64_t legacy_total = 0;
    int64_t snapshot_total = 0;

    UniValue result(UniValue::VOBJ);

    LOCK(cs_main);

    if (!IsV11Enabled(nBestHeight + 1)) {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Wait for block v11 protocol");
    }

    for (const auto& account_pair : GRC::Tally::Accounts()) {
        const GRC::Cpid& cpid = account_pair.first;
        const GRC::ResearchAccount& account = account_pair.second;

        const GRC::AccrualComputer legacy = GRC::Tally::GetLegacyComputer(cpid, now, pindexBest);
        const GRC::AccrualComputer snapshot = GRC::Tally::GetSnapshotComputer(cpid, now, pindexBest);

        const int64_t legacy_accrual = legacy->RawAccrual();
        const int64_t snapshot_accrual = snapshot->RawAccrual();

        if (legacy_accrual == 0 && snapshot_accrual == 0) {
            if (!account.IsNew() && !account.IsActive(pindexBest->nHeight)) {
                continue;
            }
        }

        UniValue accrual(UniValue::VOBJ);

        accrual.pushKV("legacy", ValueFromAmount(legacy_accrual));
        accrual.pushKV("snapshot", ValueFromAmount(snapshot_accrual));

        //UniValue params(UniValue::VARR);
        //params.push_back(cpid.ToString());
        //accrual.pushKV("audit", auditdeltaaccrual(params, false));

        result.pushKV(cpid.ToString(), accrual);

        legacy_total += legacy_accrual;
        snapshot_total += snapshot_accrual;
        ++active_account_count;
    }

    if (!active_account_count) {
        throw JSONRPCError(RPC_MISC_ERROR, "There are no active accounts.");
    }

    UniValue summary(UniValue::VOBJ);

    summary.pushKV("active_accounts", (uint64_t)active_account_count);
    summary.pushKV("legacy_total", ValueFromAmount(legacy_total));
    summary.pushKV("legacy_average", ValueFromAmount(legacy_total / active_account_count));
    summary.pushKV("snapshot_total", ValueFromAmount(snapshot_total));
    summary.pushKV("snapshot_average", ValueFromAmount(snapshot_total / active_account_count));

    result.pushKV("summary", summary);

    return result;
}

UniValue inspectaccrualsnapshot(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "inspectaccrualsnapshot <height>\n"
            "\n"
            "<height> --> block height (and file name) of the snapshot"
            "\n"
            "Display the contents of an accrual snapshot from accrual repository on disk.\n");


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

UniValue parseaccrualsnapshotfile(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "parseaccrualsnapshot <filespec>\n"
                "\n"
                "<filespec> -> String - path to file."
                "\n"
                "Parses accrual snapshot from a valid snapshot file.\n");

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
