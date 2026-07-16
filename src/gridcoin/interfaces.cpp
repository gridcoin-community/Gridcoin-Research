// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/staking.h"
#include "interfaces/mrc.h"

#include "amount.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/mrc.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "interfaces/handler.h"
#include "main.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "txmempool.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace interfaces {
namespace {

//! In-process StakingStatus implementation wrapping g_miner_status and the
//! staking difficulty helpers. The try* variants preserve the GUI's
//! historical TRY_LOCK(cs_main) semantics: skip the refresh rather than
//! block a GUI-thread slot on a contended lock.
class StakingStatusImpl : public StakingStatus
{
public:
    bool isStaking() override { return g_miner_status.StakingActive(); }

    std::string getErrors() override { return g_miner_status.FormatErrors(); }

    double getNetworkWeight() override
    {
        LOCK(cs_main);
        return GRC::GetEstimatedNetworkWeight();
    }

    std::optional<double> tryGetNetworkWeight() override
    {
        TRY_LOCK(cs_main, locked);

        if (!locked) {
            return std::nullopt;
        }

        return GRC::GetEstimatedNetworkWeight();
    }

    std::optional<double> tryGetEstimatedTimeToStake() override
    {
        TRY_LOCK(cs_main, locked);

        if (!locked) {
            return std::nullopt;
        }

        return GRC::GetEstimatedTimetoStake();
    }
};

//! In-process MRC implementation over the node's single wallet. snapshot()
//! reproduces MRCModel::refresh()'s former GUI-thread reads (chain tip, version
//! gate, trial CreateMRC, mempool queue) under one cs_main hold and returns
//! them as one consistent value snapshot; submit() reproduces
//! MRCModel::submitMRC(), recreating the claim fresh at the current tip so the
//! broadcast is atomic. The GUI keeps all classification (status enums, display
//! strings) and no longer touches core state — see interfaces/mrc.h.
class MRCImpl : public MRC
{
public:
    explicit MRCImpl(CWallet* wallet) : m_wallet(wallet) {}

    MRCSnapshot snapshot(CAmount fee_boost, bool wallet_locked, bool researcher_eligible) override
    {
        MRCSnapshot snap;

        LOCK(cs_main);

        // Defensive: before the chain tip exists, present as out-of-sync rather
        // than dereferencing a null tip. In practice the GUI models are built
        // well past genesis load, so pindexBest is set.
        if (!pindexBest) {
            snap.out_of_sync = true;
            return snap;
        }

        // Populate the chain-state flags up front (cheap tip reads) so the GUI
        // can classify OUT_OF_SYNC vs INVALID_BLOCK_VERSION correctly. In
        // particular block_version_valid must reflect the current tip even while
        // out of sync, matching MRCModel::getMRCModelStatus()'s former ordering
        // (IsV12Enabled(m_block_height) checked before OutOfSyncByAge()).
        snap.block_height = pindexBest->nHeight;
        snap.block_version_valid = IsV12Enabled(pindexBest->nHeight);
        snap.out_of_sync = OutOfSyncByAge();

        // Out of sync: the GUI shows the out-of-sync state and ignores the rest.
        if (snap.out_of_sync) {
            return snap;
        }

        // The GUI owns researcher eligibility (active beacon / not-noncruncher /
        // not-pool; ResearcherModel, migrated in 1d-iv). When the user is not a
        // valid researcher, CreateMRC would throw MRC_error on every block; skip
        // the trial claim and queue scan and let the GUI classify.
        if (!researcher_eligible) {
            return snap;
        }

        if (!snap.block_version_valid) {
            return snap;
        }

        snap.output_limit = static_cast<int>(GetMRCOutputLimit(pindexBest->nVersion, false));

        GRC::MRC mrc;
        CAmount reward = 0;
        CAmount min_fee = 0;

        // First run with fee = 0 computes the minimum required fee. CreateMRC
        // takes the wallet lock in its own scope. wallet_locked -> no_sign
        // (a locked wallet cannot sign the trial claim).
        try {
            GRC::CreateMRC(pindexBest, mrc, reward, min_fee, m_wallet, wallet_locked);
        } catch (GRC::MRC_error& e) {
            snap.create_error = true;
            snap.create_error_msg = e.what();
        }

        snap.min_fee = min_fee;

        // The total fee the caller is requesting: min_fee + boost when a boost is
        // set, 0 otherwise. This intended value drives the excessive-fee
        // classification GUI-side.
        const CAmount total_fee = (fee_boost != 0) ? min_fee + fee_boost : 0;
        snap.fee = total_fee;

        if (fee_boost != 0) {
            // Rerun with the boosted fee so the mempool queue comparison below
            // uses the fee the request would actually carry (mrc.m_fee).
            // CreateMRC takes the fee by non-const reference and resets it to the
            // computed minimum when the provided fee is out of range (before
            // throwing), so pass a scratch copy to keep snap.fee = the intended
            // total.
            CAmount rerun_fee = total_fee;
            try {
                GRC::CreateMRC(pindexBest, mrc, reward, rerun_fee, m_wallet, wallet_locked);
            } catch (GRC::MRC_error& e) {
                snap.boosted_fee_error = true;
                snap.boosted_fee_error_msg = e.what();
            }
        }

        // reward is fee-independent; both runs set it to the research subsidy, so
        // read it after the (last) run.
        snap.reward = reward;

        // Fee-ordered mempool MRC queue evaluated against this request's object
        // fee (mrc.m_fee == min_fee with no boost, the boosted fee otherwise).
        CAmount tail = std::numeric_limits<CAmount>::max();
        CAmount head = 0;
        bool found = false;

        const std::vector<std::pair<CAmount, GRC::Cpid>> mrc_queue = mempool.GetMRCQueue();

        GRC::Cpid mrc_cpid;
        if (auto cpid = mrc.m_mining_id.TryCpid()) mrc_cpid = *cpid;

        for (const auto& [mempool_fee, mempool_cpid] : mrc_queue) {
            found |= mempool_cpid == mrc_cpid;

            if (!found && mempool_fee >= mrc.m_fee) ++snap.pos;
            head = std::max(head, mempool_fee);
            tail = std::min(tail, mempool_fee);

            ++snap.queue_length;
        }

        // Tail converges from CAmount max, but cannot exceed head (e.g. an empty
        // queue leaves tail at max and head at 0).
        tail = std::min(head, tail);

        // The last queue slot that would still be paid: min(queue size,
        // output_limit) - 1, capped at the head fee.
        int pay_limit_fee_pos = std::min<int>(mrc_queue.size(), snap.output_limit) - 1;

        CAmount pay_limit = std::numeric_limits<CAmount>::max();
        if (pay_limit_fee_pos >= 0) {
            pay_limit = mrc_queue[pay_limit_fee_pos].first;
        }
        pay_limit = std::min(head, pay_limit);

        snap.found_in_queue = found;
        snap.queue_head_fee = head;
        snap.queue_tail_fee = tail;
        snap.queue_pay_limit_fee = pay_limit;

        return snap;
    }

    MRCSubmitResult submit(CAmount fee_boost, bool wallet_locked) override
    {
        MRCSubmitResult res;

        // A locked wallet cannot sign the claim: CreateMRC would build it
        // unsigned (no_sign) and the broadcast below would relay an invalid MRC.
        // The GUI only reaches submit() from the ELIGIBLE state (never locked),
        // but enforce the interface contract here so a future or out-of-process
        // caller cannot broadcast an unsigned claim.
        if (wallet_locked) {
            res.error = "Wallet is locked; unlock it before submitting an MRC.";
            return res;
        }

        LOCK(cs_main);

        if (!pindexBest) {
            res.error = "No chain tip.";
            return res;
        }

        LOCK(m_wallet->cs_wallet);

        GRC::MRC mrc;
        CAmount reward = 0;
        CAmount fee = 0;

        // Recreate the claim fresh at the current tip so the broadcast reflects
        // the fee the user acted on. First run computes the min fee; the rerun
        // applies the boost, mirroring snapshot()/refresh(). The wallet is
        // guaranteed unlocked here (guard above), so always sign (no_sign=false).
        try {
            GRC::CreateMRC(pindexBest, mrc, reward, fee, m_wallet, /*no_sign=*/false);

            if (fee_boost != 0) {
                fee = fee + fee_boost;
                GRC::CreateMRC(pindexBest, mrc, reward, fee, m_wallet, /*no_sign=*/false);
            }
        } catch (GRC::MRC_error& e) {
            res.error = e.what();
            return res;
        }

        uint32_t contract_version = IsV13Enabled(nBestHeight) ? 3 : 2;

        auto [wtx, e_str] = GRC::SendContract(GRC::MakeContract<GRC::MRC>(contract_version,
                                                                          GRC::ContractAction::ADD,
                                                                          mrc));
        if (!e_str.empty()) {
            res.error = e_str;
            return res;
        }

        res.success = true;
        res.submitted_height = pindexBest->nHeight;
        res.research_subsidy = mrc.m_research_subsidy;
        return res;
    }

    bool isOutOfSync() override
    {
        // OutOfSyncByAge() is a lock-free time comparison (GetAdjustedTime() vs
        // the g_previous_block_time global), matching the former direct GUI-thread
        // call in MRCModel::getMRCModelStatus().
        return OutOfSyncByAge();
    }

    std::unique_ptr<Handler> handleMRCChanged(MRCChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.MRCChanged_connect(std::move(fn)));
    }

private:
    CWallet* m_wallet;
};

} // namespace

std::unique_ptr<StakingStatus> MakeStakingStatus()
{
    return std::make_unique<StakingStatusImpl>();
}

std::unique_ptr<MRC> MakeMRC(CWallet* wallet)
{
    return std::make_unique<MRCImpl>(wallet);
}

} // namespace interfaces
