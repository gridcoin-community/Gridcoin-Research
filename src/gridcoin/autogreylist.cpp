// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <gridcoin/autogreylist.h>

using namespace GRC;

AutoGreylistService::AutoGreylistService()
    : m_v1(std::make_shared<AutoGreylist>())
    , m_authoritative_source()
    , m_authoritative(std::nullopt)
    , m_pending(std::nullopt)
{
}

// ---------------------------------------------------------------------------- producers ----

void AutoGreylistService::Refresh() EXCLUSIVE_LOCKS_REQUIRED (cs_main)
{
    // V1 producer write. Clear the V2 slots FIRST (scoped -- see the class comment: the
    // service lock must not be held across V1 refresh calls, which re-enter
    // Whitelist::Snapshot and take cs_lock). The V2-producer arm of this dispatch arrives
    // with the state-separation wiring; until then every anchor selects V1.
    ClearV2Slots();

    m_v1->Refresh();
}

void AutoGreylistService::RefreshWithSuperblock(
    SuperblockPtr superblock_ptr_in,
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks)
    EXCLUSIVE_LOCKS_REQUIRED (cs_main)
{
    ClearV2Slots();

    m_v1->RefreshWithSuperblock(superblock_ptr_in, unit_test_blocks);
}

void AutoGreylistService::RefreshWithAndUpdateSuperblock(Superblock& superblock,
                                                         const uint256& convergence_id,
                                                         bool update_pending_cache)
    EXCLUSIVE_LOCKS_REQUIRED (cs_main)
{
    // The convergence identity and the pending-cache election are consumed by the V2 arm of
    // this dispatch (state-separation wiring); the V1 arm reproduces the frozen behavior,
    // which keys and invalidates its single cache internally.
    (void) convergence_id;
    (void) update_pending_cache;

    ClearV2Slots();

    m_v1->RefreshWithAndUpdateSuperblock(superblock);
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
    m_authoritative = std::nullopt;
    m_pending = std::nullopt;
}

// ---------------------------------------------------------------------------- consumers ----

bool AutoGreylistService::Contains(GreylistState state, const std::string& name,
                                   const bool& only_auto_greylisted) const
{
    if (state == GreylistState::NONE) {
        return false;
    }

    LOCK(m_service_lock);

    const std::optional<GreylistComputation>& slot =
        (state == GreylistState::PENDING && m_pending) ? m_pending : m_authoritative;

    if (slot) {
        // A V2-backed result. The membership set holds the projects meeting the greylist
        // criteria, which answers the default (and overlay) question. The candidate-level
        // "has any entry" question (only_auto_greylisted == false) gains per-project detail
        // with the V2 walker; a record-derived result can only answer the criteria question.
        (void) only_auto_greylisted;
        return slot->m_auto_greylisted.count(name) > 0;
    }

    // V1 fall-through: cs_lock -> m_service_lock -> autogreylist_lock (leafward -- safe).
    return m_v1->Contains(name, only_auto_greylisted);
}

bool AutoGreylistService::IsDeepCopyActive(GreylistState state) const
{
    LOCK(m_service_lock);

    if ((state == GreylistState::PENDING && m_pending)
        || (state == GreylistState::AUTHORITATIVE && m_authoritative)) {
        // V2 only runs above the redesign gate, and the deep-copy gate is never above it (the
        // ordering is enforced at startup), so a V2-backed result always deep-copies.
        return true;
    }

    return m_v1->IsDeepCopyActive();
}

std::optional<GreylistComputation> AutoGreylistService::Get(GreylistState state) const
{
    if (state == GreylistState::NONE) {
        return std::nullopt;
    }

    LOCK(m_service_lock);

    if (state == GreylistState::PENDING && m_pending) {
        return m_pending;
    }

    if (m_authoritative) {
        // Serves both the AUTHORITATIVE selector and the PENDING base case: absent a
        // convergence, pending equals authoritative.
        return m_authoritative;
    }

    // V1 fall-through: synthesize a computation from the V1 candidate map so the caller sees
    // one result shape either side of the gate. No key -- V1's cache does not expose one, and
    // below the gate no consumer needs it.
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
