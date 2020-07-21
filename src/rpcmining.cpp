// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"
#include "miner.h"
#include "neuralnet/accrual/snapshot.h"
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
        // not using real weight to not break calculation
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

    if (!IsV11Enabled(nBestHeight + 1)) {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Wait for block v11 protocol");
    }

    const int64_t now = GetAdjustedTime();
    const int64_t computed = NN::Tally::GetAccrual(*cpid, now, pindexBest);
    const CBlockIndex* pindex = pindexBest;
    const CBlockIndex* pindex_low = pindex;
    const int64_t threshold = GetV11Threshold();
    const int64_t max_depth = IsV11Enabled(pindex->nHeight)
        ? threshold - BLOCKS_PER_DAY * 30 * 6
        : pindex->nHeight + 1 - BLOCKS_PER_DAY * 30 * 6;

    NN::SuperblockPtr superblock;

    for (; pindex && pindex->nHeight > max_depth; pindex = pindex->pprev);

    for (const CBlockIndex* pindex_superblock = pindex;
        pindex_superblock;
        pindex_superblock = pindex_superblock->pprev)
    {
        if (pindex_superblock->nIsSuperBlock == 1) {
            superblock = SuperblockPtr::ReadFromDisk(pindex_superblock);
            break;
        }
    }

    int64_t accrual = 0;

    const auto tally_accrual_period = [&](
        const std::string& boundary,
        const uint64_t height,
        const int64_t low_time,
        const int64_t high_time,
        const int64_t claimed)
    {
        constexpr double magnitude_unit = 0.25;
        const double accrual_days = (high_time - low_time) / 86400.0;
        const double magnitude = superblock->m_cpids.MagnitudeOf(*cpid).Floating();
        const int64_t period = accrual_days * magnitude * magnitude_unit * COIN;

        accrual += period;

        UniValue accrual_out(UniValue::VOBJ);
        accrual_out.pushKV("period", ValueFromAmount(period));
        accrual_out.pushKV("accumulated", ValueFromAmount(accrual));
        accrual_out.pushKV("claimed", ValueFromAmount(claimed));

        UniValue delta(UniValue::VOBJ);
        delta.pushKV("boundary", boundary);
        delta.pushKV("height", height ? height : NullUniValue);
        delta.pushKV("time", high_time);
        delta.pushKV("magnitude", magnitude);
        delta.pushKV("accrual", accrual_out);

        audit.push_back(delta);

        return accrual_days * magnitude * magnitude_unit * COIN;
    };

    for (; pindex; pindex = pindex->pnext) {
        if (pindex->nResearchSubsidy > 0 && pindex->GetMiningId() == *cpid) {
            tally_accrual_period(
                "stake",
                pindex->nHeight,
                pindex_low->nTime,
                pindex->nTime,
                pindex->nResearchSubsidy);

            accrual = 0;
            pindex_low = pindex;
        } else if (pindex->nIsSuperBlock == 1) {
            tally_accrual_period(
                "superblock",
                pindex->nHeight,
                pindex_low->nTime,
                pindex->nTime,
                0);

            pindex_low = pindex;
        }

        if (pindex->nIsSuperBlock == 1) {
            superblock = SuperblockPtr::ReadFromDisk(pindex);
        }
    }

    tally_accrual_period("tip", 0, pindex_low->nTime, GetAdjustedTime(), 0);

    result.pushKV("audit", audit);
    result.pushKV("computed", ValueFromAmount(computed));

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

    boost::filesystem::path snapshot_path = params[0].get_str();

    if (!boost::filesystem::is_regular_file(snapshot_path))
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
