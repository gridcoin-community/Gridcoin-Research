// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/cursor.h"

#include <algorithm>
#include <cstdint>

namespace {
constexpr std::size_t NPOS = static_cast<std::size_t>(-1);
} // anonymous namespace

namespace GRC {

Cursor::Cursor(int view_id, FilterSpec filter, int sort_column, int sort_order,
               FieldsFn fields, KeysFn keys)
    : m_view_id(view_id)
    , m_filter(std::move(filter))
    , m_sort_column(sort_column)
    , m_sort_order(sort_order)
    , m_fields(std::move(fields))
    , m_keys(std::move(keys))
{
}

std::size_t Cursor::cap() const
{
    return m_filter.limit_rows < 0 ? NPOS : static_cast<std::size_t>(m_filter.limit_rows);
}

std::size_t Cursor::servedCount() const
{
    return std::min(m_view_index.size(), cap());
}

std::size_t Cursor::lowerBoundSlot(std::size_t absidx) const
{
    // Sorted insertion slot. Only valid for a row NOT currently present.
    auto it = std::lower_bound(
        m_view_index.begin(), m_view_index.end(), absidx,
        [&](std::size_t a, std::size_t b) { return Less(m_keys(a), m_keys(b), m_sort_column, m_sort_order); });
    return static_cast<std::size_t>(it - m_view_index.begin());
}

std::size_t Cursor::findSlot(std::size_t absidx) const
{
    // Identity locate (spec Item 1): never a sort-key binary search.
    auto it = std::find(m_view_index.begin(), m_view_index.end(), absidx);
    return it == m_view_index.end() ? NPOS : static_cast<std::size_t>(it - m_view_index.begin());
}

std::vector<CursorDelta> Cursor::rebuild(std::size_t n)
{
    m_view_index.clear();
    for (std::size_t i = 0; i < n; ++i) {
        if (Accepts(m_fields(i), m_filter)) m_view_index.push_back(i);
    }
    std::sort(m_view_index.begin(), m_view_index.end(),
              [&](std::size_t a, std::size_t b) { return Less(m_keys(a), m_keys(b), m_sort_column, m_sort_order); });
    ++m_epoch;
    return {{CursorDelta::Reset, 0, static_cast<int>(servedCount())}};
}

// Served-window translation for a single-row INSERT that already happened at
// `slot` (m_view_index has grown by 1). Eviction when the window was full.
static void emitInsertAt(std::size_t slot, std::size_t size_after, std::size_t cap_v,
                         std::vector<CursorDelta>& out)
{
    const std::size_t served_before = std::min(size_after - 1, cap_v);
    const std::size_t served_after  = std::min(size_after, cap_v);
    if (slot < served_after) {
        out.push_back({CursorDelta::Insert, static_cast<int>(slot), 1});
        if (served_before == cap_v) {  // window was full → last visible row evicted
            out.push_back({CursorDelta::Remove, static_cast<int>(cap_v), 1});
        }
    }
}

// Served-window translation for a single-row REMOVE that already happened at
// `pos` (size_before = size before the erase). Promotion when an off-window row exists.
static void emitRemoveAt(std::size_t pos, std::size_t size_before, std::size_t cap_v,
                         std::vector<CursorDelta>& out)
{
    const std::size_t served_before = std::min(size_before, cap_v);
    if (pos < served_before) {
        out.push_back({CursorDelta::Remove, static_cast<int>(pos), 1});
        if (size_before > cap_v) {  // an off-window row promotes into the last visible slot
            out.push_back({CursorDelta::Insert, static_cast<int>(cap_v - 1), 1});
        }
    }
}

std::vector<CursorDelta> Cursor::applyStoreInsert(std::size_t P, std::size_t count)
{
    std::vector<CursorDelta> out;
    const std::size_t cap_v = cap();
    // (a) the table grew at P → every later absolute index shifted +count.
    for (auto& e : m_view_index) {
        if (e >= P) e += count;
    }
    // (b) insert each newly-accepted record at its sorted slot.
    for (std::size_t i = P; i < P + count; ++i) {
        if (Accepts(m_fields(i), m_filter)) {
            const std::size_t slot = lowerBoundSlot(i);
            m_view_index.insert(m_view_index.begin() + slot, i);
            emitInsertAt(slot, m_view_index.size(), cap_v, out);
        }
    }
    return out;
}

std::vector<CursorDelta> Cursor::applyStoreRemove(std::size_t P, std::size_t count)
{
    std::vector<CursorDelta> out;
    const std::size_t cap_v = cap();
    // (a) erase the entries whose absolute value is in [P, P+count), by identity.
    for (std::size_t v = P; v < P + count; ++v) {
        const std::size_t pos = findSlot(v);
        if (pos != NPOS) {
            const std::size_t size_before = m_view_index.size();
            m_view_index.erase(m_view_index.begin() + pos);
            emitRemoveAt(pos, size_before, cap_v, out);
        }
    }
    // (b) the table shrank by `count` at P → later survivors shift -count.
    for (auto& e : m_view_index) {
        if (e >= P + count) e -= count;
    }
    return out;
}

std::vector<CursorDelta> Cursor::applyStatusUpdate(std::size_t P)
{
    std::vector<CursorDelta> out;
    const std::size_t cap_v = cap();
    const std::size_t old_pos = findSlot(P);          // identity (Item 1)
    const bool old_in = old_pos != NPOS;
    const bool new_in = Accepts(m_fields(P), m_filter);

    if (!old_in && !new_in) {
        return out;                                   // not shown before or after
    }
    if (!old_in && new_in) {                          // membership flip-in
        const std::size_t slot = lowerBoundSlot(P);
        m_view_index.insert(m_view_index.begin() + slot, P);
        emitInsertAt(slot, m_view_index.size(), cap_v, out);
        return out;
    }
    if (old_in && !new_in) {                          // membership flip-out
        const std::size_t size_before = m_view_index.size();
        m_view_index.erase(m_view_index.begin() + old_pos);
        emitRemoveAt(old_pos, size_before, cap_v, out);
        return out;
    }

    // old_in && new_in: stays a member; may reposition. Erase-then-lower_bound
    // (Item 2). Total size is unchanged, so no membership promote/evict — only a
    // possible boundary CROSSING of the moved row itself.
    const std::size_t size_full = m_view_index.size();
    m_view_index.erase(m_view_index.begin() + old_pos);
    const std::size_t new_slot = lowerBoundSlot(P);
    m_view_index.insert(m_view_index.begin() + new_slot, P);

    const std::size_t served_full = std::min(size_full, cap_v);
    if (new_slot == old_pos) {                        // unchanged slot → just re-read
        if (old_pos < served_full) out.push_back({CursorDelta::Change, static_cast<int>(old_pos), 1});
        return out;
    }
    const bool old_vis = old_pos < served_full;
    const bool new_vis = new_slot < served_full;
    if (old_vis && new_vis) {                          // reorder within the window
        out.push_back({CursorDelta::Remove, static_cast<int>(old_pos), 1});
        out.push_back({CursorDelta::Insert, static_cast<int>(new_slot), 1});
    } else if (old_vis && !new_vis) {                  // moved out of the window
        out.push_back({CursorDelta::Remove, static_cast<int>(old_pos), 1});
        if (size_full > cap_v) out.push_back({CursorDelta::Insert, static_cast<int>(served_full - 1), 1});
    } else if (!old_vis && new_vis) {                  // moved into the window
        out.push_back({CursorDelta::Insert, static_cast<int>(new_slot), 1});
        if (size_full > cap_v) out.push_back({CursorDelta::Remove, static_cast<int>(served_full), 1});
    }
    // !old_vis && !new_vis: off-window move, no emission (view_index still maintained).
    return out;
}

std::vector<CursorDelta> Cursor::setLimit(int new_limit)
{
    std::vector<CursorDelta> out;
    const std::size_t served_old = servedCount();
    m_filter.limit_rows = new_limit;
    const std::size_t served_new = servedCount();
    if (served_new > served_old) {
        out.push_back({CursorDelta::Insert, static_cast<int>(served_old), static_cast<int>(served_new - served_old)});
    } else if (served_new < served_old) {
        out.push_back({CursorDelta::Remove, static_cast<int>(served_new), static_cast<int>(served_old - served_new)});
    }
    return out;
}

std::vector<CursorDelta> Cursor::setSort(int sort_column, int sort_order)
{
    m_sort_column = sort_column;
    m_sort_order = sort_order;
    std::sort(m_view_index.begin(), m_view_index.end(),
              [&](std::size_t a, std::size_t b) { return Less(m_keys(a), m_keys(b), m_sort_column, m_sort_order); });
    ++m_epoch;
    return {{CursorDelta::Reset, 0, static_cast<int>(servedCount())}};
}

std::vector<CursorDelta> Cursor::setFilter(const FilterSpec& filter, std::size_t n)
{
    m_filter = filter;
    return rebuild(n);
}

} // namespace GRC
