// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"
#include "miner.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/superblock.h"
#include "neuralnet/tally.h"
#include "rpcprotocol.h"
#include "rpcserver.h"

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
        pwalletMain->GetStakeWeight(nWeight);

        nNetworkWeight = GetEstimatedNetworkWeight();
        nCurrentDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
        nTargetDiff = GetBlockDifficulty(GetNextTargetRequired(pindexBest));
        nExpectedTime = GetEstimatedTimetoStake();
    }

    obj.pushKV("blocks", nBestHeight);
    diff.pushKV("current", nCurrentDiff);
    diff.pushKV("target", nTargetDiff);

    { LOCK(MinerStatus.lock);
        // not using real weigh to not break calculation
        bool staking = MinerStatus.nLastCoinStakeSearchInterval && MinerStatus.WeightSum;
        diff.pushKV("last-search-interval", MinerStatus.nLastCoinStakeSearchInterval);
        weight.pushKV("minimum",    MinerStatus.WeightMin);
        weight.pushKV("maximum",    MinerStatus.WeightMax);
        weight.pushKV("combined",   MinerStatus.WeightSum);
        weight.pushKV("valuesum",   MinerStatus.ValueSum);
        weight.pushKV("legacy",   nWeight/(double)COIN);
        obj.pushKV("stakeweight", weight);
        obj.pushKV("netstakeweight", nNetworkWeight);
        obj.pushKV("netstakingGRCvalue", nNetworkWeight / 80.0);
        obj.pushKV("staking", staking);
        obj.pushKV("mining-error", MinerStatus.ReasonNotStaking);
        obj.pushKV("time-to-stake_days", nExpectedTime/86400.0);
        obj.pushKV("expectedtime", nExpectedTime);
        obj.pushKV("mining-version", MinerStatus.Version);
        obj.pushKV("mining-created", MinerStatus.CreatedCnt);
        obj.pushKV("mining-accepted", MinerStatus.AcceptedCnt);
        obj.pushKV("mining-kernels-found", MinerStatus.KernelsFound);
        obj.pushKV("kernel-diff-best",MinerStatus.KernelDiffMax);
        obj.pushKV("kernel-diff-sum",MinerStatus.KernelDiffSum);
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

    const NN::MiningId mining_id = NN::Researcher::Get()->Id();
    obj.pushKV("CPID", mining_id.ToString());

    if (const NN::CpidOption cpid = mining_id.TryCpid())
    {
        const NN::AccrualComputer calc = NN::Tally::GetComputer(*cpid, nTime, pindexBest);

        obj.pushKV("Magnitude Unit", calc->MagnitudeUnit());
        obj.pushKV("BoincRewardPending", ValueFromAmount(calc->Accrual()));
    }

    std::string current_poll;

    try {
        current_poll = GetCurrentOverviewTabPoll();
    } catch (std::exception &e) {
        current_poll = _("No current polls");
        LogPrintf("Error obtaining last poll: %s", e.what());
    }

    obj.pushKV("researcher_status", msMiningErrors);
    obj.pushKV("current_poll", current_poll);

    return obj;
}


UniValue activatesnapshotaccrual(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "activatesnapshotaccrual\n"
            "\n"
            "TESTING ONLY: enable delta research reward accrual calculations.\n");

    LOCK(cs_main);

    return NN::Tally::ActivateSnapshotAccrual(pindexBest);
}

UniValue auditsnapshotaccrual(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "auditsnapshotaccrual\n"
            "\n"
            "Report accrual snapshot deltas for the specified CPID.\n");

    const NN::MiningId mining_id = params.size() > 0
        ? NN::MiningId::Parse(params[0].get_str())
        : NN::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    const NN::CpidOption cpid = mining_id.TryCpid();

    if (!cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
    }

    if (!pindexBest) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Invalid chain.");
    }

    UniValue result(UniValue::VOBJ);
    UniValue audit(UniValue::VARR);

    LOCK(cs_main);

    if (!NN::Tally::ActivateSnapshotAccrual(pindexBest)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Snapshot accrual activation failed.");
    }

    const int64_t now = GetAdjustedTime();
    const CBlockIndex* pindex = pindexBest;
    const CBlockIndex* pindex_end = pindex;
    const int64_t max_depth = IsV11Enabled(pindex->nHeight)
        ? GetV11Threshold() - BLOCKS_PER_DAY * 30 * 6
        : pindex->nHeight + 1 - BLOCKS_PER_DAY * 30 * 6;

    NN::ResearchAccount account = NN::Tally::GetAccount(*cpid); // copy
    NN::SuperblockPtr superblock = NN::Quorum::CurrentSuperblock();

    NN::AccrualComputer calc = NN::Tally::GetSnapshotComputer(
        *cpid,
        account,
        now, // payment time
        pindex,
        superblock);

    const int64_t computed_accrual = calc->RawAccrual();

    int64_t total_accrual = 0;
    account.m_accrual = 0;

    // Record the latest stake after the current superblock if one exists:
    for (;
        pindex && pindex->nHeight > superblock.m_height;
        pindex = pindex->pprev)
    {
        if (pindex == account.m_last_block_ptr) {
            calc = NN::Tally::GetSnapshotComputer(*cpid, account, now, pindex, superblock);

            int64_t accrual_now = calc->RawAccrual();
            total_accrual += accrual_now;

            UniValue from(UniValue::VOBJ);
            from.pushKV("boundary", "stake");
            from.pushKV("height", (uint64_t)account.LastRewardHeight());

            UniValue to(UniValue::VOBJ);
            to.pushKV("boundary", "tip");
            to.pushKV("height", NullUniValue);

            UniValue delta(UniValue::VOBJ);
            delta.pushKV("from", from);
            delta.pushKV("to", to);

            delta.pushKV("magnitude", superblock->m_cpids.MagnitudeOf(*cpid).Floating());
            delta.pushKV("accrual", ValueFromAmount(accrual_now));

            audit.push_back(delta);

            pindex = pindexGenesisBlock; // Skip the rest
        }
    }

    // Record accrual from the current superblock to the chain tip:
    if (pindex) {
        calc = NN::Tally::GetSnapshotComputer(*cpid, account, now, pindex, superblock);

        int64_t accrual_now = calc->RawAccrual();
        total_accrual += accrual_now;

        UniValue from(UniValue::VOBJ);
        from.pushKV("boundary", "superblock");
        from.pushKV("height", pindex->nHeight);

        UniValue to(UniValue::VOBJ);
        to.pushKV("boundary", "tip");
        to.pushKV("height", NullUniValue);

        UniValue delta(UniValue::VOBJ);
        delta.pushKV("from", from);
        delta.pushKV("to", to);

        delta.pushKV("magnitude", superblock->m_cpids.MagnitudeOf(*cpid).Floating());
        delta.pushKV("accrual", ValueFromAmount(accrual_now));

        audit.push_back(delta);
    }

    // Record accrual for historical superblocks:
    for (pindex_end = pindex, pindex = pindex ? pindex->pprev : nullptr;
        pindex && pindex->nHeight > max_depth;
        pindex = pindex->pprev)
    {
        if (pindex->nIsSuperBlock != 1) {
            continue;
        }

        CBlock block;

        if (!block.ReadFromDisk(pindex)) {
            throw JSONRPCError(RPC_MISC_ERROR, "Failed to read superblock.");
        }

        superblock = NN::SuperblockPtr::BindShared(block.PullSuperblock(), pindex);

        calc = NN::Tally::GetSnapshotComputer(
            *cpid,
            account,
            pindex_end->nTime, // payment time
            pindex,
            superblock);

        int64_t accrual_now = calc->RawAccrual();
        total_accrual += accrual_now;

        UniValue from(UniValue::VOBJ);
        UniValue to(UniValue::VOBJ);

        if (account.LastRewardHeight() >= (uint64_t)pindex->nHeight) {
            from.pushKV("boundary", "stake");
            from.pushKV("height", (uint64_t)account.LastRewardHeight());

            to.pushKV("boundary", "superblock");
            to.pushKV("height", pindex_end->nHeight);

            pindex = pindexGenesisBlock; // Skip the rest
        } else {
            from.pushKV("boundary", "superblock");
            from.pushKV("height", pindex->nHeight);

            to.pushKV("boundary", "superblock");
            to.pushKV("height", pindex_end->nHeight);
        }

        UniValue delta(UniValue::VOBJ);
        delta.pushKV("from", from);
        delta.pushKV("to", to);

        delta.pushKV("magnitude", superblock->m_cpids.MagnitudeOf(*cpid).Floating());
        delta.pushKV("accrual", ValueFromAmount(accrual_now));

        audit.push_back(delta);

        pindex_end = pindex;
    }

    // If the maximum depth is a superblock, we're done:
    //
    if (pindex && pindex->nIsSuperBlock == 1) {
        pindex = pindexGenesisBlock; // Skip the rest
    }

    const CBlockIndex* const pindex_max = pindex;

    // Record accrual for the period between the max depth and the superblock
    // after that:
    for (; pindex; pindex = pindex->pprev) {
        if (pindex->nIsSuperBlock != 1) {
            continue;
        }

        CBlock block;

        if (!block.ReadFromDisk(pindex)) {
            throw JSONRPCError(RPC_MISC_ERROR, "Failed to read superblock.");
        }

        superblock = NN::SuperblockPtr::BindShared(block.PullSuperblock(), pindex_max);

        calc = NN::Tally::GetSnapshotComputer(
            *cpid,
            account,
            pindex_end->nTime, // payment time
            pindex_max,
            superblock);

        int64_t accrual_now = calc->RawAccrual();
        total_accrual += accrual_now;

        UniValue from(UniValue::VOBJ);
        UniValue to(UniValue::VOBJ);

        if (account.LastRewardHeight() >= (uint64_t)pindex_max->nHeight) {
            from.pushKV("boundary", "stake");
            from.pushKV("height", (uint64_t)account.LastRewardHeight());

            to.pushKV("boundary", "superblock");
            to.pushKV("height", pindex_end->nHeight);
        } else {
            from.pushKV("boundary", "limit");
            from.pushKV("height", pindex_max->nHeight);

            to.pushKV("boundary", "superblock");
            to.pushKV("height", pindex_end->nHeight);
        }

        UniValue delta(UniValue::VOBJ);
        delta.pushKV("from", from);
        delta.pushKV("to", to);

        delta.pushKV("magnitude", superblock->m_cpids.MagnitudeOf(*cpid).Floating());
        delta.pushKV("accrual", ValueFromAmount(accrual_now));

        audit.push_back(delta);

        break;
    }

    result.pushKV("audit", audit);
    result.pushKV("audited_accrual", ValueFromAmount(total_accrual));
    result.pushKV("computed_accrual", ValueFromAmount(computed_accrual));

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

    if (!NN::Tally::ActivateSnapshotAccrual(pindexBest)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Snapshot accrual activation failed.");
    }

    for (const auto& account_pair : NN::Tally::Accounts()) {
        const NN::Cpid& cpid = account_pair.first;
        const NN::ResearchAccount& account = account_pair.second;

        const NN::AccrualComputer legacy = NN::Tally::GetLegacyComputer(cpid, now, pindexBest);
        const NN::AccrualComputer snapshot = NN::Tally::GetSnapshotComputer(cpid, now, pindexBest);

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

    UniValue summary(UniValue::VOBJ);

    summary.pushKV("active_accounts", (uint64_t)active_account_count);
    summary.pushKV("legacy_total", ValueFromAmount(legacy_total));
    summary.pushKV("legacy_average", ValueFromAmount(legacy_total / active_account_count));
    summary.pushKV("snapshot_total", ValueFromAmount(snapshot_total));
    summary.pushKV("snapshot_average", ValueFromAmount(snapshot_total / active_account_count));

    result.pushKV("summary", summary);

    return result;
}
