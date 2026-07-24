// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/coinviews.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

namespace GRC {

namespace {

constexpr std::size_t npos = static_cast<std::size_t>(-1);

//! Confirmations sort is height sort with unconfirmed (-1) ranking as the
//! fewest confirmations: ascending confirmations == this key ascending.
int64_t ConfKey(const CoinRecord& r)
{
    if (r.block_height < 0) return std::numeric_limits<int64_t>::min();
    return -static_cast<int64_t>(r.block_height);
}

bool LessOutPoint(const COutPoint& a, const COutPoint& b)
{
    if (a.hash != b.hash) return a.hash < b.hash;
    return a.n < b.n;
}

} // anonymous namespace

CoinViews::CoinViews(RecordFn records)
    : m_records(std::move(records))
{
}

// ---- comparators --------------------------------------------------------

bool CoinViews::lessRecord(const ViewState& v, std::size_t a, std::size_t b) const
{
    const CoinRecord& ra = m_records(a);
    const CoinRecord& rb = m_records(b);

    // Three-way per column, ascending; the outpoint is the deterministic
    // tiebreak so re-sorts are reproducible (the cursor's native-index
    // tiebreak transposed to the coin identity, which is stable across the
    // table compaction shifts).
    int cmp = 0;
    switch (v.sort_column) {
    case COINCOL_AMOUNT:
        cmp = (ra.amount < rb.amount) ? -1 : (ra.amount > rb.amount) ? 1 : 0;
        break;
    case COINCOL_LABEL:
        cmp = ra.label.compare(rb.label);
        if (cmp == 0) cmp = ra.address.compare(rb.address);
        break;
    case COINCOL_ADDRESS:
        cmp = ra.address.compare(rb.address);
        break;
    case COINCOL_DATE:
        cmp = (ra.time < rb.time) ? -1 : (ra.time > rb.time) ? 1 : 0;
        break;
    case COINCOL_CONFS: {
        const int64_t ka = ConfKey(ra), kb = ConfKey(rb);
        cmp = (ka < kb) ? -1 : (ka > kb) ? 1 : 0;
        break;
    }
    }

    if (cmp != 0) {
        return (v.sort_order == 0) ? (cmp < 0) : (cmp > 0);
    }
    return LessOutPoint(ra.outpoint, rb.outpoint);
}

bool CoinViews::lessGroup(const ViewState& v, const std::string& a, const std::string& b) const
{
    auto ita = m_groups.find(a);
    auto itb = m_groups.find(b);
    assert(ita != m_groups.end() && itb != m_groups.end());
    const GroupData& ga = ita->second;
    const GroupData& gb = itb->second;

    // Parent rows have no date/confirmations; those columns fall back to the
    // address so the directory order is defined (the legacy widget's order
    // under these sorts was arbitrary — documented deviation).
    int cmp = 0;
    switch (v.sort_column) {
    case COINCOL_AMOUNT:
        cmp = (ga.total_amount < gb.total_amount) ? -1
            : (ga.total_amount > gb.total_amount) ? 1 : 0;
        break;
    case COINCOL_LABEL:
        cmp = ga.label.compare(gb.label);
        break;
    case COINCOL_ADDRESS:
    case COINCOL_DATE:
    case COINCOL_CONFS:
        cmp = 0; // address tiebreak below decides
        break;
    }

    if (cmp != 0) {
        return (v.sort_order == 0) ? (cmp < 0) : (cmp > 0);
    }
    // Address tiebreak keeps equal-keyed groups in a stable, defined order.
    // Applied in the view's direction for the fallback columns so an
    // address-sorted directory actually toggles with the sort order.
    const int addr_cmp = a.compare(b);
    if (v.sort_column == COINCOL_ADDRESS || v.sort_column == COINCOL_DATE ||
        v.sort_column == COINCOL_CONFS) {
        return (v.sort_order == 0) ? (addr_cmp < 0) : (addr_cmp > 0);
    }
    return addr_cmp < 0;
}

bool CoinViews::lessAmount(std::size_t a, std::size_t b) const
{
    const CoinRecord& ra = m_records(a);
    const CoinRecord& rb = m_records(b);
    if (ra.amount != rb.amount) return ra.amount < rb.amount;
    return LessOutPoint(ra.outpoint, rb.outpoint);
}

// ---- locate helpers -----------------------------------------------------

std::size_t CoinViews::lowerBoundRecord(const ViewState& v,
                                        const std::vector<std::size_t>& index,
                                        std::size_t absidx) const
{
    auto it = std::lower_bound(index.begin(), index.end(), absidx,
                               [&](std::size_t lhs, std::size_t rhs) {
                                   return lessRecord(v, lhs, rhs);
                               });
    return static_cast<std::size_t>(it - index.begin());
}

std::size_t CoinViews::findValue(const std::vector<std::size_t>& index,
                                 std::size_t absidx) const
{
    // Identity locate by VALUE, never by key (the cursor spec's Item 1): a
    // record whose fields already changed cannot be found by key compare.
    auto it = std::find(index.begin(), index.end(), absidx);
    if (it == index.end()) return npos;
    return static_cast<std::size_t>(it - index.begin());
}

std::size_t CoinViews::lowerBoundGroup(const ViewState& v, const std::string& address) const
{
    auto it = std::lower_bound(v.directory.begin(), v.directory.end(), address,
                               [&](const std::string& lhs, const std::string& rhs) {
                                   return lessGroup(v, lhs, rhs);
                               });
    return static_cast<std::size_t>(it - v.directory.begin());
}

std::size_t CoinViews::findGroup(const ViewState& v, const std::string& address) const
{
    auto it = std::find(v.directory.begin(), v.directory.end(), address);
    if (it == v.directory.end()) return npos;
    return static_cast<std::size_t>(it - v.directory.begin());
}

// ---- builds -------------------------------------------------------------

void CoinViews::buildGroups(std::size_t n)
{
    m_groups.clear();
    m_amount_order.clear();
    m_amount_order.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const CoinRecord& r = m_records(i);
        GroupData& g = m_groups[r.group_address];
        g.label = r.label;
        g.total_amount += r.amount;
        g.output_count += 1;
        if (!r.is_change) g.direct_output_count += 1;
        g.members.push_back(i);
        m_amount_order.push_back(i);
    }

    std::sort(m_amount_order.begin(), m_amount_order.end(),
              [this](std::size_t a, std::size_t b) { return lessAmount(a, b); });

    m_count = n;
}

void CoinViews::buildView(ViewState& v, std::size_t n)
{
    v.flat_index.clear();
    v.directory.clear();
    v.group_index.clear();

    if (v.mode == CoinViewMode::Flat) {
        v.flat_index.reserve(n);
        for (std::size_t i = 0; i < n; ++i) v.flat_index.push_back(i);
        std::sort(v.flat_index.begin(), v.flat_index.end(),
                  [&](std::size_t a, std::size_t b) { return lessRecord(v, a, b); });
        return;
    }

    v.directory.reserve(m_groups.size());
    for (const auto& [address, group] : m_groups) {
        v.directory.push_back(address);
        auto& members = v.group_index[address];
        members = group.members;
        std::sort(members.begin(), members.end(),
                  [&](std::size_t a, std::size_t b) { return lessRecord(v, a, b); });
    }
    std::sort(v.directory.begin(), v.directory.end(),
              [&](const std::string& a, const std::string& b) {
                  return lessGroup(v, a, b);
              });
}

// ---- view lifecycle -----------------------------------------------------

std::vector<CoinViewDelta> CoinViews::registerView(int view_id, CoinViewMode mode,
                                                   int sort_column, int sort_order,
                                                   std::size_t n)
{
    // Membership/aggregates are shared; build them on the first registration
    // (or when the caller's table size moved under us via rebuild()).
    if (m_views.empty()) {
        buildGroups(n);
    }
    assert(n == m_count);

    auto [it, inserted] = m_views.try_emplace(view_id);
    ViewState& v = it->second;
    if (!inserted) v.epoch += 1;
    v.mode = mode;
    v.sort_column = sort_column;
    v.sort_order = sort_order;
    v.scope_seqno.clear();
    v.directory_seqno = 0;
    buildView(v, n);

    return {{view_id, CoinViewDelta::Reset, std::string(), 0, 0}};
}

void CoinViews::unregisterView(int view_id)
{
    m_views.erase(view_id);
}

std::vector<CoinViewDelta> CoinViews::setViewMode(int view_id, CoinViewMode mode,
                                                  std::size_t n)
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return {};

    ViewState& v = it->second;
    v.mode = mode;
    v.epoch += 1;
    buildView(v, n);
    return {{view_id, CoinViewDelta::Reset, std::string(), 0, 0}};
}

std::vector<CoinViewDelta> CoinViews::setViewSort(int view_id, int sort_column,
                                                  int sort_order, std::size_t n)
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return {};

    ViewState& v = it->second;
    v.sort_column = sort_column;
    v.sort_order = sort_order;
    v.epoch += 1;
    buildView(v, n);
    return {{view_id, CoinViewDelta::Reset, std::string(), 0, 0}};
}

std::vector<CoinViewDelta> CoinViews::rebuild(std::size_t n)
{
    buildGroups(n);

    std::vector<CoinViewDelta> out;
    for (auto& [view_id, v] : m_views) {
        v.epoch += 1;
        buildView(v, n);
        out.push_back({view_id, CoinViewDelta::Reset, std::string(), 0, 0});
    }
    return out;
}

// ---- directory reslot ---------------------------------------------------

void CoinViews::reslotDirectory(ViewState& v, int view_id, const std::string& address,
                                std::vector<CoinViewDelta>& out)
{
    const std::size_t old_pos = findGroup(v, address);
    assert(old_pos != npos);

    // Erase-then-lower_bound (the cursor spec's Item 2): the group's parent
    // sort key already changed, so its slot must be recomputed against the
    // directory WITHOUT it.
    v.directory.erase(v.directory.begin() + old_pos);
    const std::size_t new_pos = lowerBoundGroup(v, address);
    v.directory.insert(v.directory.begin() + new_pos, address);

    if (new_pos == old_pos) {
        // In place: aggregates changed but the row did not move. An in-place
        // Change is only correct when the position is unchanged — a MOVED
        // group must go out as Remove+Insert or the consumer's positional
        // coordinate system diverges from the store's.
        out.push_back({view_id, CoinViewDelta::GroupChange, std::string(),
                       static_cast<int>(old_pos), 1});
    } else {
        out.push_back({view_id, CoinViewDelta::GroupRemove, std::string(),
                       static_cast<int>(old_pos), 1});
        out.push_back({view_id, CoinViewDelta::GroupInsert, std::string(),
                       static_cast<int>(new_pos), 1});
    }
}

// ---- table mutations ----------------------------------------------------

std::vector<CoinViewDelta> CoinViews::applyInsert(std::size_t absidx)
{
    assert(absidx == m_count);
    const CoinRecord& r = m_records(absidx);

    std::vector<CoinViewDelta> out;

    // Shared membership/aggregates, once.
    auto [git, group_is_new] = m_groups.try_emplace(r.group_address);
    GroupData& g = git->second;
    g.label = r.label;
    g.total_amount += r.amount;
    g.output_count += 1;
    if (!r.is_change) g.direct_output_count += 1;
    g.members.push_back(absidx);

    const std::size_t amount_pos =
        static_cast<std::size_t>(std::lower_bound(m_amount_order.begin(), m_amount_order.end(),
                                                  absidx,
                                                  [this](std::size_t a, std::size_t b) {
                                                      return lessAmount(a, b);
                                                  }) -
                                 m_amount_order.begin());
    m_amount_order.insert(m_amount_order.begin() + amount_pos, absidx);

    // Per-view positional maintenance.
    for (auto& [view_id, v] : m_views) {
        if (v.mode == CoinViewMode::Flat) {
            const std::size_t pos = lowerBoundRecord(v, v.flat_index, absidx);
            v.flat_index.insert(v.flat_index.begin() + pos, absidx);
            out.push_back({view_id, CoinViewDelta::Insert, std::string(),
                           static_cast<int>(pos), 1});
            continue;
        }

        auto& members = v.group_index[r.group_address];
        const std::size_t mpos = lowerBoundRecord(v, members, absidx);
        members.insert(members.begin() + mpos, absidx);
        out.push_back({view_id, CoinViewDelta::Insert, r.group_address,
                       static_cast<int>(mpos), 1});

        if (group_is_new) {
            const std::size_t gpos = lowerBoundGroup(v, r.group_address);
            v.directory.insert(v.directory.begin() + gpos, r.group_address);
            out.push_back({view_id, CoinViewDelta::GroupInsert, std::string(),
                           static_cast<int>(gpos), 1});
        } else {
            reslotDirectory(v, view_id, r.group_address, out);
        }
    }

    m_count += 1;
    return out;
}

std::vector<CoinViewDelta> CoinViews::applyRemove(std::size_t absidx, bool was_selected)
{
    assert(absidx < m_count);
    const CoinRecord& r = m_records(absidx);
    const std::string group_address = r.group_address;

    std::vector<CoinViewDelta> out;

    auto git = m_groups.find(group_address);
    assert(git != m_groups.end());
    GroupData& g = git->second;
    g.total_amount -= r.amount;
    g.output_count -= 1;
    if (!r.is_change) g.direct_output_count -= 1;
    if (was_selected) {
        g.selected_count -= 1;
        g.selected_amount -= r.amount;
    }
    {
        auto mit = std::find(g.members.begin(), g.members.end(), absidx);
        assert(mit != g.members.end());
        g.members.erase(mit);
    }
    const bool group_dead = (g.output_count == 0);

    {
        const std::size_t apos = findValue(m_amount_order, absidx);
        assert(apos != npos);
        m_amount_order.erase(m_amount_order.begin() + apos);
    }

    for (auto& [view_id, v] : m_views) {
        if (v.mode == CoinViewMode::Flat) {
            const std::size_t pos = findValue(v.flat_index, absidx);
            assert(pos != npos);
            v.flat_index.erase(v.flat_index.begin() + pos);
            out.push_back({view_id, CoinViewDelta::Remove, std::string(),
                           static_cast<int>(pos), 1});
            continue;
        }

        auto& members = v.group_index[group_address];
        const std::size_t mpos = findValue(members, absidx);
        assert(mpos != npos);
        members.erase(members.begin() + mpos);
        out.push_back({view_id, CoinViewDelta::Remove, group_address,
                       static_cast<int>(mpos), 1});

        if (group_dead) {
            const std::size_t gpos = findGroup(v, group_address);
            assert(gpos != npos);
            v.directory.erase(v.directory.begin() + gpos);
            v.group_index.erase(group_address);
            out.push_back({view_id, CoinViewDelta::GroupRemove, std::string(),
                           static_cast<int>(gpos), 1});
        } else {
            reslotDirectory(v, view_id, group_address, out);
        }
    }

    if (group_dead) {
        m_groups.erase(git);
    }

    // The caller compacts the table after this returns; mirror the shift.
    shiftDown(absidx);
    m_count -= 1;
    return out;
}

std::vector<CoinViewDelta> CoinViews::applyUpdate(std::size_t absidx,
                                                  const std::string& old_group_address,
                                                  bool old_is_change,
                                                  bool was_selected)
{
    assert(absidx < m_count);
    const CoinRecord& r = m_records(absidx);
    const bool moved_group = (r.group_address != old_group_address);

    std::vector<CoinViewDelta> out;

    // Shared aggregates.
    if (moved_group) {
        auto oit = m_groups.find(old_group_address);
        assert(oit != m_groups.end());
        GroupData& og = oit->second;
        og.total_amount -= r.amount;
        og.output_count -= 1;
        if (!old_is_change) og.direct_output_count -= 1;
        if (was_selected) {
            og.selected_count -= 1;
            og.selected_amount -= r.amount;
        }
        {
            auto mit = std::find(og.members.begin(), og.members.end(), absidx);
            assert(mit != og.members.end());
            og.members.erase(mit);
        }
        const bool old_dead = (og.output_count == 0);

        auto [nit, group_is_new] = m_groups.try_emplace(r.group_address);
        GroupData& ng = nit->second;
        ng.label = r.label;
        ng.total_amount += r.amount;
        ng.output_count += 1;
        if (!r.is_change) ng.direct_output_count += 1;
        if (was_selected) {
            ng.selected_count += 1;
            ng.selected_amount += r.amount;
        }
        ng.members.push_back(absidx);

        for (auto& [view_id, v] : m_views) {
            if (v.mode == CoinViewMode::Flat) {
                // Re-slot: the label/group key change may move the row.
                const std::size_t old_pos = findValue(v.flat_index, absidx);
                assert(old_pos != npos);
                v.flat_index.erase(v.flat_index.begin() + old_pos);
                const std::size_t new_pos = lowerBoundRecord(v, v.flat_index, absidx);
                v.flat_index.insert(v.flat_index.begin() + new_pos, absidx);
                if (new_pos == old_pos) {
                    out.push_back({view_id, CoinViewDelta::Change, std::string(),
                                   static_cast<int>(old_pos), 1});
                } else {
                    out.push_back({view_id, CoinViewDelta::Remove, std::string(),
                                   static_cast<int>(old_pos), 1});
                    out.push_back({view_id, CoinViewDelta::Insert, std::string(),
                                   static_cast<int>(new_pos), 1});
                }
                continue;
            }

            // Out of the old group...
            auto& old_members = v.group_index[old_group_address];
            const std::size_t mpos = findValue(old_members, absidx);
            assert(mpos != npos);
            old_members.erase(old_members.begin() + mpos);
            out.push_back({view_id, CoinViewDelta::Remove, old_group_address,
                           static_cast<int>(mpos), 1});
            if (old_dead) {
                const std::size_t gpos = findGroup(v, old_group_address);
                assert(gpos != npos);
                v.directory.erase(v.directory.begin() + gpos);
                v.group_index.erase(old_group_address);
                out.push_back({view_id, CoinViewDelta::GroupRemove, std::string(),
                               static_cast<int>(gpos), 1});
            } else {
                reslotDirectory(v, view_id, old_group_address, out);
            }

            // ...into the new one.
            auto& new_members = v.group_index[r.group_address];
            const std::size_t npos_new = lowerBoundRecord(v, new_members, absidx);
            new_members.insert(new_members.begin() + npos_new, absidx);
            out.push_back({view_id, CoinViewDelta::Insert, r.group_address,
                           static_cast<int>(npos_new), 1});
            if (group_is_new) {
                const std::size_t gpos = lowerBoundGroup(v, r.group_address);
                v.directory.insert(v.directory.begin() + gpos, r.group_address);
                out.push_back({view_id, CoinViewDelta::GroupInsert, std::string(),
                               static_cast<int>(gpos), 1});
            } else {
                reslotDirectory(v, view_id, r.group_address, out);
            }
        }

        if (old_dead) {
            m_groups.erase(old_group_address);
        }
        return out;
    }

    // Same group: refresh the label snapshot and re-slot within each index.
    auto git = m_groups.find(r.group_address);
    assert(git != m_groups.end());
    git->second.label = r.label;

    for (auto& [view_id, v] : m_views) {
        if (v.mode == CoinViewMode::Flat) {
            const std::size_t old_pos = findValue(v.flat_index, absidx);
            assert(old_pos != npos);
            v.flat_index.erase(v.flat_index.begin() + old_pos);
            const std::size_t new_pos = lowerBoundRecord(v, v.flat_index, absidx);
            v.flat_index.insert(v.flat_index.begin() + new_pos, absidx);
            if (new_pos == old_pos) {
                out.push_back({view_id, CoinViewDelta::Change, std::string(),
                               static_cast<int>(old_pos), 1});
            } else {
                out.push_back({view_id, CoinViewDelta::Remove, std::string(),
                               static_cast<int>(old_pos), 1});
                out.push_back({view_id, CoinViewDelta::Insert, std::string(),
                               static_cast<int>(new_pos), 1});
            }
            continue;
        }

        auto& members = v.group_index[r.group_address];
        const std::size_t old_pos = findValue(members, absidx);
        assert(old_pos != npos);
        members.erase(members.begin() + old_pos);
        const std::size_t new_pos = lowerBoundRecord(v, members, absidx);
        members.insert(members.begin() + new_pos, absidx);
        if (new_pos == old_pos) {
            out.push_back({view_id, CoinViewDelta::Change, r.group_address,
                           static_cast<int>(old_pos), 1});
        } else {
            out.push_back({view_id, CoinViewDelta::Remove, r.group_address,
                           static_cast<int>(old_pos), 1});
            out.push_back({view_id, CoinViewDelta::Insert, r.group_address,
                           static_cast<int>(new_pos), 1});
        }

        // A label change re-keys a label-sorted directory (and refreshes the
        // rendered parent row either way).
        reslotDirectory(v, view_id, r.group_address, out);
    }

    return out;
}

std::vector<CoinViewDelta> CoinViews::applySelection(std::size_t absidx, bool selected)
{
    assert(absidx < m_count);
    const CoinRecord& r = m_records(absidx);

    auto git = m_groups.find(r.group_address);
    assert(git != m_groups.end());
    GroupData& g = git->second;
    if (selected) {
        g.selected_count += 1;
        g.selected_amount += r.amount;
    } else {
        g.selected_count -= 1;
        g.selected_amount -= r.amount;
    }

    // Selection never moves a directory row (selected_* are not parent sort
    // keys) — an in-place GroupChange refreshes the tristate rendering.
    std::vector<CoinViewDelta> out;
    for (auto& [view_id, v] : m_views) {
        if (v.mode != CoinViewMode::Tree) continue;
        const std::size_t gpos = findGroup(v, r.group_address);
        assert(gpos != npos);
        out.push_back({view_id, CoinViewDelta::GroupChange, std::string(),
                       static_cast<int>(gpos), 1});
    }
    return out;
}

void CoinViews::shiftDown(std::size_t absidx)
{
    auto shift = [absidx](std::vector<std::size_t>& index) {
        for (std::size_t& v : index) {
            if (v > absidx) v -= 1;
        }
    };

    shift(m_amount_order);
    for (auto& entry : m_groups) {
        shift(entry.second.members);
    }
    for (auto& view_entry : m_views) {
        ViewState& v = view_entry.second;
        shift(v.flat_index);
        for (auto& group_entry : v.group_index) {
            shift(group_entry.second);
        }
    }
}

// ---- reads --------------------------------------------------------------

namespace {

//! Clamp [first, first+count) against a container's size; count < 0 = rest.
std::pair<std::size_t, std::size_t> ClampRange(std::size_t size, int first, int count)
{
    if (first < 0) first = 0;
    std::size_t lo = std::min(static_cast<std::size_t>(first), size);
    std::size_t n = (count < 0) ? (size - lo)
                                : std::min(static_cast<std::size_t>(count), size - lo);
    return {lo, n};
}

} // anonymous namespace

std::vector<std::size_t> CoinViews::flatSlice(int view_id, int first, int count,
                                              int& total_out) const
{
    total_out = 0;
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return {};
    const ViewState& v = it->second;

    total_out = static_cast<int>(v.flat_index.size());
    auto [lo, n] = ClampRange(v.flat_index.size(), first, count);
    return std::vector<std::size_t>(v.flat_index.begin() + lo,
                                    v.flat_index.begin() + lo + n);
}

std::vector<std::string> CoinViews::directorySlice(int view_id, int first, int count,
                                                   int& total_out) const
{
    total_out = 0;
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return {};
    const ViewState& v = it->second;

    total_out = static_cast<int>(v.directory.size());
    auto [lo, n] = ClampRange(v.directory.size(), first, count);
    return std::vector<std::string>(v.directory.begin() + lo,
                                    v.directory.begin() + lo + n);
}

std::vector<std::size_t> CoinViews::groupSlice(int view_id, const std::string& group_address,
                                               int first, int count, int& total_out) const
{
    total_out = 0;
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return {};
    const ViewState& v = it->second;

    auto git = v.group_index.find(group_address);
    if (git == v.group_index.end()) return {};
    const auto& members = git->second;

    total_out = static_cast<int>(members.size());
    auto [lo, n] = ClampRange(members.size(), first, count);
    return std::vector<std::size_t>(members.begin() + lo, members.begin() + lo + n);
}

CoinGroupInfo CoinViews::groupInfo(const std::string& group_address) const
{
    CoinGroupInfo info;
    auto it = m_groups.find(group_address);
    if (it == m_groups.end()) return info;

    const GroupData& g = it->second;
    info.address = group_address;
    info.label = g.label;
    info.total_amount = g.total_amount;
    info.output_count = g.output_count;
    info.direct_output_count = g.direct_output_count;
    info.selected_count = g.selected_count;
    info.selected_amount = g.selected_amount;
    return info;
}

std::vector<CoinGroupInfo> CoinViews::groupDirectory() const
{
    std::vector<CoinGroupInfo> out;
    out.reserve(m_groups.size());
    // m_groups is an ordered map, so this is the address-sorted directory.
    for (const auto& entry : m_groups) {
        out.push_back(groupInfo(entry.first));
    }
    return out;
}

std::vector<std::size_t> CoinViews::groupMembers(const std::string& group_address) const
{
    auto it = m_groups.find(group_address);
    if (it == m_groups.end()) return {};
    return it->second.members;
}

// ---- event bookkeeping --------------------------------------------------

void CoinViews::noteScopeEvent(int view_id, const std::string& scope, uint64_t seqno)
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return;
    it->second.scope_seqno[scope] = seqno;
}

void CoinViews::noteDirectoryEvent(int view_id, uint64_t seqno)
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return;
    it->second.directory_seqno = seqno;
}

void CoinViews::noteBroadcast(int view_id, uint64_t seqno)
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return;
    it->second.floor_seqno = seqno;
}

uint64_t CoinViews::highWater(int view_id, const std::string& scope) const
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return 0;
    const ViewState& v = it->second;

    uint64_t scope_seqno = 0;
    auto sit = v.scope_seqno.find(scope);
    if (sit != v.scope_seqno.end()) scope_seqno = sit->second;
    return std::max(scope_seqno, v.floor_seqno);
}

uint64_t CoinViews::directoryHighWater(int view_id) const
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return 0;
    return std::max(it->second.directory_seqno, it->second.floor_seqno);
}

uint64_t CoinViews::epoch(int view_id) const
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return 0;
    return it->second.epoch;
}

CoinViewMode CoinViews::mode(int view_id) const
{
    auto it = m_views.find(view_id);
    if (it == m_views.end()) return CoinViewMode::Tree;
    return it->second.mode;
}

} // namespace GRC
