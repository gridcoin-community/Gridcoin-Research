// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/staking.h"
#include "interfaces/mrc.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/voting.h"

#include "amount.h"
#include "chainparams.h"
#include "fs.h"
#include "gridcoin/beacon.h"
#include "gridcoin/boinc.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/magnitude.h"
#include "gridcoin/mrc.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/scraper/scraper.h"
#include "gridcoin/sidestake.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/voting/builders.h"
#include "gridcoin/voting/filter.h"
#include "gridcoin/voting/fwd.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/poll_result_cache.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "interfaces/handler.h"
#include "key_io.h"
#include "main.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "txmempool.h"
#include "util/strencodings.h"
#include "util/string.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

extern CWallet* pwalletMain;
extern ConvergedScraperStats ConvergedScraperStatsCache;

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

//! In-process VotingManager implementation. buildPollTable drives the core
//! PollResultCache (tally memoization, pinned-tip consistency and the reorg-retry
//! all live in the cache) and flattens each result into a value row; submitPoll /
//! submitVote reproduce the former VotingModel send paths through the poll/vote
//! builders, choosing the payload version node-side. The GUI keeps the wallet
//! unlock (a modal it drives through the wallet interface) and all presentation
//! — see interfaces/voting.h.
class VotingManagerImpl : public VotingManager
{
public:
    std::vector<PollTableItem> buildPollTable(int filter_flags) override
    {
        std::vector<PollTableItem> table;

        for (const GRC::PollResultItem& src :
             GRC::GetPollResultCache().BuildPollTable(static_cast<GRC::PollFilterFlag>(filter_flags))) {
            table.push_back(ToTableItem(src));
        }

        return table;
    }

    CAmount estimatePollFee() override
    {
        // Matches the former VotingModel::estimatePollFee. TODO: derive a precise
        // fee from the balance-attestation output count rather than a flat value.
        return 50 * COIN;
    }

    std::string currentPollTitle() override
    {
        // Raw title; the GUI applies its cosmetic formatting (length cap, '_' -> ' ').
        return GRC::GetCurrentPollTitle();
    }

    int64_t latestActivePollTime() override
    {
        // The newest active poll's timestamp. This needs only cs_poll_registry:
        // Polls().OnlyActive() filters on GetAdjustedTime() and PollReference::Time()
        // reads a stored timestamp — neither touches cs_main — so it is not taken
        // (avoiding needless cs_main contention). The sequence is built under
        // cs_poll_registry (matching PollResultCache::BuildPollTable's Where()
        // WITH_LOCK); the per-ref reads then run outside the registry lock as in
        // that walk.
        GRC::PollRegistry& registry = GRC::GetPollRegistry();

        int64_t latest = 0;
        for (const auto& iter : WITH_LOCK(registry.cs_poll_registry, return registry.Polls().OnlyActive())) {
            latest = std::max<int64_t>(latest, iter->Ref().Time());
        }

        return latest;
    }

    VotingSubmitResult submitPoll(const PollSubmission& poll) override
    {
        // The payload version (and, pre-v3, the forced SURVEY type) depend on the
        // chain height, so they are resolved node-side rather than in the GUI.
        uint32_t payload_version = 0;
        GRC::PollType type_by_version = GRC::PollType::SURVEY;
        {
            LOCK(cs_main);
            const bool v3_enabled = IsPollV3Enabled(nBestHeight);
            payload_version = v3_enabled ? 3 : 2;
            type_by_version = v3_enabled ? static_cast<GRC::PollType>(poll.type) : GRC::PollType::SURVEY;
        }

        std::vector<GRC::Poll::AdditionalField> additional_fields;
        for (const auto& field : poll.additional_fields) {
            additional_fields.push_back(GRC::Poll::AdditionalField(field.name, field.value, field.required));
        }

        try {
            GRC::PollBuilder builder = GRC::PollBuilder();

            {
                // SetPayloadVersion reads nBestHeight; the other setters do not.
                LOCK(cs_main);
                builder = std::move(builder).SetPayloadVersion(payload_version);
            }

            builder = std::move(builder)
                          .SetType(type_by_version)
                          .SetTitle(poll.title)
                          .SetDuration(poll.duration_days)
                          .SetQuestion(poll.question)
                          .SetWeightType(poll.weight_type)
                          .SetResponseType(poll.response_type)
                          .SetUrl(poll.url)
                          .SetAdditionalFields(additional_fields);

            for (const auto& choice : poll.choices) {
                builder = builder.AddChoice(choice);
            }

            // The wallet must already be unlocked by the caller; SendPollContract
            // signs and broadcasts. A VotingError (or any other failure) is turned
            // into a result rather than thrown across the interface boundary.
            const uint256 txid = GRC::SendPollContract(std::move(builder));
            return Submitted(txid);
        } catch (const std::exception& e) {
            return Failed(e.what());
        }
    }

    VotingSubmitResult submitVote(const std::string& poll_txid,
                                  const std::vector<uint8_t>& choice_offsets) override
    {
        const uint256 txid = uint256S(poll_txid);

        // Look up and read the poll under both locks it needs, then copy out the
        // poll and its txid and release before building/broadcasting the vote:
        //   - cs_poll_registry: TryByTxid reads the registry map, and
        //     TryReadFromDisk mutates the PollReference (m_timestamp,
        //     m_magnitude_weight_factor).
        //   - cs_main: TryReadFromDisk's ResolveMagnitudeWeightFactor walks the
        //     chain index (GetStartingBlockIndexPtr).
        // SendVoteContract takes cs_main + cs_wallet itself, so the build/broadcast
        // must run OUTSIDE these locks (and cs_main is no longer held across it).
        GRC::PollOption poll;
        uint256 poll_reference_txid;
        {
            LOCK2(cs_main, GRC::GetPollRegistry().cs_poll_registry);

            const GRC::PollReference* ref = GRC::GetPollRegistry().TryByTxid(txid);
            if (!ref) {
                return SubmitStatus(VotingSubmitStatus::POLL_NOT_FOUND);
            }

            poll_reference_txid = ref->Txid();
            poll = ref->TryReadFromDisk();
        }

        if (!poll) {
            return SubmitStatus(VotingSubmitStatus::POLL_LOAD_FAILED);
        }

        try {
            GRC::VoteBuilder builder = GRC::VoteBuilder::ForPoll(*poll, poll_reference_txid);
            builder = builder.AddResponses(choice_offsets);

            const uint256 vote_txid = GRC::SendVoteContract(std::move(builder));
            return Submitted(vote_txid);
        } catch (const std::exception& e) {
            return Failed(e.what());
        }
    }

    std::unique_ptr<Handler> handleNewPollReceived(NewPollReceivedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NewPollReceived_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleNewVoteReceived(NewVoteReceivedFn fn) override
    {
        // The core signal carries a uint256; hand the consumer its hex form so no
        // core type crosses the boundary.
        return MakeSignalHandler(uiInterface.NewVoteReceived_connect(
            [fn = std::move(fn)](const uint256& poll_txid) { fn(poll_txid.ToString()); }));
    }

private:
    //! Flatten a core tally bundle into the pointer-free value row the GUI renders.
    //! Mirrors the former VotingModel BuildPollItem field-for-field, but produces
    //! std::string / value types and leaves cosmetic string transforms (underscore
    //! substitution, URL scheme prefixing) to the GUI.
    static PollTableItem ToTableItem(const GRC::PollResultItem& src)
    {
        const GRC::PollResult& result = src.result;
        const GRC::Poll& poll = result.m_poll;

        PollTableItem item;
        item.txid = src.txid.ToString();
        item.payload_version = src.payload_version;
        item.type_str = poll.PollTypeToString();
        item.title = poll.m_title;
        item.question = poll.m_question;
        item.url = poll.m_url;
        item.start_time = poll.m_timestamp;
        item.expiration = poll.Expiration();
        item.duration_days = poll.m_duration_days;
        item.weight_type = poll.m_weight_type.Raw();
        item.weight_type_str = poll.WeightTypeToString();
        item.response_type = poll.ResponseTypeToString();
        item.total_votes = result.m_votes.size();
        item.total_weight = result.m_total_weight / COIN;

        if (result.m_active_vote_weight) {
            item.active_weight = *result.m_active_vote_weight / COIN;
        }

        if (result.m_vote_percent_avw) {
            item.vote_percent_avw = *result.m_vote_percent_avw;
        }

        item.validated = result.m_poll_results_validated;
        item.finished = result.m_finished;
        item.multiple_choice = poll.AllowsMultipleChoices();

        for (size_t i = 0; i < poll.m_additional_fields.size(); ++i) {
            PollAdditionalField field;
            field.name = poll.AdditionalFields().At(i)->m_name;
            field.value = poll.AdditionalFields().At(i)->m_value;
            field.required = poll.AdditionalFields().At(i)->m_required;
            item.additional_fields.push_back(std::move(field));
        }

        for (size_t i = 0; i < result.m_responses.size(); ++i) {
            PollChoiceResult choice;
            choice.label = poll.Choices().At(i)->m_label;
            choice.votes = result.m_responses[i].m_votes;
            choice.weight = result.m_responses[i].m_weight / COIN;
            item.choices.push_back(std::move(choice));
        }

        item.self_voted = result.m_self_voted;
        if (result.m_self_voted) {
            for (const auto& response : result.m_self_vote_detail.m_responses) {
                item.self_vote_responses.push_back(PollSelfVoteResponse{response.first, response.second});
            }
        }

        if (!result.m_votes.empty()) {
            item.top_answer = result.WinnerLabel();
        }

        return item;
    }

    static VotingSubmitResult Submitted(const uint256& txid)
    {
        VotingSubmitResult res;
        res.status = VotingSubmitStatus::OK;
        res.txid = txid.ToString();
        return res;
    }

    //! A categorized failure (GUI maps the status to translated text).
    static VotingSubmitResult SubmitStatus(VotingSubmitStatus status)
    {
        VotingSubmitResult res;
        res.status = status;
        return res;
    }

    //! A failure carrying a dynamic message (e.g. a VotingError) shown as-is.
    static VotingSubmitResult Failed(std::string error)
    {
        VotingSubmitResult res;
        res.status = VotingSubmitStatus::FAILED;
        res.error = std::move(error);
        return res;
    }
};

//! Maps a core BeaconError to the interface BeaconStatus. Ported verbatim from
//! the former GUI ResearcherModel::MapAdvertiseBeaconError so the classification
//! is byte-for-byte identical, now computed node-side.
BeaconStatus MapBeaconError(const GRC::BeaconError error)
{
    switch (error) {
        case GRC::BeaconError::NONE:               return BeaconStatus::ACTIVE;
        case GRC::BeaconError::INSUFFICIENT_FUNDS: return BeaconStatus::ERROR_INSUFFICIENT_FUNDS;
        case GRC::BeaconError::MISSING_KEY:        return BeaconStatus::ERROR_MISSING_KEY;
        case GRC::BeaconError::NO_CPID:            return BeaconStatus::NO_CPID;
        case GRC::BeaconError::NOT_NEEDED:         return BeaconStatus::ERROR_NOT_NEEDED;
        case GRC::BeaconError::PENDING:            return BeaconStatus::PENDING;
        case GRC::BeaconError::TX_FAILED:          return BeaconStatus::ERROR_TX_FAILED;
        case GRC::BeaconError::V14_NOT_ENABLED:    return BeaconStatus::ERROR_TX_FAILED;
        case GRC::BeaconError::WALLET_LOCKED:      return BeaconStatus::ERROR_WALLET_LOCKED;
        case GRC::BeaconError::ALEADY_IN_MEMPOOL:  return BeaconStatus::ALREADY_IN_MEMPOOL;
    }

    assert(false); // unreachable: every BeaconError enumerator is handled above
    return BeaconStatus::UNKNOWN;
}

//! Beacon-renewal warning window (mirrors the former GUI ResearcherModel constant
//! BEACON_RENEWAL_WARNING_THRESHOLD): a beacon expiring within 15 days reads as
//! RENEWAL_NEEDED.
constexpr int64_t BEACON_RENEWAL_WARNING_THRESHOLD = 15 * 24 * 60 * 60;

//! Classify a whitelist entry's status into a ResearcherProjectRow::WhitelistStatus,
//! matching the former buildProjectTable's if/else chain. \p is_label_status is
//! set true for the greylisted/excluded cases where the status itself is the row's
//! error label (so the caller clears any baseline error, letting the GUI derive the
//! label from the status).
ResearcherProjectRow::WhitelistStatus ClassifyWhitelist(const GRC::ProjectEntry& wl,
                                                        const std::vector<std::string>& excluded,
                                                        bool& is_label_status)
{
    using WS = ResearcherProjectRow::WhitelistStatus;

    is_label_status = true;

    if (wl.m_status == GRC::ProjectEntryStatus::MAN_GREYLISTED) {
        return WS::MANUALLY_GREYLISTED;
    }
    if (wl.m_status == GRC::ProjectEntryStatus::AUTO_GREYLISTED) {
        return WS::AUTOMATICALLY_GREYLISTED;
    }
    if (std::find(excluded.begin(), excluded.end(), wl.m_name) != excluded.end()) {
        return WS::EXCLUDED;
    }

    is_label_status = false;

    if (wl.m_status == GRC::ProjectEntryStatus::UNKNOWN) {
        return WS::NOT_WHITELISTED;
    }

    // Remaining REG_ACTIVE / AUTO_GREYLIST_OVERRIDE == the ACTIVE whitelist filter.
    return WS::WHITELISTED;
}

//! Copy a whitelist entry's magnitude/RAC into the row from the ExplainMagnitude
//! results (matches the inner lookup loops in the former buildProjectTable).
void ApplyExplainMagnitude(ResearcherProjectRow& row,
                           const std::string& project_name,
                           const std::vector<GRC::ExplainMagnitudeProject>& explain_mag)
{
    for (const auto& explain_mag_project : explain_mag) {
        if (explain_mag_project.m_name == project_name) {
            row.magnitude = explain_mag_project.m_magnitude;
            row.rac = explain_mag_project.m_rac;
            return;
        }
    }
}

//! In-process ResearcherContext (Phase 1d-iv). Owns the node's single wallet
//! pointer for the beacon-key/advertise/V3 paths; every read runs node-side so the
//! Qt ResearcherModel holds no GRC::Researcher / GRC::Beacon and touches no beacon
//! registry, quorum, whitelist, or scraper-cache global directly.
class ResearcherContextImpl : public ResearcherContext
{
public:
    explicit ResearcherContextImpl(CWallet* wallet) : m_wallet(wallet) {}

    ResearcherSnapshot snapshot() override
    {
        LOCK(cs_main);
        return BuildSnapshotLocked();
    }

    std::optional<ResearcherSnapshot> trySnapshot() override
    {
        TRY_LOCK(cs_main, locked);
        if (!locked) {
            return std::nullopt;
        }
        return BuildSnapshotLocked();
    }

    bool outOfSync() override
    {
        // Lock-free, matching the former ResearcherModel::refresh()'s unguarded
        // OutOfSyncByAge() call.
        return OutOfSyncByAge();
    }

    bool hasV3CapableProjects() override
    {
        return !GetProjectsWithOwnershipProofSupport().empty();
    }

    std::vector<ResearcherProjectRow> projects(bool extended) override
    {
        // Fuses the same three loosely-related record types the former
        // ResearcherModel::buildProjectTable did: local BOINC projects, the
        // Gridcoin whitelist, and scraper magnitude statistics.
        const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

        // One whitelist snapshot, reused for both TryWhitelist() and the
        // whitelist-only pass below (the former GUI took two snapshots; one is
        // more internally consistent and cheaper).
        const GRC::WhitelistSnapshot whitelist =
            GRC::GetWhitelist().Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED);

        std::vector<std::string> excluded_projects;
        {
            LOCK(cs_ConvergedScraperStatsCache);
            excluded_projects = ConvergedScraperStatsCache.Convergence.vExcludedProjects;
        }

        const std::vector<std::string> external_adapter_projects = GetProjectsExternalAdapterRequired();

        std::vector<GRC::ExplainMagnitudeProject> explain_mag;
        if (extended) {
            if (const GRC::CpidOption cpid = researcher->Id().TryCpid()) {
                explain_mag = GRC::Quorum::ExplainMagnitude(*cpid);
            }
        }

        std::map<std::string, ResearcherProjectRow> rows;

        // Pass 1: local BOINC projects, linked to the whitelist where possible.
        for (const auto& project_pair : researcher->Projects()) {
            const GRC::MiningProject& project = project_pair.second;

            ResearcherProjectRow row;

            if (!project.m_cpid.IsZero()) {
                row.cpid = project.m_cpid.ToString();
            }

            const bool eligible = project.Eligible();
            if (!eligible) {
                row.error_kind = ResearcherProjectRow::ErrorKind::CORE_MESSAGE;
                row.error_message = project.ErrorMessage();
            }

            if (const GRC::ProjectEntry* whitelist_project = project.TryWhitelist(whitelist)) {
                bool is_label_status = false;
                row.whitelisted = ClassifyWhitelist(*whitelist_project, excluded_projects, is_label_status);

                // Greylisted/excluded: the status IS the label, so drop the
                // baseline core error and let the GUI render the status label.
                if (is_label_status) {
                    row.error_kind = ResearcherProjectRow::ErrorKind::NONE;
                    row.error_message.clear();
                }

                // Display name in original case; the GUI lowercases it with
                // QString::toLower() (Unicode-aware, as the former code did).
                row.name = whitelist_project->DisplayName();
                ApplyExplainMagnitude(row, whitelist_project->m_name, explain_mag);
                row.gdpr_controls = whitelist_project->HasGDPRControls();

                rows.emplace(whitelist_project->m_name, std::move(row));
            } else {
                row.whitelisted = ResearcherProjectRow::WhitelistStatus::NOT_WHITELISTED;
                row.name = project.m_name;
                row.rac = project.m_rac;

                if (eligible) {
                    row.error_kind = ResearcherProjectRow::ErrorKind::NOT_WHITELISTED;
                    row.error_message.clear();
                }

                rows.emplace(project.m_name, std::move(row));
            }
        }

        // Pass 2: whitelisted projects not detected from the local BOINC client.
        for (const auto& project : whitelist) {
            if (rows.find(project.m_name) != rows.end()) {
                continue;
            }

            ResearcherProjectRow row;
            row.gdpr_controls = project.HasGDPRControls();
            row.name = project.DisplayName();
            row.magnitude = 0.0;

            const bool requires_external_adapter =
                project.RequiresExtAdapter().has_value() && project.RequiresExtAdapter().value();
            const bool legacy_external_adapter = std::find(external_adapter_projects.begin(),
                                                           external_adapter_projects.end(),
                                                           project.m_name) != external_adapter_projects.end();

            if (requires_external_adapter || legacy_external_adapter) {
                row.error_kind = ResearcherProjectRow::ErrorKind::USES_EXTERNAL_ADAPTER;
            } else {
                row.error_kind = ResearcherProjectRow::ErrorKind::NOT_ATTACHED;
            }

            bool is_label_status = false;
            row.whitelisted = ClassifyWhitelist(project, excluded_projects, is_label_status);
            if (is_label_status) {
                row.error_kind = ResearcherProjectRow::ErrorKind::NONE;
            }

            ApplyExplainMagnitude(row, project.m_name, explain_mag);

            rows.emplace(project.m_name, std::move(row));
        }

        std::vector<ResearcherProjectRow> rows_out;
        rows_out.reserve(rows.size());
        for (auto& row_pair : rows) {
            rows_out.emplace_back(std::move(row_pair.second));
        }

        return rows_out;
    }

    std::vector<WhitelistProject> whitelistProjects() override
    {
        // Matches the former VotingModel::getActiveProjectNames/getActiveProjectUrls
        // exactly: the ACTIVE-filter snapshot, Sorted(), raw m_name/m_url (not the
        // Display* variants the researcher table uses). A single call now backs both
        // of the wizard's parallel name/url lists, so their indices still align.
        std::vector<WhitelistProject> result;

        for (const auto& project : GRC::GetWhitelist().Snapshot().Sorted()) {
            result.push_back({project.m_name, project.m_url});
        }

        return result;
    }

    int maxProjectNameLength() override { return static_cast<int>(GRC::Project::MAX_NAME_SIZE); }

    int maxProjectUrlLength() override { return static_cast<int>(GRC::Project::MAX_URL_SIZE); }

    std::vector<WhitelistProject> v3CapableProjects() override
    {
        // Ports ResearcherModel::buildV3ProjectList: the whitelist entries whose
        // (trailing-slash-normalized) base URL is in the ownership-proof set.
        const std::set<std::string> v3_urls = GetProjectsWithOwnershipProofSupport();

        std::vector<WhitelistProject> result;
        if (v3_urls.empty()) {
            return result;
        }

        for (const auto& project : GRC::GetWhitelist().Snapshot(
                 GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED)) {
            std::string base_url = project.BaseUrl();
            if (!base_url.empty() && base_url.back() != '/') {
                base_url += '/';
            }

            if (v3_urls.count(base_url) > 0) {
                result.push_back({project.DisplayName(), project.DisplayUrl()});
            }
        }

        return result;
    }

    bool switchMode(ResearcherMode mode, const std::string& email) override
    {
        const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

        switch (mode) {
            case ResearcherMode::SOLO:
                return researcher->ChangeMode(GRC::ResearcherMode::SOLO, email);
            case ResearcherMode::POOL:
                return researcher->ChangeMode(GRC::ResearcherMode::POOL, std::string());
            case ResearcherMode::NONCRUNCHER:
                return researcher->ChangeMode(GRC::ResearcherMode::NONCRUNCHER, std::string());
        }

        return false;
    }

    BeaconAdvertiseResult advertiseBeacon() override
    {
        const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
        const GRC::AdvertiseBeaconResult result = researcher->AdvertiseBeacon();

        return {MapBeaconError(result.Error())};
    }

    std::string generateBeaconKeyForV3() override
    {
        const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
        const GRC::CpidOption cpid = researcher->Id().TryCpid();

        // The wizard flow requires a CPID before the beacon page, so this is not
        // reachable in practice; defensive only.
        if (!cpid) {
            return std::string();
        }

        // pwalletMain is the node's single wallet (== m_wallet); referencing it
        // directly matches SendBeaconContractV3/GenerateBeaconKey's
        // EXCLUSIVE_LOCKS_REQUIRED(cs_main, pwalletMain->cs_wallet) annotation.
        // Enforce the single-wallet invariant this relies on.
        assert(pwalletMain && pwalletMain == m_wallet);
        LOCK2(cs_main, pwalletMain->cs_wallet);

        GRC::AdvertiseBeaconResult result = GRC::GenerateBeaconKey(*cpid);

        if (auto public_key = result.TryPublicKey()) {
            return HexStr(*public_key);
        }

        return std::string();
    }

    BeaconAdvertiseResult advertiseBeaconV3(const std::string& ownership_proof_xml) override
    {
        const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
        const GRC::CpidOption cpid = researcher->Id().TryCpid();

        if (!cpid) {
            return {BeaconStatus::NO_CPID};
        }

        const std::string master_url = TrimString(ExtractXML(ownership_proof_xml, "<master_url>", "</master_url>"));
        const std::string msg = TrimString(ExtractXML(ownership_proof_xml, "<msg>", "</msg>"));
        const std::string sig_b64 = TrimString(ExtractXML(ownership_proof_xml, "<signature>", "</signature>"));

        if (master_url.empty() || msg.empty() || sig_b64.empty()) {
            return {BeaconStatus::ERROR_INVALID_PROOF_XML};
        }

        // Parse the msg field: "{account_id} {beacon_public_key_hex}".
        const size_t space_pos = msg.find(' ');
        if (space_pos == std::string::npos || space_pos + 1 >= msg.size()) {
            return {BeaconStatus::ERROR_INVALID_PROOF_XML};
        }

        uint32_t account_id = 0;
        if (!ParseUInt32(msg.substr(0, space_pos), &account_id) || account_id == 0) {
            return {BeaconStatus::ERROR_INVALID_PROOF_XML};
        }

        const std::string public_key_hex = msg.substr(space_pos + 1);
        const std::vector<uint8_t> pubkey_bytes = ParseHex(public_key_hex);
        CPubKey beacon_pubkey(pubkey_bytes);

        if (!beacon_pubkey.IsValid()) {
            return {BeaconStatus::ERROR_INVALID_PROOF_XML};
        }

        // pwalletMain (== the node's single wallet m_wallet) is used to satisfy
        // SendBeaconContractV3's EXCLUSIVE_LOCKS_REQUIRED(pwalletMain->cs_wallet)
        // annotation; enforce that single-wallet invariant.
        assert(pwalletMain && pwalletMain == m_wallet);
        LOCK2(cs_main, pwalletMain->cs_wallet);

        if (!pwalletMain->HaveKey(beacon_pubkey.GetID())) {
            return {BeaconStatus::ERROR_MISSING_KEY};
        }

        bool b64_invalid = false;
        const std::vector<uint8_t> rsa_sig_bytes = DecodeBase64(sig_b64.c_str(), &b64_invalid);

        if (b64_invalid || rsa_sig_bytes.empty()) {
            return {BeaconStatus::ERROR_INVALID_PROOF_XML};
        }

        GRC::Beacon beacon(beacon_pubkey);
        GRC::OwnershipProof proof;
        proof.m_master_url = master_url;
        proof.m_account_id = account_id;
        proof.m_rsa_signature = rsa_sig_bytes;

        GRC::AdvertiseBeaconResult result = GRC::SendBeaconContractV3(*cpid, beacon, std::move(proof));

        return {MapBeaconError(result.Error())};
    }

    void reload() override
    {
        LOCK(cs_main);
        GRC::Researcher::Reload();
    }

    std::unique_ptr<Handler> handleResearcherChanged(ResearcherChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.ResearcherChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleBeaconChanged(BeaconChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.BeaconChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleAccrualChanged(AccrualChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.AccrualChangedFromStakeOrMRC_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleBlocksChanged(BlocksChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyBlocksChanged_connect(std::move(fn)));
    }

private:
    CWallet* m_wallet;

    //! Build the full snapshot with cs_main held. Shared by snapshot() (blocking)
    //! and trySnapshot() (TRY_LOCK). The whole snapshot is built under one cs_main
    //! hold so it reflects a single consistent tip (identity/magnitude reads off
    //! the immutable Researcher::Get() pointer do not strictly need cs_main, but
    //! holding it is harmless and keeps the beacon/version reads consistent).
    ResearcherSnapshot BuildSnapshotLocked() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        ResearcherSnapshot snap;

        const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

        const GRC::MiningId id = researcher->Id();
        snap.has_cpid = id.Which() == GRC::MiningId::Kind::CPID;
        snap.has_eligible_projects = snap.has_cpid;
        snap.cpid = id.ToString();
        snap.has_split_cpid = researcher->hasSplitCpid();
        snap.has_rac = researcher->HasRAC();
        snap.has_pool_projects = researcher->Projects().ContainsPool();
        snap.detected_pool_mode = !snap.has_eligible_projects && snap.has_pool_projects;
        snap.email = GRC::Researcher::Email();
        snap.configured_for_noncruncher_mode = GRC::Researcher::ConfiguredForNoncruncherMode();

        const GRC::Magnitude magnitude = researcher->Magnitude();
        snap.magnitude = magnitude.Floating();
        snap.magnitude_text = magnitude.ToString();
        snap.has_magnitude = magnitude != 0;
        snap.accrual = researcher->Accrual();
        snap.accrual_near_limit = researcher->AccrualNearLimit();

        snap.out_of_sync = OutOfSyncByAge();
        snap.is_v14_enabled = IsV14Enabled(nBestHeight);

        // msMiningErrors is shared with the getstakinginfo RPC; read it under its
        // own lock. cs_msMiningErrors is a leaf everywhere it is taken
        // (StoreResearcher, getstakinginfo, and here all acquire it alone and
        // release without acquiring cs_main under it), so nesting it beneath
        // cs_main here introduces no lock-order inversion.
        {
            LOCK(cs_msMiningErrors);
            snap.mining_status = msMiningErrors;
        }

        DeriveBeacon(*researcher, snap);
        snap.action_needed = ComputeActionNeeded(snap);
        // UTF-8 for user-facing display: fsbridge::LongPathString is the in-tree
        // helper documented "for error messages, log output, and any user-facing
        // display" (it restores the long Unicode form on Windows before UTF-8
        // encoding), so the GUI's QString::fromStdString() renders non-ASCII paths
        // correctly cross-platform. Matches the datadir display in bitcoin.cpp.
        snap.boinc_data_dir = fsbridge::LongPathString(GRC::GetBoincDataDir());

        return snap;
    }

    //! Ports ResearcherModel::updateBeacon(): derive the beacon status and flatten
    //! the committed active/pending beacons into the snapshot's value fields.
    void DeriveBeacon(const GRC::Researcher& researcher, ResearcherSnapshot& snap)
    {
        const GRC::CpidOption cpid = researcher.Id().TryCpid();

        if (!cpid) {
            snap.beacon_status = BeaconStatus::NO_CPID;
            return;
        }

        if (snap.out_of_sync) {
            snap.beacon_status = BeaconStatus::UNKNOWN;
            return;
        }

        bool beacon_key_present = false;
        std::unique_ptr<GRC::Beacon> beacon;
        std::unique_ptr<GRC::Beacon> pending_beacon;

        if (auto beacon_option = researcher.TryBeacon()) {
            beacon = std::make_unique<GRC::Beacon>(std::move(*beacon_option));
            beacon_key_present = beacon->WalletHasPrivateKey(m_wallet);
        }

        if (auto beacon_option = researcher.TryPendingBeacon()) {
            pending_beacon = std::make_unique<GRC::Beacon>(std::move(*beacon_option));
            beacon_key_present = pending_beacon->WalletHasPrivateKey(m_wallet);
        }

        BeaconStatus beacon_status;

        if (beacon_key_present) {
            beacon_status = MapBeaconError(researcher.BeaconError());
        } else if (!beacon && !pending_beacon) {
            beacon_status = BeaconStatus::NO_BEACON;
        } else {
            beacon_status = BeaconStatus::ERROR_MISSING_KEY;
        }

        const int64_t now = GetAdjustedTime();

        if (beacon_status != BeaconStatus::ACTIVE) {
            // Keep the mapped/derived status.
        } else if (pending_beacon) {
            beacon_status = BeaconStatus::PENDING;
        } else if (beacon) {
            if (beacon->Expired(now + BEACON_RENEWAL_WARNING_THRESHOLD)) {
                beacon_status = BeaconStatus::RENEWAL_NEEDED;
            } else if (beacon->Renewable(now)) {
                beacon_status = BeaconStatus::RENEWAL_POSSIBLE;
            } else if (researcher.Magnitude() == 0) {
                beacon_status = BeaconStatus::NO_MAGNITUDE;
            } else {
                beacon_status = BeaconStatus::ACTIVE;
            }
        }

        snap.beacon_status = beacon_status;

        FillBeaconFields(beacon.get(), pending_beacon.get(), now, snap);
    }

    //! Flatten the committed active/pending beacons into the snapshot's value
    //! fields — the booleans the former ResearcherModel getters returned plus the
    //! epochs (Unix seconds) the GUI formats locally.
    static void FillBeaconFields(const GRC::Beacon* beacon,
                                 const GRC::Beacon* pending,
                                 int64_t now,
                                 ResearcherSnapshot& snap)
    {
        snap.beacon_present = beacon != nullptr;
        snap.pending_beacon_present = pending != nullptr;
        snap.has_active_beacon = beacon && !beacon->Expired(now);
        snap.beacon_expired = beacon && beacon->Expired(now);
        snap.has_renewable_beacon = beacon && beacon->Renewable(now);

        bool has_pending = false;
        if (pending) {
            GRC::PendingBeacon pending_beacon(*pending);
            has_pending = !pending_beacon.PendingExpired(now);
        }
        snap.has_pending_beacon = has_pending;

        if (!has_pending) {
            snap.needs_beacon_auth = false;
        } else if (!snap.has_active_beacon) {
            snap.needs_beacon_auth = true;
        } else {
            snap.needs_beacon_auth = beacon->m_public_key != pending->m_public_key;
        }

        if (beacon) {
            snap.beacon_timestamp = beacon->m_timestamp;
            snap.beacon_age = beacon->Age(now);
            snap.time_to_beacon_expiration = GRC::Beacon::MAX_AGE - beacon->Age(now);
            snap.beacon_address = EncodeDestination(beacon->GetAddress());
        }

        if (pending) {
            snap.time_to_pending_beacon_expiration = GRC::PendingBeacon::RETENTION_AGE - pending->Age(now);
            snap.beacon_verification_code = pending->GetVerificationCode();
        }
    }

    //! Ports ResearcherModel::actionNeeded() over the already-filled snapshot.
    static bool ComputeActionNeeded(const ResearcherSnapshot& snap)
    {
        if (snap.out_of_sync) {
            return false;
        }
        if (snap.configured_for_noncruncher_mode) {
            return false;
        }
        if (snap.has_eligible_projects) {
            return snap.has_split_cpid || (!snap.has_active_beacon && !snap.has_pending_beacon);
        }
        return !snap.has_pool_projects;
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

std::unique_ptr<VotingManager> MakeVotingManager()
{
    return std::make_unique<VotingManagerImpl>();
}

std::unique_ptr<ResearcherContext> MakeResearcherContext(CWallet* wallet)
{
    return std::make_unique<ResearcherContextImpl>(wallet);
}

} // namespace interfaces
