// Copyright (c) 2021 The Bitcoin Core developers
// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_TRANSACTION_H
#define BITCOIN_WALLET_TRANSACTION_H

#include "uint256.h"
#include <variant>

/**
 * Transaction State System (Issue #1157)
 *
 * Replaces legacy hashBlock-based implicit state with explicit variant types.
 * State transitions:
 *
 *   [New Tx] -> TxStateInMempool -> TxStateConfirmed <-> TxStateInMempool (reorg)
 *                     |                    |
 *                     +-----> TxStateInactive <------+
 *
 * Serialization uses the existing CMerkleTx hashBlock/nIndex fields with
 * sentinel values for backward compatibility (no envelope format).
 */

//! Transaction is in the mempool (unconfirmed).
struct TxStateInMempool {
    TxStateInMempool() = default;
};

//! Transaction is confirmed in a block on the active chain.
struct TxStateConfirmed {
    uint256 m_confirmed_block_hash;
    int m_confirmed_block_height;
    int m_position_in_block;

    TxStateConfirmed()
        : m_confirmed_block_hash()
        , m_confirmed_block_height(-1)
        , m_position_in_block(-1) {}

    TxStateConfirmed(const uint256& hash, int height, int pos)
        : m_confirmed_block_hash(hash)
        , m_confirmed_block_height(height)
        , m_position_in_block(pos) {}
};

//! Transaction is inactive: either conflicted (abandoned=false) or
//! explicitly abandoned by user (abandoned=true).
struct TxStateInactive {
    bool m_abandoned;

    TxStateInactive() : m_abandoned(false) {}
    explicit TxStateInactive(bool _abandoned) : m_abandoned(_abandoned) {}
};

//! Transaction state unrecognized (loading old wallet or future format).
//! Temporary — resolved to a proper state during ReacceptWalletTransactions.
struct TxStateUnrecognized {
    uint256 m_block_hash;  //!< Legacy hashBlock value, if present
    int m_index;           //!< Legacy nIndex value, if present

    TxStateUnrecognized() : m_index(-1) {}
};

/**
 * Variant holding the current transaction state.
 * Index order is fixed: 0=InMempool, 1=Confirmed, 2=Inactive, 3=Unrecognized.
 */
using TxState = std::variant<
    TxStateInMempool,
    TxStateConfirmed,
    TxStateInactive,
    TxStateUnrecognized
>;

// ---------------------------------------------------------------------------
// Legacy hashBlock sentinel values for backward-compatible serialization
// ---------------------------------------------------------------------------

//! Sentinel hash used in legacy format to mark abandoned transactions.
inline const uint256 ABANDONED_HASH_SENTINEL =
    uint256S("0000000000000000000000000000000000000000000000000000000000000001");

//! Sentinel hash used in legacy format to mark conflicted transactions.
inline const uint256 CONFLICTED_HASH_SENTINEL =
    uint256S("0000000000000000000000000000000000000000000000000000000000000002");

/**
 * Migrate a legacy hashBlock/nIndex pair to a TxState.
 * Used when reading old wallet.dat entries that predate the state system.
 */
inline TxState MigrateFromLegacyHashBlock(const uint256& hashBlock, int nIndex)
{
    if (hashBlock == ABANDONED_HASH_SENTINEL)  return TxStateInactive{true};
    if (hashBlock == CONFLICTED_HASH_SENTINEL) return TxStateInactive{false};
    if (!hashBlock.IsNull())                   return TxStateConfirmed(hashBlock, -1, nIndex);
    return TxStateUnrecognized{};
}

// ---------------------------------------------------------------------------
// Sentinel-based serialization helpers
//
// These extract the hashBlock and nIndex values that CMerkleTx writes to disk.
// On read, MigrateFromLegacyHashBlock() reconstructs m_state from those fields.
// This approach is backward-compatible with all existing wallet.dat files.
// ---------------------------------------------------------------------------

//! Return the hashBlock value to serialize for a given TxState.
inline uint256 TxStateSerializedBlockHash(const TxState& state)
{
    return std::visit([](const auto& s) -> uint256 {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, TxStateConfirmed>) {
            return s.m_confirmed_block_hash;
        } else if constexpr (std::is_same_v<T, TxStateInactive>) {
            return s.m_abandoned ? ABANDONED_HASH_SENTINEL : CONFLICTED_HASH_SENTINEL;
        } else {
            return uint256{};
        }
    }, state);
}

//! Return the nIndex value to serialize for a given TxState.
inline int TxStateSerializedIndex(const TxState& state)
{
    return std::visit([](const auto& s) -> int {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, TxStateConfirmed>) {
            return s.m_position_in_block;
        } else {
            return -1;
        }
    }, state);
}

#endif // BITCOIN_WALLET_TRANSACTION_H
