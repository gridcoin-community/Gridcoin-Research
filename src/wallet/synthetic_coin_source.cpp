// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/wallet_coin_source.h"

#include "amount.h"
#include "arith_uint256.h"
#include "logging.h"
#include "tinyformat.h"
#include "wallet/wallet_event_queue.h"
#include "wallet/walletcoinstore.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace interfaces {
namespace {

uint256 SynthHash(int n)
{
    return ArithToUint256(arith_uint256(static_cast<uint64_t>(n) + 1));
}

//! DEV HARNESS ONLY: a WalletCoinSource over a null-wallet GRC::WalletCoinStore
//! seeded with synthetic records. See MakeSyntheticCoinSource() in
//! interfaces/wallet_coin_source.h for the contract and its limits.
class SyntheticCoinSourceImpl : public WalletCoinSource
{
public:
    SyntheticCoinSourceImpl(int total_coins, int groups);
    ~SyntheticCoinSourceImpl() override = default;

    SyntheticCoinSourceImpl(const SyntheticCoinSourceImpl&) = delete;
    SyntheticCoinSourceImpl& operator=(const SyntheticCoinSourceImpl&) = delete;

    void registerView(int view_id, GRC::CoinViewMode mode,
                      int sort_column, int sort_order) override
    {
        m_store.registerView(view_id, mode, sort_column, sort_order);
    }

    void unregisterView(int view_id) override { m_store.unregisterView(view_id); }

    void setViewMode(int view_id, GRC::CoinViewMode mode) override
    {
        m_store.setViewMode(view_id, mode);
    }

    void setViewSort(int view_id, int sort_column, int sort_order) override
    {
        m_store.setViewSort(view_id, sort_column, sort_order);
    }

    GRC::CoinRowsResult getRows(int view_id, int first, int count) override
    {
        return m_store.getRows(view_id, first, count);
    }

    GRC::CoinGroupsResult getGroups(int view_id, int first, int count) override
    {
        return m_store.getGroups(view_id, first, count);
    }

    GRC::CoinRowsResult getGroupRows(int view_id, const std::string& group_address,
                                     int first, int count) override
    {
        return m_store.getGroupRows(view_id, group_address, first, count);
    }

    std::vector<GRC::CoinGroupInfo> getGroupDirectory() override
    {
        return m_store.getGroupDirectory();
    }

    std::set<COutPoint> reconcileSelection(std::set<COutPoint> selection) override
    {
        return m_store.reconcileSelection(std::move(selection));
    }

    GRC::CoinSelectionUpdate setSelected(const COutPoint& outpoint, bool selected) override
    {
        return m_store.setSelected(outpoint, selected);
    }

    GRC::CoinBulkSelectionResult selectGroup(const std::string& group_address,
                                             bool selected) override
    {
        return m_store.selectGroup(group_address, selected);
    }

    GRC::CoinBulkSelectionResult selectAll(bool selected) override
    {
        return m_store.selectAll(selected);
    }

    GRC::CoinBulkSelectionResult applyValueFilter(bool less_or_equal, int64_t value,
                                                  uint32_t max_inputs) override
    {
        return m_store.applyValueFilter(less_or_equal, value, max_inputs);
    }

    GRC::CoinGroupsResult reloadAndSnapshot() override
    {
        // The synthetic snapshot was installed at construction; the load
        // thread's call is a harmless no-op that reports the directory.
        GRC::CoinGroupsResult result;
        result.groups = m_store.getGroupDirectory();
        result.total_groups = static_cast<int>(result.groups.size());
        return result;
    }

    std::vector<GRC::WalletCoinEvent> drainEvents(std::size_t max_batch) override
    {
        return m_queue.drain(max_batch);
    }

    bool consumeNeedsResync() override { return m_store.consumeNeedsResync(); }

    void noteAddressBookChanged(const std::string&, const std::string&) override
    {
        // Synthetic records have no address book.
    }

private:
    GRC::WalletCoinEventQueue m_queue;
    GRC::WalletCoinStore m_store;
};

SyntheticCoinSourceImpl::SyntheticCoinSourceImpl(int total_coins, int groups)
    : m_store(nullptr, m_queue)
{
    m_store.start();

    total_coins = std::max(total_coins, 1);
    groups = std::max(groups, 1);

    // CLAMP to <total_coins>. Injective addresses are necessary for the
    // directory to reach <groups> rows, but not sufficient: small_group is an
    // integer division, so once <groups> exceeds <total_coins> it is 0, every
    // non-whale group seeds no records, and buildGroups() -- which only ever
    // sees records -- never gives them a row. -devsyntheticcoins=100:500 built
    // a ONE-row directory and reported 500.
    //
    // That is the same silent shortfall as the aliasing bug below, one regime
    // over, so it is fixed the same way: by making the advertised contract
    // true rather than by reporting its violation. A group count above the
    // coin count has no meaning here -- there are not enough coins to put one
    // in each group -- so clamping loses nothing.
    const int requested_groups = groups;
    groups = std::min(groups, total_coins);

    // Group 0 is the pathological single-address case; the rest get up to
    // 1000 coins each, falling to an even split once <groups> is large
    // relative to <total_coins> (and to 1 apiece when they are equal, which
    // post-clamp is the floor: small_group is now always >= 1).
    const int small_group = std::min(1000, total_coins / std::max(groups, 1));
    const int whale = total_coins - (groups - 1) * small_group;

    std::vector<GRC::CoinRecord> records;
    records.reserve(static_cast<std::size_t>(total_coins));

    int seed = 0;
    for (int g = 0; g < groups; ++g) {
        // The group index is rendered in full, zero-padded to the same
        // 34-character width as the whale address. It has to stay
        // injective: an aliasing scheme (a single letter mod 26, say)
        // collapses the directory onto 26 addresses no matter what
        // <groups> asks for, because buildGroups() keys on group_address
        // -- which silently caps the many-groups axis this harness exists
        // to exercise.
        const std::string address = (g == 0)
            ? std::string("SynthWhale000000000000000000000000")
            : strprintf("SynthGroup%024d", g);
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

    if (requested_groups != groups) {
        LogPrintf("SyntheticCoinSource: clamped %d requested groups to %d, the coin count "
                  "(a group per coin is the most that can be seeded)",
                  requested_groups, groups);
    }

    // Reports the shape actually seeded: <groups> is the clamped value, so
    // this line can no longer advertise a directory the store does not hold.
    LogPrintf("SyntheticCoinSource: seeded %d coins over %d groups (whale group: %d)",
              total_coins, groups, whale);
}

} // namespace

std::shared_ptr<WalletCoinSource> MakeSyntheticCoinSource(int total_coins, int groups)
{
    return std::make_shared<SyntheticCoinSourceImpl>(total_coins, groups);
}

} // namespace interfaces
