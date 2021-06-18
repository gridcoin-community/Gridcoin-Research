// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/status.h"
#include "ui_interface.h"
#include "util.h"
#include "wallet/wallet.h"

using namespace GRC;
using SearchReport = MinerStatus::SearchReport;
using EfficiencyReport = MinerStatus::EfficiencyReport;

extern bool fQtActive;

// -----------------------------------------------------------------------------
// Global Variables
// -----------------------------------------------------------------------------

MinerStatus g_miner_status;

// -----------------------------------------------------------------------------
// Class: MinerStatus
// -----------------------------------------------------------------------------

MinerStatus::MinerStatus() : m_error_flags(NONE)
{
}

std::string MinerStatus::FormatErrors(ErrorFlags error_flags)
{
    std::string errors;

    if (error_flags == NONE) {
        return errors;
    }

    for (size_t i = NONE + 1; i < OUT_OF_BOUND; i <<= 1) {
        if (!(error_flags & i)) {
            continue;
        }

        if (!errors.empty()) {
            errors += "; ";
        }

        switch (i) {
            case DISABLED_BY_CONFIGURATION:
                // When disabled, skip any remaining alerts:
                errors  = _("Disabled by configuration"); return errors;
            case NO_MATURE_COINS:
                errors += _("No mature coins");           break;
            case NO_COINS:
                errors += _("No coins");                  break;
            case ENTIRE_BALANCE_RESERVED:
                errors += _("Entire balance reserved");   break;
            case NO_UTXOS_AVAILABLE_DUE_TO_RESERVE:
                errors += _("No UTXOs available due to reserve balance"); break;
            case WALLET_LOCKED:
                errors += _("Wallet locked");             break;
            case TESTNET_ONLY:
                errors += _("Testnet-only version");      break;
            case OFFLINE:
                errors += _("Offline");                   break;
        }
    }

    return errors;
}

bool MinerStatus::StakingEnabled() const
{
    return m_error_flags == NONE || m_error_flags == NO_MATURE_COINS;
}

bool MinerStatus::StakingActive() const
{
    LOCK(m_cs);

    return m_search.m_timestamp != 0 && m_search.m_weight_sum != 0;
}

SearchReport MinerStatus::GetSearchReport() const
{
    LOCK(m_cs);

    return m_search;
}

EfficiencyReport MinerStatus::GetEfficiencyReport() const
{
    LOCK(m_cs);

    return m_efficiency;
}

std::optional<CWalletTx> MinerStatus::GetLastStake(CWallet& wallet)
{
    CWalletTx stake_tx;
    uint256 cached_stake_tx_hash;

    {
        LOCK(m_cs);
        cached_stake_tx_hash = m_last_pos_tx_hash;
    }

    if (!cached_stake_tx_hash.IsNull()) {
        if (wallet.GetTransaction(cached_stake_tx_hash, stake_tx)) {
            return stake_tx;
        }
    }

    const auto is_my_confirmed_stake = [](const CWalletTx& tx) {
        return tx.IsCoinStake() && tx.IsFromMe() && tx.GetDepthInMainChain() > 0;
    };

    {
        LOCK2(cs_main, wallet.cs_wallet);

        if (wallet.mapWallet.empty()) {
            return std::nullopt;
        }

        auto latest_iter = wallet.mapWallet.cbegin();

        for (auto iter = latest_iter; iter != wallet.mapWallet.cend(); ++iter) {
            if (iter->second.nTime > latest_iter->second.nTime
                && is_my_confirmed_stake(iter->second))
            {
                latest_iter = iter;
            }
        }

        if (latest_iter == wallet.mapWallet.cbegin()
            && !is_my_confirmed_stake(latest_iter->second))
        {
            return std::nullopt;
        }

        cached_stake_tx_hash = latest_iter->first;
        stake_tx = latest_iter->second;
    }

    {
        LOCK(m_cs);
        m_last_pos_tx_hash = cached_stake_tx_hash;
    }

    return stake_tx;
}

std::string MinerStatus::FormatErrors() const
{
    return FormatErrors(m_error_flags);
}

void MinerStatus::AddError(ErrorFlags error_flag)
{
    m_error_flags = static_cast<ErrorFlags>(m_error_flags | error_flag);
}

void MinerStatus::UpdateCurrentErrors(ErrorFlags error_flags)
{
    ClearErrors();
    AddError(error_flags);

    // Avoid the lock on headless nodes:
    if (fQtActive) {
        LOCK(m_cs);
        uiInterface.MinerStatusChanged(StakingActive(), m_search.CoinWeight());
    }
}

void MinerStatus::IncrementBlocksCreated()
{
    LOCK(m_cs);

    ++m_search.m_blocks_created;
}

void MinerStatus::UpdateLastSearch(
    bool kernel_found,
    int64_t search_timestamp,
    int block_version,
    uint64_t weight_sum,
    double value_sum,
    uint64_t weight_min,
    uint64_t weight_max,
    int64_t balance_weight)
{
    LOCK(m_cs);

    if (kernel_found) {
        ++m_search.m_kernels_found;
    }

    m_search.m_block_version = block_version;
    m_search.m_weight_sum = weight_sum;
    m_search.m_value_sum = value_sum;
    m_search.m_weight_min = weight_min;
    m_search.m_weight_max = weight_max;

    // search_timestamp has already been granularized to the stake time mask.
    // The StakeMiner loop has a sleep of 8 seconds. You can have no more than
    // one successful stake in STAKE_TIMESTAMP_MASK, so only increment the
    // counter if this iteration is with a search_timestamp in the next mask
    // interval. (When the UTXO count is low, with a sleep of 8 seconds, and a
    // nominal mask of 16 seconds, many times two stake UTXO loop traversals
    // will occur during the 16 seconds. Only one will result in a possible
    // stake.)
    //
    if (search_timestamp > m_search.m_timestamp) {
        m_efficiency.UpdateMetrics(weight_sum, balance_weight);
    }

    m_search.m_timestamp = search_timestamp;

    uiInterface.MinerStatusChanged(true, m_search.CoinWeight());
}

void MinerStatus::UpdateLastStake(const uint256& coinstake_hash)
{
    LOCK(m_cs);

    m_search.m_blocks_accepted++;
    m_last_pos_tx_hash = coinstake_hash;
}

void MinerStatus::ClearLastSearch()
{
    LOCK(m_cs);

    m_search.m_block_version = 0;
    m_search.m_weight_sum = 0;
    m_search.m_value_sum = 0;
    m_search.m_weight_min = 0;
    m_search.m_weight_max = 0;
    m_search.m_timestamp = 0;

    uiInterface.MinerStatusChanged(false, 0);
}

void MinerStatus::ClearLastStake()
{
    LOCK(m_cs);

    m_last_pos_tx_hash.SetNull();
}

void MinerStatus::ClearErrors()
{
    m_error_flags = NONE;
}

// -----------------------------------------------------------------------------
// Class: MinerStatus::SearchReport
// -----------------------------------------------------------------------------

double SearchReport::CoinWeight() const
{
    return m_weight_sum / 80.0;
}

size_t SearchReport::KernelsRejected() const
{
    return m_kernels_found - m_blocks_accepted;
}

// -----------------------------------------------------------------------------
// Class: MinerStatus::EfficiencyReport
// -----------------------------------------------------------------------------

double EfficiencyReport::StakingLoopEfficiency() const
{
    if (masked_time_intervals_elapsed <= 0) {
        return 0.0;
    }

    return masked_time_intervals_covered * 100.0 / masked_time_intervals_elapsed;
}

double EfficiencyReport::StakingEfficiency() const
{
    if (ideal_cumulative_weight <= 0.0) {
        return 0.0;
    }

    return actual_cumulative_weight * 100.0 / ideal_cumulative_weight;
}

void EfficiencyReport::UpdateMetrics(uint64_t weight_sum, int64_t balance_weight)
{
    const uint64_t prev_masked_time_intervals_elapsed = masked_time_intervals_elapsed;

    ++masked_time_intervals_covered;

    // This is effectively sum of the weight of each stake attempt added back
    // to itself for cumulative total stake weight. This is the total effective
    // weight for the run time of the miner.
    actual_cumulative_weight += weight_sum;

    // This is effectively the idealized weight equivalent of the balance times
    // the number of quantized (masked) staking periods that have elapsed since
    // the last trip through, added back for a cumulative total. The
    // calculation is done this way rather than just using the elapsed time to
    // ensure the masked time intervals are aligned in the calculations.
    const int64_t node_start_elapsed_seconds = g_timer.GetStartTime("default") / 1000;
    const int64_t now = GetTimeMillis() / 1000;
    const int64_t masked_time_elapsed = GRC::MaskStakeTime(now + GetTimeOffset())
        - GRC::MaskStakeTime(node_start_elapsed_seconds + GetTimeOffset());

    masked_time_intervals_elapsed = masked_time_elapsed / (GRC::STAKE_TIMESTAMP_MASK + 1);

    ideal_cumulative_weight += (double)balance_weight
        * (masked_time_intervals_elapsed - prev_masked_time_intervals_elapsed);

    // masked_time_intervals_covered / masked_time_intervals_elapsed provides a
    // measure of the miner loop efficiency.
    //
    // actual_cumulative_weight / ideal_cumulative_weight provides a measure of
    // the overall mining efficiency compared to ideal.
}
