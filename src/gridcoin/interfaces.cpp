// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/staking.h"
#include "interfaces/mrc.h"
#include "interfaces/sidestake.h"

#include "amount.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/mrc.h"
#include "gridcoin/sidestake.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "interfaces/handler.h"
#include "key_io.h"
#include "main.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "txmempool.h"
#include "util/strencodings.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <variant>
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

//! In-process SideStakeManager over the global sidestake registry (Phase 1d-ii).
//! Presents the unified mandatory+local table as value rows and moves all local
//! add/edit/delete validation (formerly in SideStakeTableModel) behind command
//! methods, so no GUI code holds a GRC::SideStake or calls GetSideStakeRegistry()
//! directly. Mutations return the post-mutation local sidestake revision (design
//! §4.4) for read-your-writes.
class SideStakeManagerImpl : public SideStakeManager
{
public:
    SideStakeSnapshot entries() override
    {
        GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

        SideStakeSnapshot snap;

        // Read the revision BEFORE the entries so an interleaving local mutation
        // only makes it conservatively low (an extra refetch), never stale.
        snap.local_revision = registry.GetLocalSideStakeRevision();

        for (const auto& entry :
             registry.ActiveSideStakeEntries(GRC::SideStake::FilterFlag::ALL, true)) {
            SideStakeEntry row;
            row.address = EncodeDestination(entry->GetDestination());
            row.allocation_percent = entry->GetAllocation().ToPercent();
            row.description = entry->GetDescription();
            row.status = entry->StatusToString();
            row.is_mandatory = entry->IsMandatory();
            row.status_sort_key = StatusSortKey(*entry);

            snap.entries.push_back(std::move(row));
        }

        return snap;
    }

    uint64_t localRevision() override
    {
        return GRC::GetSideStakeRegistry().GetLocalSideStakeRevision();
    }

    SideStakeEditResult addLocal(const std::string& address,
                                 double allocation_percent,
                                 const std::string& description) override
    {
        GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

        const CTxDestination destination = DecodeDestination(address);
        if (!IsValidDestination(destination)) {
            return Result(SideStakeEditStatus::INVALID_ADDRESS);
        }

        // Duplicate local sidestake check against the registry (not a stale view).
        if (!registry.Try(destination, GRC::SideStake::FilterFlag::LOCAL).empty()) {
            return Result(SideStakeEditStatus::DUPLICATE_ADDRESS);
        }

        // Validate the percent before constructing GRC::Allocation, whose
        // double -> int64_t conversion is undefined for NaN/Inf/out-of-range.
        if (!ValidAllocationPercent(allocation_percent)) {
            return Result(SideStakeEditStatus::INVALID_ALLOCATION);
        }

        const GRC::Allocation new_allocation(allocation_percent / 100.0);

        // The new allocation plus all active entries must not exceed 100%.
        if (TotalActiveAllocation(registry, nullptr) + new_allocation > 1) {
            return Result(SideStakeEditStatus::INVALID_ALLOCATION);
        }

        const std::string sanitized = SanitizeString(description, SAFE_CHARS_CSV);
        if (sanitized != description) {
            return Result(SideStakeEditStatus::INVALID_DESCRIPTION);
        }

        registry.NonContractAdd(GRC::LocalSideStake(destination,
                                                    new_allocation,
                                                    sanitized,
                                                    GRC::LocalSideStake::LocalSideStakeStatus::ACTIVE));

        return Result(SideStakeEditStatus::OK, EncodeDestination(destination));
    }

    SideStakeEditResult setAllocation(const std::string& address,
                                      double allocation_percent) override
    {
        GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

        GRC::SideStake original;
        if (!FindLocal(registry, address, original)) {
            return Result(SideStakeEditStatus::INVALID_ADDRESS);
        }

        if (original.GetAllocation().ToPercent() == allocation_percent) {
            return Result(SideStakeEditStatus::NO_CHANGES);
        }

        // Validate the percent before constructing GRC::Allocation (see addLocal).
        if (!ValidAllocationPercent(allocation_percent)) {
            return Result(SideStakeEditStatus::INVALID_ALLOCATION);
        }

        const CTxDestination destination = original.GetDestination();
        const GRC::Allocation new_allocation(allocation_percent / 100.0);

        // The new allocation plus the OTHER active entries must not exceed 100%.
        if (TotalActiveAllocation(registry, &destination) + new_allocation > 1) {
            return Result(SideStakeEditStatus::INVALID_ALLOCATION);
        }

        registry.NonContractAdd(
            GRC::LocalSideStake(destination,
                                new_allocation,
                                original.GetDescription(),
                                std::get<GRC::LocalSideStake::Status>(original.GetStatus()).Value()),
            true);

        return Result(SideStakeEditStatus::OK);
    }

    SideStakeEditResult setDescription(const std::string& address,
                                       const std::string& description) override
    {
        GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

        GRC::SideStake original;
        if (!FindLocal(registry, address, original)) {
            return Result(SideStakeEditStatus::INVALID_ADDRESS);
        }

        if (original.GetDescription() == description) {
            return Result(SideStakeEditStatus::NO_CHANGES);
        }

        const std::string sanitized = SanitizeString(description, SAFE_CHARS_CSV);
        if (sanitized != description) {
            return Result(SideStakeEditStatus::INVALID_DESCRIPTION);
        }

        registry.NonContractAdd(
            GRC::LocalSideStake(original.GetDestination(),
                                original.GetAllocation(),
                                sanitized,
                                std::get<GRC::LocalSideStake::Status>(original.GetStatus()).Value()),
            true);

        return Result(SideStakeEditStatus::OK);
    }

    SideStakeEditResult deleteLocal(const std::string& address) override
    {
        GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

        GRC::SideStake original;
        // Only local entries are deletable; a mandatory address is not found as
        // LOCAL, so this also enforces the GUI's "no deleting mandatory" rule.
        if (!FindLocal(registry, address, original)) {
            return Result(SideStakeEditStatus::INVALID_ADDRESS);
        }

        registry.NonContractDelete(original.GetDestination());

        return Result(SideStakeEditStatus::OK);
    }

    std::unique_ptr<Handler> handleRwSettingsUpdated(RwSettingsUpdatedFn fn) override
    {
        // Forward the payload-free callback directly. Deliberately does NOT read
        // the revision here: this slot may run before the registry's own
        // RwSettingsUpdated reload slot (slot order is not guaranteed), so an
        // emission-time revision could predate the reload. The consumer reads
        // localRevision() itself when handling the notification — see
        // interfaces/sidestake.h.
        return MakeSignalHandler(uiInterface.RwSettingsUpdated_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleMandatorySideStakeChanged(MandatorySideStakeChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.MandatorySideStakeChanged_connect(std::move(fn)));
    }

private:
    //! Build a result stamped with the current local sidestake revision.
    static SideStakeEditResult Result(SideStakeEditStatus status, std::string address = {})
    {
        SideStakeEditResult res;
        res.status = status;
        res.address = std::move(address);
        res.local_revision = GRC::GetSideStakeRegistry().GetLocalSideStakeRevision();
        return res;
    }

    //! A well-formed allocation percentage: finite and within [0, 100]. Checked
    //! before constructing GRC::Allocation, whose double -> int64_t conversion is
    //! undefined behavior for NaN/Inf/out-of-range values (QString::toDouble can
    //! yield inf, e.g. from "1e400").
    static bool ValidAllocationPercent(double percent)
    {
        return std::isfinite(percent) && percent >= 0.0 && percent <= 100.0;
    }

    //! The Status-column sort key: mandatory statuses keep their enum value,
    //! local statuses are shifted above the mandatory range (reproducing the
    //! former SideStakeLessThan ordering).
    static int StatusSortKey(const GRC::SideStake& entry)
    {
        if (entry.IsMandatory()) {
            return static_cast<int>(
                std::get<GRC::MandatorySideStake::Status>(entry.GetStatus()).Value());
        }

        return static_cast<int>(std::get<GRC::LocalSideStake::Status>(entry.GetStatus()).Value())
               + static_cast<int>(GRC::MandatorySideStake::MandatorySideStakeStatus::OUT_OF_BOUND);
    }

    //! Total allocation of the active entries, optionally excluding one
    //! destination (used when re-validating an edit of that entry).
    static GRC::Allocation TotalActiveAllocation(GRC::SideStakeRegistry& registry,
                                                 const CTxDestination* exclude)
    {
        GRC::Allocation total;

        for (const auto& entry :
             registry.ActiveSideStakeEntries(GRC::SideStake::FilterFlag::ALL, true)) {
            if (exclude != nullptr && entry->GetDestination() == *exclude) {
                continue;
            }

            total += entry->GetAllocation();
        }

        return total;
    }

    //! Look up an existing LOCAL sidestake by encoded address. Returns false if
    //! the address is invalid or has no local entry.
    static bool FindLocal(GRC::SideStakeRegistry& registry,
                          const std::string& address,
                          GRC::SideStake& out)
    {
        const CTxDestination destination = DecodeDestination(address);
        if (!IsValidDestination(destination)) {
            return false;
        }

        const auto existing = registry.Try(destination, GRC::SideStake::FilterFlag::LOCAL);
        if (existing.empty()) {
            return false;
        }

        out = *existing.front();
        return true;
    }
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

std::unique_ptr<SideStakeManager> MakeSideStakeManager()
{
    return std::make_unique<SideStakeManagerImpl>();
}

} // namespace interfaces
