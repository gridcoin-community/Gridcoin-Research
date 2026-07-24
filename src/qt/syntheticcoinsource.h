// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SYNTHETICCOINSOURCE_H
#define BITCOIN_QT_SYNTHETICCOINSOURCE_H

#include "interfaces/wallet_coin_source.h"
#include "wallet/wallet_event_queue.h"
#include "wallet/walletcoinstore.h"

#include <memory>

//!
//! \brief DEV HARNESS ONLY (-devsyntheticcoins=<n>[:<groups>]): an
//! interfaces::WalletCoinSource over a null-wallet GRC::WalletCoinStore
//! seeded with synthetic records — the #3183 acceptance-gate substitution.
//!
//! Because the REAL store, views and queue are underneath (only the wallet
//! scan is replaced by seedSynthetic), every windowing, epoch/floor and
//! selection-mirror semantic the production dialog exercises is exercised
//! here too, at whatever scale the argument requests. Group 0 receives the
//! bulk of the coins (the pathological single-address case); the remaining
//! groups get 1000 each.
//!
//! Limits vs production: the summary labels read 0 (computeCoinControlSummary
//! resolves outpoints against the real wallet), and there is no live mutation
//! feed — this harness is for model/view behavior and the measured 500k-child
//! expand gate, not fee math.
//!
class SyntheticCoinSource : public interfaces::WalletCoinSource
{
public:
    SyntheticCoinSource(int total_coins, int groups);
    ~SyntheticCoinSource() override = default;

    void registerView(int view_id, GRC::CoinViewMode mode,
                      int sort_column, int sort_order) override;
    void unregisterView(int view_id) override;
    void setViewMode(int view_id, GRC::CoinViewMode mode) override;
    void setViewSort(int view_id, int sort_column, int sort_order) override;
    GRC::CoinRowsResult getRows(int view_id, int first, int count) override;
    GRC::CoinGroupsResult getGroups(int view_id, int first, int count) override;
    GRC::CoinRowsResult getGroupRows(int view_id, const std::string& group_address,
                                     int first, int count) override;
    std::vector<GRC::CoinGroupInfo> getGroupDirectory() override;
    std::set<COutPoint> reconcileSelection(std::set<COutPoint> selection) override;
    GRC::CoinSelectionUpdate setSelected(const COutPoint& outpoint, bool selected) override;
    GRC::CoinBulkSelectionResult selectGroup(const std::string& group_address,
                                             bool selected) override;
    GRC::CoinBulkSelectionResult selectAll(bool selected) override;
    GRC::CoinBulkSelectionResult applyValueFilter(bool less_or_equal, int64_t value,
                                                  uint32_t max_inputs) override;
    GRC::CoinGroupsResult reloadAndSnapshot() override;
    std::vector<GRC::WalletCoinEvent> drainEvents(std::size_t max_batch) override;
    bool consumeNeedsResync() override;
    void noteAddressBookChanged(const std::string& address, const std::string& label) override;

private:
    GRC::WalletCoinEventQueue m_queue;
    GRC::WalletCoinStore m_store;
};

#endif // BITCOIN_QT_SYNTHETICCOINSOURCE_H
