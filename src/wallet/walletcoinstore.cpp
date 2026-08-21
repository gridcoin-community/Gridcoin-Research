// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/walletcoinstore.h"

#include "chain.h"
#include "consensus/tx_verify.h"
#include "key_io.h"
#include "main.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace GRC {

WalletCoinStore::WalletCoinStore(CWallet* wallet, GRC::WalletCoinEventQueue& queue)
    : m_wallet(wallet)
    , m_queue(queue)
    , m_views([this](std::size_t i) -> const CoinRecord& { return recordAt(i); })
{
}

const CoinRecord& WalletCoinStore::recordAt(std::size_t i) const
{
    return m_records[i];
}

WalletCoinStore::~WalletCoinStore()
{
    {
        LOCK(cs_intake);
        m_stop = true;
    }
    m_intake_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void WalletCoinStore::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_worker = std::thread([this] { workerLoop(); });
}

// ---- producers ----------------------------------------------------------

void WalletCoinStore::enqueueUpsert(const uint256& hash, std::vector<CoinRecord> records,
                                    bool pending)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::Upsert, hash, std::move(records), pending});
    }
    m_intake_cv.notify_one();
}

void WalletCoinStore::enqueueRemove(const uint256& hash)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::Remove, hash});
    }
    m_intake_cv.notify_one();
}

void WalletCoinStore::enqueueAddressBookChange(const std::string& address, const std::string& label)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::AddressBook, uint256(), {}, false,
                                      address, label});
    }
    m_intake_cv.notify_one();
}

// ---- worker (the WalletTxStore park/drain loop, verbatim shape) ---------

void WalletCoinStore::workerLoop()
{
    WAIT_LOCK(cs_intake, lock);
    while (true) {
        // Explicit while-condition form (not a wait() predicate lambda) so the
        // thread-safety analyzer keeps seeing the held lock over the guarded
        // reads — the WalletTxStore::workerLoop discipline.
        while (!m_stop && (m_rebuilding || m_intake.empty())) {
            if (m_rebuilding && !m_worker_parked) {
                m_worker_parked = true;
                m_idle_cv.notify_all();
            }
            m_intake_cv.wait(lock);
        }
        if (m_stop) {
            return;
        }
        m_worker_parked = false;

        IntakeItem item = std::move(m_intake.front());
        m_intake.pop_front();

        // Drop cs_intake for the O(N) maintenance (which takes cs_store); the
        // two leaves are never held together.
        {
            REVERSE_LOCK(lock);
            applyIntake(std::move(item));
        }
    }
}

void WalletCoinStore::applyIntake(IntakeItem item)
{
    if (item.kind == IntakeItem::Upsert) {
        upsertCoins(item.hash, std::move(item.records), item.pending);
    } else if (item.kind == IntakeItem::AddressBook) {
        applyAddressBookChange(item.ab_address, item.ab_label);
    } else {
        removeCoins(item.hash);
    }
}

// ---- decomposition ------------------------------------------------------

namespace {

//! The change-walk from the retired listCoins(): a change output groups under
//! the address of the ancestor output that funded its chain. Returns the
//! walked destination's encoded address, or "" when none is extractable.
//! \p walk_memo caches the from-tx walk result by txid (the walk from any
//! change output of one tx follows vin[0] identically).
std::string WalkChangeGroup(CWallet* wallet, const CWalletTx& wtx, unsigned int i,
                            std::map<uint256, std::string>* walk_memo)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet)
{
    const CWalletTx* ptx = &wtx;
    unsigned int idx = i;

    if (walk_memo) {
        auto mit = walk_memo->find(wtx.GetHash());
        if (mit != walk_memo->end()) return mit->second;
    }

    while (wallet->IsChange(ptx->vout[idx]) && ptx->vin.size() > 0
           && wallet->IsMine(ptx->vin[0]) != ISMINE_NO)
    {
        auto it = wallet->mapWallet.find(ptx->vin[0].prevout.hash);
        if (it == wallet->mapWallet.end()) break;
        idx = ptx->vin[0].prevout.n;
        ptx = &it->second;
        if (idx >= ptx->vout.size()) return std::string(); // corrupt walk target
    }

    CTxDestination dest;
    std::string group;
    if (ExtractDestination(ptx->vout[idx].scriptPubKey, dest)) {
        group = EncodeDestination(dest);
    }
    if (walk_memo) {
        (*walk_memo)[wtx.GetHash()] = group;
    }
    return group;
}

} // anonymous namespace

std::vector<CoinRecord> WalletCoinStore::DecomposeCoins(
    CWallet* wallet, const CWalletTx& wtx, bool& pending_out,
    std::map<uint256, std::string>* walk_memo)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet)
{
    // Reproduces the CWallet::AvailableCoins(fOnlyConfirmed=true,
    // coinControl=nullptr, fIncludeStakedCoins=false) conditions the retired
    // listCoins() applied, per transaction. Exclusions that depend on global
    // chain state (finality, the IsTrusted confirmation gate, generation
    // maturity) flag pending_out: no notification fires for THIS hash when
    // the chain advances past them, so applyChainTipRefresh re-decomposes
    // the flagged set each tip.
    AssertLockHeld(cs_main);
    AssertLockHeld(wallet->cs_wallet);

    pending_out = false;
    std::vector<CoinRecord> records;

    const int depth = wtx.GetDepthInMainChain();

    if (!IsFinalTx(wtx)) {
        pending_out = true;
        return records;
    }
    if (!wtx.IsTrusted()) {
        // Untrusted-below-3-conf becomes trusted with depth alone; a
        // conflicted tx (depth < 0) instead changes via its own or its
        // parents' notifications, but the flag is a bounded recheck either way.
        pending_out = (depth >= 0);
        return records;
    }
    if ((wtx.IsCoinBase() || wtx.IsCoinStake()) && wtx.GetBlocksToMaturity() > 0) {
        pending_out = true;
        return records;
    }
    if (depth < 0) {
        return records;
    }

    int block_height = -1;
    if (!wtx.hashBlock.IsNull()) {
        auto bi = mapBlockIndex.find(wtx.hashBlock);
        if (bi != mapBlockIndex.end() && bi->second != nullptr) {
            block_height = bi->second->nHeight;
        }
    }

    for (unsigned int i = 0; i < wtx.vout.size(); ++i) {
        if (wallet->IsMine(wtx.vout[i]) == ISMINE_NO) continue;
        if (wtx.IsSpent(i)) continue;
        if (wtx.vout[i].nValue < nMinimumInputValue) continue;

        CTxDestination dest;
        std::string address;
        if (ExtractDestination(wtx.vout[i].scriptPubKey, dest)) {
            address = EncodeDestination(dest);
        }

        std::string group = address;
        bool is_change = false;
        if (wallet->IsChange(wtx.vout[i]) && wtx.vin.size() > 0
            && wallet->IsMine(wtx.vin[0]) != ISMINE_NO) {
            group = WalkChangeGroup(wallet, wtx, i, walk_memo);
            is_change = (group != address);
        }
        // The retired listCoins() dropped outputs with no extractable group
        // key; preserve that.
        if (group.empty()) continue;

        std::string label;
        {
            CTxDestination group_dest = DecodeDestination(group);
            auto abit = wallet->mapAddressBook.find(group_dest);
            if (abit != wallet->mapAddressBook.end()) {
                label = abit->second.name;
            }
        }

        CoinRecord r;
        r.outpoint = COutPoint(wtx.GetHash(), i);
        r.amount = wtx.vout[i].nValue;
        r.address = address;
        r.group_address = group;
        r.label = label;
        r.time = wtx.GetTxTime();
        r.block_height = block_height;
        r.is_change = is_change;
        records.push_back(std::move(r));
    }

    return records;
}

// ---- store maintenance --------------------------------------------------

void WalletCoinStore::removeRecordAt(std::size_t absidx)
{
    AssertLockHeld(cs_store);

    const COutPoint outpoint = m_records[absidx].outpoint;
    const bool was_selected = (m_selected.count(outpoint) > 0);

    // CoinViews first (it reads the record and mirrors the compaction shift
    // internally), then compact the table and this class's identity maps.
    const auto deltas = m_views.applyRemove(absidx, was_selected);
    m_selected.erase(outpoint);
    emitDeltas(deltas, &outpoint);

    m_records.erase(m_records.begin() + absidx);
    m_by_outpoint.erase(outpoint);
    {
        auto range = m_by_hash.equal_range(outpoint.hash);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == absidx) {
                m_by_hash.erase(it);
                break;
            }
        }
    }
    for (auto& kv : m_by_outpoint) {
        if (kv.second > absidx) kv.second -= 1;
    }
    for (auto& kv : m_by_hash) {
        if (kv.second > absidx) kv.second -= 1;
    }
}

void WalletCoinStore::upsertCoins(const uint256& hash, std::vector<CoinRecord> records,
                                  bool pending)
{
    LOCK(cs_store);

    if (pending) {
        m_pending.insert(hash);
    } else {
        m_pending.erase(hash);
    }

    // Index the incoming set by outpoint.
    std::map<COutPoint, const CoinRecord*> incoming;
    for (const CoinRecord& r : records) {
        incoming.emplace(r.outpoint, &r);
    }

    // Snapshot the stored (absidx, outpoint) pairs for this hash.
    std::vector<std::pair<std::size_t, COutPoint>> stored;
    {
        auto range = m_by_hash.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it) {
            stored.emplace_back(it->second, m_records[it->second].outpoint);
        }
    }

    // Removals first, in DESCENDING absidx order so the compaction shifts
    // cannot invalidate the indices still queued for removal.
    std::sort(stored.begin(), stored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& [absidx, outpoint] : stored) {
        if (incoming.count(outpoint) == 0) {
            removeRecordAt(absidx);
        }
    }

    // In-place updates for surviving outpoints whose fields changed.
    for (const CoinRecord& r : records) {
        auto it = m_by_outpoint.find(r.outpoint);
        if (it == m_by_outpoint.end()) continue;
        CoinRecord& stored_rec = m_records[it->second];
        if (stored_rec.block_height == r.block_height
            && stored_rec.label == r.label
            && stored_rec.group_address == r.group_address
            && stored_rec.is_change == r.is_change
            && stored_rec.amount == r.amount) {
            continue;
        }
        const std::string old_group = stored_rec.group_address;
        const bool old_is_change = stored_rec.is_change;
        const bool was_selected = (m_selected.count(r.outpoint) > 0);
        stored_rec = r;
        emitDeltas(m_views.applyUpdate(it->second, old_group, old_is_change, was_selected));
    }

    // Fresh inserts (append + index).
    for (const CoinRecord& r : records) {
        if (m_by_outpoint.count(r.outpoint) > 0) continue;
        const std::size_t absidx = m_records.size();
        m_records.push_back(r);
        m_by_outpoint.emplace(r.outpoint, absidx);
        m_by_hash.emplace(r.outpoint.hash, absidx);
        emitDeltas(m_views.applyInsert(absidx));
    }
}

void WalletCoinStore::removeCoins(const uint256& hash)
{
    LOCK(cs_store);

    m_pending.erase(hash);

    std::vector<std::size_t> indices;
    {
        auto range = m_by_hash.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it) {
            indices.push_back(it->second);
        }
    }
    std::sort(indices.rbegin(), indices.rend());
    for (std::size_t absidx : indices) {
        removeRecordAt(absidx);
    }
}

void WalletCoinStore::applyAddressBookChange(const std::string& address, const std::string& label)
{
    LOCK(cs_store);

    // Refresh the label snapshot on records grouped under the address. The
    // IsChange regroup consequences arrive as producer-side upserts (the
    // worker never takes wallet locks); this pass keeps the rendered labels
    // and the label sort key current.
    for (std::size_t i = 0; i < m_records.size(); ++i) {
        if (m_records[i].group_address != address) continue;
        if (m_records[i].label == label) continue;
        const std::string old_group = m_records[i].group_address;
        const bool old_is_change = m_records[i].is_change;
        const bool was_selected = (m_selected.count(m_records[i].outpoint) > 0);
        m_records[i].label = label;
        emitDeltas(m_views.applyUpdate(i, old_group, old_is_change, was_selected));
    }
}

void WalletCoinStore::applyChainTipRefresh(int height)
{
    AssertLockHeld(cs_main);
    m_tip_height.store(height, std::memory_order_relaxed);

    if (m_wallet) {
        // Re-decompose the bounded pending-availability set: coins whose
        // exclusion was depth/time-dependent (trust gate, maturity, finality)
        // flip with the chain alone, with no notification for their hash.
        // cs_wallet under the caller's cs_main, then cs_store inside
        // upsertCoins — canonical order; the worker is never involved.
        LOCK(m_wallet->cs_wallet);

        std::vector<uint256> pending;
        {
            LOCK(cs_store);
            pending.assign(m_pending.begin(), m_pending.end());
        }
        for (const uint256& hash : pending) {
            auto it = m_wallet->mapWallet.find(hash);
            if (it == m_wallet->mapWallet.end()) {
                removeCoins(hash);
                continue;
            }
            bool still_pending = false;
            std::vector<CoinRecord> recs = DecomposeCoins(m_wallet, it->second, still_pending);
            upsertCoins(hash, std::move(recs), still_pending);
        }
    }

    LOCK(cs_store);
    if (m_views.hasViews()) {
        m_queue.push(CoinDepthRefreshPayload{height});
    }
}

// ---- event emission -----------------------------------------------------

void WalletCoinStore::emitDeltas(const std::vector<CoinViewDelta>& deltas,
                                 const COutPoint* removed_outpoint)
{
    AssertLockHeld(cs_store);

    for (const CoinViewDelta& d : deltas) {
        const uint64_t epoch = m_views.epoch(d.view_id);
        switch (d.type) {
        case CoinViewDelta::Insert: {
            int total = 0;
            std::vector<std::size_t> idxs =
                d.scope.empty() ? m_views.flatSlice(d.view_id, d.first, d.count, total)
                                : m_views.groupSlice(d.view_id, d.scope, d.first, d.count, total);
            std::vector<CoinRecord> recs;
            recs.reserve(idxs.size());
            for (std::size_t idx : idxs) recs.push_back(m_records[idx]);
            fillDepth(recs);
            const uint64_t seqno = m_queue.push(CoinRowsInsertedPayload{
                d.view_id, epoch, d.scope, d.first, std::move(recs)});
            m_views.noteScopeEvent(d.view_id, d.scope, seqno);
            break;
        }
        case CoinViewDelta::Remove: {
            std::vector<COutPoint> outs;
            if (removed_outpoint) outs.push_back(*removed_outpoint);
            const uint64_t seqno = m_queue.push(CoinRowsRemovedPayload{
                d.view_id, epoch, d.scope, d.first, d.count, std::move(outs)});
            m_views.noteScopeEvent(d.view_id, d.scope, seqno);
            break;
        }
        case CoinViewDelta::Change: {
            const uint64_t seqno = m_queue.push(CoinRowsChangedPayload{
                d.view_id, epoch, d.scope, d.first, d.count});
            m_views.noteScopeEvent(d.view_id, d.scope, seqno);
            break;
        }
        case CoinViewDelta::GroupInsert: {
            int total = 0;
            std::vector<std::string> addrs =
                m_views.directorySlice(d.view_id, d.first, d.count, total);
            std::vector<CoinGroupInfo> groups;
            groups.reserve(addrs.size());
            for (const std::string& a : addrs) groups.push_back(m_views.groupInfo(a));
            const uint64_t seqno = m_queue.push(CoinGroupsInsertedPayload{
                d.view_id, epoch, d.first, std::move(groups)});
            m_views.noteDirectoryEvent(d.view_id, seqno);
            break;
        }
        case CoinViewDelta::GroupRemove: {
            const uint64_t seqno = m_queue.push(CoinGroupsRemovedPayload{
                d.view_id, epoch, d.first, d.count});
            m_views.noteDirectoryEvent(d.view_id, seqno);
            break;
        }
        case CoinViewDelta::GroupChange: {
            const uint64_t seqno = m_queue.push(CoinGroupsChangedPayload{
                d.view_id, epoch, d.first, d.count});
            m_views.noteDirectoryEvent(d.view_id, seqno);
            break;
        }
        case CoinViewDelta::Reset: {
            const uint64_t seqno = m_queue.push(CoinResetPayload{d.view_id, epoch});
            m_views.noteBroadcast(d.view_id, seqno);
            break;
        }
        }
    }
}

void WalletCoinStore::fillDepth(std::vector<CoinRecord>& records) const
{
    const int tip = m_tip_height.load(std::memory_order_relaxed);
    for (CoinRecord& r : records) {
        r.depth = (r.block_height >= 0 && tip >= r.block_height)
                      ? tip - r.block_height + 1
                      : 0;
    }
}

// ---- view lifecycle -----------------------------------------------------

void WalletCoinStore::registerView(int view_id, CoinViewMode mode,
                                   int sort_column, int sort_order)
{
    LOCK(cs_store);
    const bool first = !m_views.hasViews();
    const auto deltas = m_views.registerView(view_id, mode, sort_column, sort_order,
                                             m_records.size());
    if (first) {
        // The first registration (re)built the shared aggregates, which
        // cleared the selection counters; restore them from the mirror.
        reapplyMirrorAggregates();
    }
    emitDeltas(deltas);
}

void WalletCoinStore::unregisterView(int view_id)
{
    LOCK(cs_store);
    m_views.unregisterView(view_id);
    if (!m_views.hasViews()) {
        // Nothing watches: discard pending events (seqnos stay monotonic) so
        // an idle GUI node accumulates nothing between dialog opens.
        m_queue.clear();
    }
}

void WalletCoinStore::setViewMode(int view_id, CoinViewMode mode)
{
    LOCK(cs_store);
    emitDeltas(m_views.setViewMode(view_id, mode, m_records.size()));
}

void WalletCoinStore::setViewSort(int view_id, int sort_column, int sort_order)
{
    LOCK(cs_store);
    emitDeltas(m_views.setViewSort(view_id, sort_column, sort_order, m_records.size()));
}

// ---- reads --------------------------------------------------------------

CoinRowsResult WalletCoinStore::getRows(int view_id, int first, int count)
{
    LOCK(cs_store);
    CoinRowsResult result;
    std::vector<std::size_t> idxs =
        m_views.flatSlice(view_id, first, count, result.total_accepted);
    result.records.reserve(idxs.size());
    for (std::size_t idx : idxs) result.records.push_back(m_records[idx]);
    fillDepth(result.records);
    result.epoch = m_views.epoch(view_id);
    result.high_water = m_views.highWater(view_id, std::string());
    return result;
}

CoinGroupsResult WalletCoinStore::getGroups(int view_id, int first, int count)
{
    LOCK(cs_store);
    CoinGroupsResult result;
    std::vector<std::string> addrs =
        m_views.directorySlice(view_id, first, count, result.total_groups);
    result.groups.reserve(addrs.size());
    for (const std::string& a : addrs) result.groups.push_back(m_views.groupInfo(a));
    result.epoch = m_views.epoch(view_id);
    result.high_water = m_views.directoryHighWater(view_id);
    return result;
}

CoinRowsResult WalletCoinStore::getGroupRows(int view_id, const std::string& group_address,
                                             int first, int count)
{
    LOCK(cs_store);
    CoinRowsResult result;
    std::vector<std::size_t> idxs =
        m_views.groupSlice(view_id, group_address, first, count, result.total_accepted);
    result.records.reserve(idxs.size());
    for (std::size_t idx : idxs) result.records.push_back(m_records[idx]);
    fillDepth(result.records);
    result.epoch = m_views.epoch(view_id);
    result.high_water = m_views.highWater(view_id, group_address);
    return result;
}

std::vector<CoinGroupInfo> WalletCoinStore::getGroupDirectory()
{
    LOCK(cs_store);
    return m_views.groupDirectory();
}

// ---- selection ----------------------------------------------------------

void WalletCoinStore::reapplyMirrorAggregates()
{
    AssertLockHeld(cs_store);
    for (const COutPoint& outpoint : m_selected) {
        auto it = m_by_outpoint.find(outpoint);
        if (it == m_by_outpoint.end()) continue;
        (void)m_views.applySelection(it->second, true);
    }
}

std::set<COutPoint> WalletCoinStore::reconcileSelection(std::set<COutPoint> selection)
{
    LOCK(cs_store);

    // Clear the current mirror's aggregates, then install the pruned set.
    // No events: the consumer reconciles immediately before seeding its
    // caches, so it reads the refreshed aggregates directly.
    for (const COutPoint& outpoint : m_selected) {
        auto it = m_by_outpoint.find(outpoint);
        if (it == m_by_outpoint.end()) continue;
        (void)m_views.applySelection(it->second, false);
    }

    std::set<COutPoint> pruned;
    for (const COutPoint& outpoint : selection) {
        if (m_by_outpoint.count(outpoint) > 0) {
            pruned.insert(outpoint);
        }
    }
    m_selected = pruned;
    reapplyMirrorAggregates();
    return pruned;
}

CoinSelectionUpdate WalletCoinStore::setSelected(const COutPoint& outpoint, bool selected)
{
    LOCK(cs_store);
    CoinSelectionUpdate update;

    auto it = m_by_outpoint.find(outpoint);
    if (it == m_by_outpoint.end()) {
        // Unknown (e.g. spent and removed between render and click): refuse.
        // The caller must NOT mutate its own selection set — accepting it
        // would plant a phantom mirror entry no future event repairs (the
        // outpoint's removal event predates this call).
        return update;
    }

    update.applied = true;
    const bool currently = (m_selected.count(outpoint) > 0);
    if (currently != selected) {
        if (selected) {
            m_selected.insert(outpoint);
        } else {
            m_selected.erase(outpoint);
        }
        emitDeltas(m_views.applySelection(it->second, selected));
    }
    update.group = m_views.groupInfo(m_records[it->second].group_address);
    return update;
}

void WalletCoinStore::toggleLocked(std::size_t absidx, bool selected,
                                   CoinBulkSelectionResult& result)
{
    const COutPoint& outpoint = m_records[absidx].outpoint;
    const bool currently = (m_selected.count(outpoint) > 0);
    if (currently == selected) return;

    if (selected) {
        m_selected.insert(outpoint);
        result.added.push_back(outpoint);
    } else {
        m_selected.erase(outpoint);
        result.removed.push_back(outpoint);
    }
    emitDeltas(m_views.applySelection(absidx, selected));
}

CoinBulkSelectionResult WalletCoinStore::selectGroup(const std::string& group_address,
                                                     bool selected)
{
    LOCK(cs_store);
    CoinBulkSelectionResult result;
    for (std::size_t absidx : m_views.groupMembers(group_address)) {
        toggleLocked(absidx, selected, result);
    }
    return result;
}

CoinBulkSelectionResult WalletCoinStore::selectAll(bool selected)
{
    LOCK(cs_store);
    CoinBulkSelectionResult result;
    for (std::size_t absidx = 0; absidx < m_records.size(); ++absidx) {
        toggleLocked(absidx, selected, result);
    }
    return result;
}

CoinBulkSelectionResult WalletCoinStore::applyValueFilter(bool less_or_equal, int64_t value,
                                                          uint32_t max_inputs)
{
    LOCK(cs_store);
    CoinBulkSelectionResult result;

    // Phase 1 — predicate prune over the current selection (never selects):
    // deselect members on the wrong side of the value threshold.
    const std::vector<std::size_t>& order = m_views.amountOrder();
    for (std::size_t absidx : order) {
        const CoinRecord& r = m_records[absidx];
        if (m_selected.count(r.outpoint) == 0) continue;
        const bool fails = less_or_equal ? (r.amount > value) : (r.amount < value);
        if (fails) toggleLocked(absidx, false, result);
    }

    // Phase 2 — the input cap: keep the max_inputs smallest (less_or_equal)
    // or largest survivors, walking the persistent (amount, outpoint) order
    // so ties at the boundary break deterministically and the pass is O(n)
    // with no per-call sort inside this GUI-thread cs_store hold.
    uint32_t kept = 0;
    if (less_or_equal) {
        for (auto it = order.begin(); it != order.end(); ++it) {
            capPassLocked(*it, max_inputs, kept, result);
        }
    } else {
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            capPassLocked(*it, max_inputs, kept, result);
        }
    }

    return result;
}

void WalletCoinStore::capPassLocked(std::size_t absidx, uint32_t max_inputs, uint32_t& kept,
                                    CoinBulkSelectionResult& result)
{
    const CoinRecord& r = m_records[absidx];
    if (m_selected.count(r.outpoint) == 0) return;
    if (kept < max_inputs) {
        ++kept;
    } else {
        toggleLocked(absidx, false, result);
        result.culled = true;
    }
}

// ---- load / resync ------------------------------------------------------

CoinGroupsResult WalletCoinStore::reloadAndSnapshot()
{
    CoinGroupsResult result;
    if (!m_wallet) {
        return result;
    }

    // Hold cs_main + cs_wallet across the scan AND the queue discard, with
    // the store-worker parked — the WalletTxStore::reloadAndSnapshot
    // protocol, verbatim: producers are excluded by the wallet locks, the
    // worker by the park, so the swapped index, the emptied queue, and the
    // published Resets are mutually consistent.
    LOCK2(cs_main, m_wallet->cs_wallet);

    {
        WAIT_LOCK(cs_intake, ilock);
        m_rebuilding = true;
        m_intake_cv.notify_all();
        while (m_started && !m_worker_parked) {
            m_idle_cv.wait(ilock);
        }
        m_intake.clear();
    }

    std::vector<CoinRecord> built;
    std::unordered_set<uint256, CoinTxHashHasher> pending_set;
    std::map<uint256, std::string> walk_memo;
    built.reserve(m_wallet->mapWallet.size());
    for (const auto& entry : m_wallet->mapWallet) {
        bool pending = false;
        std::vector<CoinRecord> recs =
            DecomposeCoins(m_wallet, entry.second, pending, &walk_memo);
        if (pending) pending_set.insert(entry.first);
        for (CoinRecord& r : recs) built.push_back(std::move(r));
    }

    m_tip_height.store(nBestHeight, std::memory_order_relaxed);

    std::vector<CoinViewDelta> resets;
    {
        LOCK(cs_store);
        m_records = std::move(built);
        m_by_outpoint.clear();
        m_by_hash.clear();
        for (std::size_t i = 0; i < m_records.size(); ++i) {
            m_by_outpoint.emplace(m_records[i].outpoint, i);
            m_by_hash.emplace(m_records[i].outpoint.hash, i);
        }
        m_pending = std::move(pending_set);

        // Prune the mirror to surviving coins, then rebuild the views (which
        // clears aggregates) and restore the mirror's aggregates.
        for (auto it = m_selected.begin(); it != m_selected.end();) {
            if (m_by_outpoint.count(*it) == 0) {
                it = m_selected.erase(it);
            } else {
                ++it;
            }
        }
        resets = m_views.rebuild(m_records.size());
        reapplyMirrorAggregates();
    }

    // Events computed against the old index are superseded; discard them
    // (producers still blocked on the wallet locks, worker parked).
    m_queue.drain();

    {
        LOCK(cs_store);
        // Publish each view's Reset AFTER the discard so it survives, and
        // record it as the view's broadcast floor.
        emitDeltas(resets);
        result.groups = m_views.groupDirectory();
        result.total_groups = static_cast<int>(result.groups.size());
    }

    {
        LOCK(cs_intake);
        m_rebuilding = false;
    }
    m_intake_cv.notify_all();

    return result;
}

} // namespace GRC
