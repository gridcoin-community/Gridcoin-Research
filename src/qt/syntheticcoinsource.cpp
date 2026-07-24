// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/syntheticcoinsource.h"

#include "amount.h"
#include "arith_uint256.h"
#include "qt/guilog.h"

#include <algorithm>
#include <utility>

namespace {

uint256 SynthHash(int n)
{
    return ArithToUint256(arith_uint256(static_cast<uint64_t>(n) + 1));
}

} // anonymous namespace

SyntheticCoinSource::SyntheticCoinSource(int total_coins, int groups)
    : m_store(nullptr, m_queue)
{
    m_store.start();

    total_coins = std::max(total_coins, 1);
    groups = std::max(groups, 1);

    // Group 0 is the pathological single-address case; the rest get 1000
    // coins each (or an even split when the total is small).
    const int small_group = std::min(1000, total_coins / std::max(groups, 1));
    const int whale = total_coins - (groups - 1) * small_group;

    std::vector<GRC::CoinRecord> records;
    records.reserve(static_cast<std::size_t>(total_coins));

    int seed = 0;
    for (int g = 0; g < groups; ++g) {
        const std::string address = (g == 0)
            ? std::string("SynthWhale000000000000000000000000")
            : std::string("SynthGroup") + std::string(1, static_cast<char>('A' + (g - 1) % 26))
                  + "00000000000000000000000";
        const int count = (g == 0) ? whale : small_group;
        for (int i = 0; i < count; ++i, ++seed) {
            GRC::CoinRecord r;
            r.outpoint = COutPoint(SynthHash(seed), 0);
            // Varied amounts (deterministic, non-monotonic) so sorts are
            // non-trivial; ~0.0001 to ~100 GRC.
            r.amount = static_cast<int64_t>((static_cast<int64_t>(seed) * 7919) % 1000000 + 1)
                       * (COIN / 10000);
            r.address = address;
            r.group_address = address;
            r.label = (g == 0) ? "synthetic whale" : "";
            r.time = 1700000000 + seed;
            r.block_height = 100 + (seed % 100000);
            r.is_change = false;
            records.push_back(std::move(r));
        }
    }

    m_store.seedSynthetic(std::move(records), /*tip_height=*/300000);

    GUILogPrintf("SyntheticCoinSource: seeded %d coins over %d groups (whale group: %d)",
                 total_coins, groups, whale);
}

void SyntheticCoinSource::registerView(int view_id, GRC::CoinViewMode mode,
                                       int sort_column, int sort_order)
{
    m_store.registerView(view_id, mode, sort_column, sort_order);
}

void SyntheticCoinSource::unregisterView(int view_id) { m_store.unregisterView(view_id); }

void SyntheticCoinSource::setViewMode(int view_id, GRC::CoinViewMode mode)
{
    m_store.setViewMode(view_id, mode);
}

void SyntheticCoinSource::setViewSort(int view_id, int sort_column, int sort_order)
{
    m_store.setViewSort(view_id, sort_column, sort_order);
}

GRC::CoinRowsResult SyntheticCoinSource::getRows(int view_id, int first, int count)
{
    return m_store.getRows(view_id, first, count);
}

GRC::CoinGroupsResult SyntheticCoinSource::getGroups(int view_id, int first, int count)
{
    return m_store.getGroups(view_id, first, count);
}

GRC::CoinRowsResult SyntheticCoinSource::getGroupRows(int view_id,
                                                      const std::string& group_address,
                                                      int first, int count)
{
    return m_store.getGroupRows(view_id, group_address, first, count);
}

std::vector<GRC::CoinGroupInfo> SyntheticCoinSource::getGroupDirectory()
{
    return m_store.getGroupDirectory();
}

std::set<COutPoint> SyntheticCoinSource::reconcileSelection(std::set<COutPoint> selection)
{
    return m_store.reconcileSelection(std::move(selection));
}

GRC::CoinSelectionUpdate SyntheticCoinSource::setSelected(const COutPoint& outpoint, bool selected)
{
    return m_store.setSelected(outpoint, selected);
}

GRC::CoinBulkSelectionResult SyntheticCoinSource::selectGroup(const std::string& group_address,
                                                              bool selected)
{
    return m_store.selectGroup(group_address, selected);
}

GRC::CoinBulkSelectionResult SyntheticCoinSource::selectAll(bool selected)
{
    return m_store.selectAll(selected);
}

GRC::CoinBulkSelectionResult SyntheticCoinSource::applyValueFilter(bool less_or_equal,
                                                                   int64_t value,
                                                                   uint32_t max_inputs)
{
    return m_store.applyValueFilter(less_or_equal, value, max_inputs);
}

GRC::CoinGroupsResult SyntheticCoinSource::reloadAndSnapshot()
{
    // The synthetic snapshot was installed at construction; the load thread's
    // call is a harmless no-op that reports the directory.
    GRC::CoinGroupsResult result;
    result.groups = m_store.getGroupDirectory();
    result.total_groups = static_cast<int>(result.groups.size());
    return result;
}

std::vector<GRC::WalletCoinEvent> SyntheticCoinSource::drainEvents(std::size_t max_batch)
{
    return m_queue.drain(max_batch);
}

bool SyntheticCoinSource::consumeNeedsResync() { return m_store.consumeNeedsResync(); }

void SyntheticCoinSource::noteAddressBookChanged(const std::string&, const std::string&)
{
    // Synthetic records have no address book.
}
