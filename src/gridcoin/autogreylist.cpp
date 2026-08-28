// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <gridcoin/autogreylist.h>

#include "chain.h"
#include "chainparams.h"
#include "gridcoin/quorum.h"
#include "gridcoin/support/block_finder.h"

using namespace GRC;

namespace {
//!
//! \brief The authoritative key: the committed superblock's (quorum) hash as a uint256.
//! v3+ superblocks always carry a SHA256-kind quorum hash; anything else keys as null,
//! which is harmless -- the key is an identity label, and freshness is push-driven.
//!
uint256 AuthoritativeKey(const SuperblockPtr& source)
{
    const QuorumHash quorum_hash = source->GetHash();

    if (quorum_hash.Which() != QuorumHash::Kind::SHA256) {
        return uint256();
    }

    return uint256(std::vector<unsigned char>(quorum_hash.Raw(), quorum_hash.Raw() + 32));
}

//!
//! \brief Build a computation from a superblock's serialized m_project_status record.
//!
//! A pure map read: membership is exactly the AUTO_GREYLISTED entries. MAN_GREYLISTED
//! entries in the record are redundant with the registry (which remains the source for
//! manual and override status at overlay time) and absence means "not auto greylisted" --
//! the record's write rule omits ACTIVE entries. No candidate detail: the record does not
//! carry it, which m_from_record signals to consumers.
//!
GreylistComputation ComputationFromRecord(const SuperblockPtr& source)
{
    GreylistComputation result;
    result.m_version = GreylistVersion::V2;
    result.m_key = AuthoritativeKey(source);
    result.m_from_record = true;

    for (const auto& iter : source->m_project_status.m_project_status) {
        if (iter.second.Value() == ProjectEntryStatus::AUTO_GREYLISTED) {
            result.m_auto_greylisted.insert(iter.first);
        }
    }

    return result;
}

//!
//! \brief Build a computation from a V2 walker result.
//!
GreylistComputation ComputationFromWalk(AutoGreylistV2::Result result, const uint256& key)
{
    GreylistComputation computation;
    computation.m_version = GreylistVersion::V2;
    computation.m_key = key;
    computation.m_from_record = false;
    computation.m_auto_greylisted = std::move(result.m_auto_greylisted);
    computation.m_candidates = std::move(result.m_candidates);

    return computation;
}
} // anonymous namespace

AutoGreylistService::AutoGreylistService()
    : m_v1(std::make_shared<AutoGreylist>())
    , m_authoritative_source()
    , m_have_authoritative_source(false)
    , m_authoritative(std::nullopt)
    , m_pending(std::nullopt)
{
}

// ---------------------------------------------------------------------------- producers ----

void AutoGreylistService::Refresh() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Version dispatch, from the anchor height at write time (the producer holds cs_main and
    // the anchor; readers never re-derive it). The anchor for the chain-handler refresh is
    // the current committed superblock.
    SuperblockPtr superblock_ptr = Quorum::CurrentSuperblock();

    if (!superblock_ptr.IsEmpty() && IsAutoGreylistRedesignEnabled(superblock_ptr.m_height)) {
        RefreshWithSuperblock(superblock_ptr);
        return;
    }

    // V1 producer write. Clear the V2 slots FIRST (scoped -- see the class comment: the
    // service lock must not be held across V1 refresh calls, which re-enter
    // Whitelist::Snapshot and take cs_lock). This arm also covers a reorg back across the
    // gate: the V1 write clears the V2 slots, and no state migrates in either direction.
    ClearV2Slots();

    m_v1->Refresh();
}

void AutoGreylistService::RefreshWithSuperblock(
    SuperblockPtr superblock_ptr_in,
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!superblock_ptr_in.IsEmpty() && superblock_ptr_in->m_version >= 3
        && IsAutoGreylistRedesignEnabled(superblock_ptr_in.m_height)) {
        // V2 producer write, authoritative anchor: the committed superblock's serialized
        // record IS the answer (DESIGN.md section 10.1) -- store the owning pointer and let
        // the first read derive the membership map lazily. No walk, no whitelist dependency:
        // this works during startup before the contract registry loads, which retires the
        // "0 greylisted after restart" defect class.
        //
        // The PENDING slot is cleared as well: its computation walked the committed
        // superblock set behind the tip, and this write means that set just changed (a
        // superblock was pushed, popped, or the index reloaded). Convergence identity alone
        // cannot see a chain change, so reusing across one would serve a greylist derived
        // from a different chain -- while a node that computed fresh (restart, late
        // convergence receipt) would disagree: a real divergence vector. Non-superblock
        // blocks cannot change the walk (it reads only committed superblocks), so these
        // chain-handler writes are exactly the right pending invalidation points. The next
        // candidate derivation recomputes against the new chain -- one walk per superblock
        // transition, after which reuse resumes within the convergence cycle.
        (void)unit_test_blocks; // The record read requires no chain traversal.

        LOCK(m_service_lock);

        m_authoritative_source = superblock_ptr_in;
        m_have_authoritative_source = true;
        m_authoritative = std::nullopt;
        m_pending = std::nullopt;

        return;
    }

    // V1 producer write.
    ClearV2Slots();

    m_v1->RefreshWithSuperblock(superblock_ptr_in, unit_test_blocks);
}

void AutoGreylistService::RefreshWithAndUpdateSuperblock(
    Superblock& superblock, const uint256& convergence_id, bool update_pending_cache,
    const CBlockIndex* anchor_index,
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks)
{
    // Version dispatch: the pending anchor is the chain tip the candidate is bound to,
    // captured by the caller under cs_main and passed in. Reading immutable fields of a
    // never-deleted block index entry needs no lock here.
    const int anchor_height = anchor_index != nullptr ? anchor_index->nHeight : 0;

    if (anchor_index != nullptr && IsAutoGreylistRedesignEnabled(anchor_height)) {
        // V2 producer write, pending anchor.
        //
        // Reuse: the pending state is keyed by convergence identity, and the walker is
        // deterministic given (chain, candidate) -- so a candidate re-derived from the SAME
        // convergence reuses the cached computation instead of walking again. This is the
        // workload win of the separation: validation's cached-contract path and repeated
        // convergence reads stop paying a 40-superblock walk each.
        std::optional<GreylistComputation> cached;

        {
            LOCK(m_service_lock);

            if (m_pending && m_pending->m_key == convergence_id) {
                cached = m_pending;
            }
        }

        // One snapshot serves both the walker (on a cache miss) and the record derivation
        // below: identical arguments, and nothing between the uses can mutate the registry
        // (the caller holds cs_main).
        const WhitelistSnapshot whitelist_snapshot = GetWhitelist().Snapshot(
            GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED);

        if (!cached) {
            // Compute fresh: bind the candidate to the captured tip, exactly as the V1 path
            // binds to pindexBest.
            SuperblockPtr superblock_ptr;
            superblock_ptr.Replace(superblock);
            superblock_ptr.Rebind(anchor_index);

            AutoGreylistV2::Result result = AutoGreylistV2::Compute(
                superblock_ptr,
                whitelist_snapshot,
                GetWhitelist().GetProjectsFirstActive(),
                unit_test_blocks, anchor_index->pprev);

            cached = ComputationFromWalk(std::move(result), convergence_id);
        }

        // Stamp the candidate's record through the ONE record rule (shared with the
        // acceptance-time validator so producer and checker cannot drift).
        {
            AutoGreylistV2::Result stamp_input;
            stamp_input.m_auto_greylisted = cached->m_auto_greylisted;

            superblock.m_project_status = AutoGreylistV2::DeriveProjectStatusRecord(
                stamp_input, whitelist_snapshot);
        }

        if (update_pending_cache) {
            LOCK(m_service_lock);

            m_pending = cached;
        }

        return;
    }

    // V1 producer write: the frozen behavior, including its internal cache-invalidation
    // reset. The convergence identity and pending election are V2-arm concepts.
    (void)convergence_id;
    (void)update_pending_cache;
    (void)unit_test_blocks;

    ClearV2Slots();

    m_v1->RefreshWithAndUpdateSuperblock(superblock);
}

void AutoGreylistService::StampProjectStatus(
    Superblock& superblock, int anchor_height, int64_t anchor_time, CBlockIndex* walk_start,
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks)
{
    // No cs_main requirement of its own: every chain-derived input (anchor height/time and
    // the walk start) is captured by the caller -- the miner holds cs_main for its own block
    // assembly -- and the walk reads only immutable ancestor state.
    if (!IsAutoGreylistRedesignEnabled(anchor_height)) {
        return;
    }

    // Bind the candidate to the block being created. There is no CBlockIndex for it yet, so
    // the binding context is set directly; the validator reconstructs the identical context
    // from the containing block's index entry once the block exists.
    SuperblockPtr superblock_ptr;
    superblock_ptr.Replace(superblock);
    superblock_ptr.m_height = anchor_height;
    superblock_ptr.m_timestamp = anchor_time;

    const WhitelistSnapshot whitelist_snapshot = GetWhitelist().Snapshot(
        GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED);

    const AutoGreylistV2::Result result = AutoGreylistV2::Compute(
        superblock_ptr,
        whitelist_snapshot,
        GetWhitelist().GetProjectsFirstActive(),
        unit_test_blocks, walk_start);

    superblock.m_project_status = AutoGreylistV2::DeriveProjectStatusRecord(result, whitelist_snapshot);
}

bool AutoGreylistService::ValidateProjectStatus(
    const SuperblockPtr& superblock_ptr, CBlockIndex* walk_start,
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks) const
{
    // No cs_main requirement of its own: the received superblock is already bound to its
    // containing block and the walk start is that block's pprev, both supplied by the
    // caller (ConnectBlock, which holds cs_main for its own reasons); the walk reads only
    // immutable ancestor state, and the registry inputs are cs_lock-guarded snapshots.
    // The check applies only where the record is read back as authoritative: above the gate,
    // v3+ superblocks. Below the gate the record stays advisory, exactly as it is today.
    if (superblock_ptr->m_version < 3
        || !IsAutoGreylistRedesignEnabled(superblock_ptr.m_height)) {
        return true;
    }

    // One snapshot for the walker and the record derivation: this runs on the validation
    // thread for every accepted v11+ block above the gate, so the duplicate cs_lock
    // acquisition and ProjectList allocation are worth removing.
    const WhitelistSnapshot whitelist_snapshot = GetWhitelist().Snapshot(
        GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED);

    const AutoGreylistV2::Result result = AutoGreylistV2::Compute(
        superblock_ptr,
        whitelist_snapshot,
        GetWhitelist().GetProjectsFirstActive(),
        unit_test_blocks, walk_start);

    const Superblock::ProjectStatus expected = AutoGreylistV2::DeriveProjectStatusRecord(
        result, whitelist_snapshot);

    if (expected.m_project_status != superblock_ptr->m_project_status.m_project_status) {
        error("%s: project status record mismatch: expected %u entries, received %u.",
              __func__,
              (unsigned int) expected.m_project_status.size(),
              (unsigned int) superblock_ptr->m_project_status.m_project_status.size());

        return false;
    }

    return true;
}

void AutoGreylistService::Reset()
{
    ClearV2Slots();

    m_v1->Reset();
}

void AutoGreylistService::ClearV2Slots()
{
    LOCK(m_service_lock);

    m_authoritative_source = SuperblockPtr::Empty();
    m_have_authoritative_source = false;
    m_authoritative = std::nullopt;
    m_pending = std::nullopt;
}

void AutoGreylistService::PrimeAuthoritativeLocked() const
{
    if (m_authoritative || !m_have_authoritative_source) {
        return;
    }

    m_authoritative = ComputationFromRecord(m_authoritative_source);
}

// ---------------------------------------------------------------------------- consumers ----

bool AutoGreylistService::Contains(GreylistState state, const std::string& name,
                                   const bool& only_auto_greylisted) const
{
    if (state == GreylistState::NONE) {
        return false;
    }

    LOCK(m_service_lock);

    PrimeAuthoritativeLocked();

    // Pending serves its own slot when filled, and otherwise the authoritative state -- the
    // base case: absent a convergence, pending equals authoritative (DESIGN.md section 10.7).
    const std::optional<GreylistComputation>& slot =
        (state == GreylistState::PENDING && m_pending) ? m_pending : m_authoritative;

    if (slot) {
        if (only_auto_greylisted || slot->m_from_record) {
            // Membership: the criteria-meeting set. A record-derived result can only answer
            // this question -- the record carries no non-greylisted candidate entries.
            return slot->m_auto_greylisted.count(name) > 0;
        }

        // "Has any candidate entry" (V1's only_auto_greylisted == false semantics), from the
        // walker's candidate detail.
        return slot->m_candidates.count(name) > 0;
    }

    // V1 fall-through: cs_lock -> m_service_lock -> autogreylist_lock (leafward -- safe).
    return m_v1->Contains(name, only_auto_greylisted);
}

bool AutoGreylistService::IsDeepCopyActive(GreylistState state) const
{
    LOCK(m_service_lock);

    PrimeAuthoritativeLocked();

    const bool v2_backed = (state == GreylistState::PENDING && m_pending) || m_authoritative;

    if (v2_backed) {
        // V2 only runs above the redesign gate, and the deep-copy gate is never above it
        // (the ordering is enforced at startup), so a V2-backed result always deep-copies.
        return true;
    }

    return m_v1->IsDeepCopyActive();
}

std::optional<GreylistComputation> AutoGreylistService::Get(GreylistState state) const
{
    if (state == GreylistState::NONE) {
        return std::nullopt;
    }

    {
        LOCK(m_service_lock);

        PrimeAuthoritativeLocked();

        if (state == GreylistState::PENDING && m_pending) {
            return m_pending;
        }

        if (m_authoritative) {
            // Serves both the AUTHORITATIVE selector and the PENDING base case: absent a
            // convergence, pending equals authoritative.
            return m_authoritative;
        }
    }

    // V1 fall-through: synthesize a computation so the caller sees one result shape either
    // side of the gate. No key -- V1's cache does not expose one, and below the gate no
    // consumer needs it.
    //
    // Deliberately NOT built by iterating m_v1->begin()/end(): V1's iterators are handed out
    // with autogreylist_lock already released, so iterating them from a consumer that (by
    // this API's contract) holds no cs_main could race a producer refresh and dereference
    // invalidated iterators. Membership is instead derived per project through
    // V1::Contains(), which takes the V1 lock internally on every call, over the whitelist
    // names -- every V1 candidate is keyed by a whitelisted project name. A refresh landing
    // mid-loop can tear the view across projects (each answer is individually consistent),
    // which is the same transient staleness any unlocked reader of V1 always had -- but
    // never undefined behavior. The whitelist snapshot is taken with NO facade lock held:
    // Snapshot() takes cs_lock, and the canonical order is cs_lock -> m_service_lock.
    GreylistComputation result;
    result.m_version = GreylistVersion::V1;
    result.m_key = uint256();
    result.m_from_record = false;

    const WhitelistSnapshot whitelist = GetWhitelist().Snapshot(
        GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED);

    for (const auto& project : whitelist) {
        if (m_v1->Contains(project.m_name)) {
            result.m_auto_greylisted.insert(project.m_name);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------- reporting ----

GreylistComputation AutoGreylistService::ComputeReport() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    SuperblockPtr superblock_ptr = Quorum::CurrentSuperblock();

    if (!superblock_ptr.IsEmpty() && superblock_ptr->m_version >= 3
        && IsAutoGreylistRedesignEnabled(superblock_ptr.m_height)) {
        // A fresh walk against the committed head -- the same computation a validator runs
        // to check the record, so this report doubles as an operator-visible cross-check of
        // the recorded m_project_status. Value-returning: no cached state is touched.
        AutoGreylistV2::Result result = AutoGreylistV2::Compute(
            superblock_ptr,
            GetWhitelist().Snapshot(GreylistState::NONE,
                                    GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED),
            GetWhitelist().GetProjectsFirstActive(),
            nullptr,
            GRC::BlockFinder::FindByHeight(superblock_ptr.m_height - 1));

        return ComputationFromWalk(std::move(result), AuthoritativeKey(superblock_ptr));
    }

    // Below the gate: preserve the pre-redesign getautogreylist behavior exactly (an explicit
    // V1 refresh; the RPC reads V1 candidate detail through the transitional iterators).
    // Iterating V1 here (and in the RPC's V1 branch) is safe from iterator invalidation
    // because this method requires cs_main and EVERY producer path that mutates the V1 map
    // also requires cs_main -- the iteration cannot interleave with a refresh. That
    // serialization is the transitional iterators' safety contract; they are retired with V1.
    m_v1->Refresh();

    GreylistComputation result;
    result.m_version = GreylistVersion::V1;
    result.m_key = uint256();
    result.m_from_record = false;

    for (auto iter = m_v1->begin(); iter != m_v1->end(); ++iter) {
        if (iter->second.m_meets_greylisting_crit) {
            result.m_auto_greylisted.insert(iter->first);
        }
    }

    return result;
}

// ------------------------------------------------------------- transitional pass-throughs ----

AutoGreylist::const_iterator AutoGreylistService::begin() const
{
    return m_v1->begin();
}

AutoGreylist::const_iterator AutoGreylistService::end() const
{
    return m_v1->end();
}

AutoGreylist::size_type AutoGreylistService::size() const
{
    return m_v1->size();
}
