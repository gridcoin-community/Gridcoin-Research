// Copyright (c) 2021 The Bitcoin Core developers
// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_TRANSACTION_H
#define BITCOIN_WALLET_TRANSACTION_H

#include "uint256.h"
#include "serialize.h"
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
 * Variant index order is fixed for serialization compatibility.
 */

//! Transaction is in the mempool (unconfirmed).
struct TxStateInMempool {
    TxStateInMempool() = default;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) { }
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

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_confirmed_block_hash);
        READWRITE(m_confirmed_block_height);
        READWRITE(m_position_in_block);
    }
};

//! Transaction is inactive: either conflicted (abandoned=false) or
//! explicitly abandoned by user (abandoned=true).
struct TxStateInactive {
    bool m_abandoned;

    TxStateInactive() : m_abandoned(false) {}
    explicit TxStateInactive(bool _abandoned) : m_abandoned(_abandoned) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_abandoned);
    }
};

//! Transaction state unrecognized (loading old wallet or future format).
//! Temporary — resolved to a proper state during ReacceptWalletTransactions.
struct TxStateUnrecognized {
    uint256 m_block_hash;  //!< Legacy hashBlock value, if present
    int m_index;           //!< Legacy nIndex value, if present

    TxStateUnrecognized() : m_index(-1) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_block_hash);
        READWRITE(m_index);
    }
};

/**
 * Variant holding the current transaction state.
 * Index order is fixed: 0=InMempool, 1=Confirmed, 2=Inactive, 3=Unrecognized.
 * Do NOT reorder without migration logic — it would break wallet.dat compat.
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
// Variant serialization helpers
// ---------------------------------------------------------------------------

template<typename Stream>
void SerializeTxState(Stream& s, const TxState& state)
{
    int state_index = state.index();
    ::Serialize(s, state_index);
    std::visit([&s](const auto& st) { ::Serialize(s, st); }, state);
}

template<typename Stream>
void UnserializeTxState(Stream& s, TxState& state)
{
    int state_index;
    ::Unserialize(s, state_index);

    switch (state_index) {
        case 0: { TxStateInMempool st;     ::Unserialize(s, st); state = st; break; }
        case 1: { TxStateConfirmed st;     ::Unserialize(s, st); state = st; break; }
        case 2: { TxStateInactive st;      ::Unserialize(s, st); state = st; break; }
        case 3: { TxStateUnrecognized st;  ::Unserialize(s, st); state = st; break; }
        default: {
            // Forward compatibility: unknown state type → unrecognized.
            // NOTE: No LogPrint here to avoid heavy logging.h include in this header.
            // Callers that care can check for TxStateUnrecognized after deserialization.
            TxStateUnrecognized st;
            try {
                ::Unserialize(s, st);
            } catch (...) {
                st = TxStateUnrecognized{};
            }
            state = st;
            break;
        }
    }
}

template<typename Stream>
void Serialize(Stream& s, const TxState& state) { SerializeTxState(s, state); }

template<typename Stream>
void Unserialize(Stream& s, TxState& state) { UnserializeTxState(s, state); }

#endif // BITCOIN_WALLET_TRANSACTION_H
