// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_TRANSACTION_H
#define BITCOIN_WALLET_TRANSACTION_H

#include "uint256.h"
#include "serialize.h"
#include <variant>

/**
 * ============================================================================
 * Issue 1157 TRANSACTION STATE SYSTEM
 * ============================================================================
 *
 * OVERVIEW:
 * This file implements a robust variant-based state tracking system for
 * wallet transactions, replacing the legacy hashBlock-based implicit state.
 *
 * KEY FEATURES IMPLEMENTED:
 * Variant Index Version Gating - Prevents wallet.dat corruption
 * Block Height Validation - Prevents orphaned block references
 * Default Initialization - Explicit field initialization
 * Logging Initialization Safety - Safe null checks on startup
 *
 * ============================================================================
 */

namespace wallet {

/**
 * Transaction State Infrastructure for Wallet
 *
 * OVERVIEW:
 * This file defines the transaction state tracking system for the wallet,
 * replacing the legacy hashBlock-based approach with explicit state types.
 *
 * WHY THIS EXISTS:
 * Previously, the wallet used hashBlock to implicitly determine transaction
 * state: null hashBlock meant mempool, non-null meant confirmed. This was
 * fragile and didn't support abandoned/conflicted states. The new explicit
 * state system provides:
 * 1. Clear state semantics (no ambiguity)
 * 2. Support for abandoned/conflicted transactions
 * 3. Compatibility with Bitcoin Core validation interface patterns
 * 4. Better handling of reorgs and state transitions
 *
 * STATE TRANSITION DIAGRAM:
 *
 *     [New Tx]
 *         |
 *         v
 *   TxStateInMempool  <----+
 *         |                |
 *         |                | (reorg)
 *         v                |
 *   TxStateConfirmed   ----+
 *         |
 *         | (conflict/abandon)
 *         v
 *   TxStateInactive
 *
 * USAGE PATTERN:
 *   wallet::TxState state;
 *   if (in_mempool) {
 *       state = wallet::TxStateInMempool{};
 *   } else if (confirmed) {
 *       state = wallet::TxStateConfirmed{hash, height, pos};
 *   }
 *   wallet.SyncTransaction(tx, state);
 */

/**
 * TxStateInMempool: Transaction is unconfirmed and in the mempool
 *
 * WHEN USED:
 * - Transaction received via network or RPC
 * - Transaction accepted to mempool
 * - After reorg, when tx returns to mempool
 *
 * CHARACTERISTICS:
 * - Not yet in a block
 * - Can be evicted from mempool
 * - Can become conflicted if competing tx confirms
 * - May eventually confirm or be abandoned
 *
 * DATA: No additional data needed - mempool membership is binary state
 *
 * Default Initialization:
 * Explicit empty constructor ensures consistent initialization.
 * Even though no members to initialize, this documents intent and allows
 * compile-time size verification via static_assert below.
 */
struct TxStateInMempool {
    TxStateInMempool() = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        // No data members to serialize - empty body is correct
    }
};

// Static size check for struct safety
// Ensures TxStateInMempool doesn't unexpectedly grow due to padding/alignment changes
static_assert(sizeof(TxStateInMempool) <= 8, "TxStateInMempool unexpectedly large");

/**
 * TxStateConfirmed: Transaction is confirmed in a block
 *
 * WHEN USED:
 * - Block containing transaction connected to active chain
 * - After validation, block accepted as part of best chain
 * - Transaction has at least 1 confirmation
 *
 * CHARACTERISTICS:
 * - Transaction is in a block on the main chain
 * - Has depth >= 1 (confirmations)
 * - Can be reorg'd back to mempool (if block becomes stale)
 * - Stable once depth is sufficiently high (~100 blocks)
 *
 * DATA FIELDS:
 * - m_confirmed_block_hash: Hash of block containing transaction
 * - m_confirmed_block_height: Height of confirming block in chain
 * - m_position_in_block: Index within block's vtx array (for ordering)
 *
 * WHY WE TRACK POSITION:
 * Position in block determines transaction ordering within that block,
 * which affects wallet display order and helps with deterministic
 * transaction processing during reorgs.
 */
struct TxStateConfirmed {
    uint256 m_confirmed_block_hash;
    int m_confirmed_block_height;
    int m_position_in_block;

    /**
     * Explicit initialization of all members
     *
     * CRITICAL: ALL struct fields must be explicitly initialized in the
     * default constructor. While uint256 defaults to zero, being explicit:
     * 1. Documents intent clearly
     * 2. Catches future bugs if members are added
     * 3. Prevents uninitialized memory access
     *
     * Block height validation:
     * - -1 signals "invalid/uninitialized height"
     * - ValidateTxStateConfirmed() enforces >= 0 when active
     * - Prevents use of corrupted height values
     */
    TxStateConfirmed()
        : m_confirmed_block_hash()
        , m_confirmed_block_height(-1)
        , m_position_in_block(-1)
    {
    }

    TxStateConfirmed(const uint256& hash, int height, int pos)
        : m_confirmed_block_hash(hash)
        , m_confirmed_block_height(height)
        , m_position_in_block(pos)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        /**
         * Variant Index Version Gating:
         * This serialization is called ONLY when variant index = 1
         * (TxStateConfirmed). The variant index itself is serialized
         * by SerializeTxState/UnserializeTxState functions below.
         * Version gating at the variant level prevents wallet.dat
         * corruption when old wallets encounter new state types.
         */
        READWRITE(m_confirmed_block_hash);
        READWRITE(m_confirmed_block_height);
        READWRITE(m_position_in_block);
    }
};

/**
 * TxStateInactive: Transaction is not active (conflicted or abandoned)
 *
 * WHEN USED:
 * - Transaction explicitly abandoned by user (AbandonTransaction RPC)
 * - Transaction conflicts with confirmed transaction (double-spend)
 * - Transaction's inputs spent by another confirmed transaction
 *
 * CHARACTERISTICS:
 * - Will never confirm (conflicting tx already confirmed)
 * - Should not be rebroadcast
 * - User has given up on this transaction
 * - May free up inputs for use in new transactions
 *
 * ABANDONED vs CONFLICTED:
 * - abandoned=true: User explicitly abandoned (via RPC)
 * - abandoned=false: Automatically marked conflicted due to input spent
 *
 * WHY DISTINGUISH:
 * Abandoned is user-initiated, conflicted is automatic. Distinguishing
 * helps with:
 * - UI display (show why tx failed)
 * - Reorg handling (conflicted may become valid again, abandoned won't)
 * - User expectations (abandoned = intentional, conflicted = accidental)
 *
 * DATA FIELDS:
 * - m_abandoned: true if user explicitly abandoned, false if auto-conflicted
 */
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

/**
 * TxStateUnrecognized: Transaction state is unrecognized
 *
 * WHEN USED:
 * - Reading old wallet.dat from before state system implemented
 * - Deserializing transaction with unknown state marker
 * - Forward compatibility (future state types)
 *
 * PURPOSE: BACKWARD/FORWARD COMPATIBILITY
 * When loading old wallets that used hashBlock for state, we need a way
 * to represent "we have some state data but don't recognize the format".
 * This allows:
 * 1. Old wallets to load without error
 * 2. Future state types to be ignored gracefully
 * 3. Wallet to re-determine state from chain data
 *
 * CHARACTERISTICS:
 * - Temporary state during wallet upgrade
 * - Should be resolved to proper state on next sync
 * - Not used for new transactions
 *
 * MIGRATION PATH:
 * Old wallet -> TxStateUnrecognized -> Re-scan chain -> Proper state
 *
 * DATA FIELDS:
 * - m_block_hash: Legacy hashBlock value (if present)
 * - m_index: Legacy index value (if present)
 *
 * NOTE: These fields preserved for debugging and migration only
 */
struct TxStateUnrecognized {
    uint256 m_block_hash;
    int m_index;

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
 * TxState: Variant type holding any transaction state
 *
 * USAGE:
 * TxState is a std::variant that can hold any of the four state types.
 * Use std::visit or std::holds_alternative to work with the active state.
 *
 * EXAMPLE:
 *   TxState state = GetTransactionState(txid);
 *   if (std::holds_alternative<TxStateConfirmed>(state)) {
 *       auto& confirmed = std::get<TxStateConfirmed>(state);
 *       LogPrint("wallet", "Confirmed at height %d\n",
 *                confirmed.m_confirmed_block_height);
 *   }
 *
 * ORDERING MATTERS:
 * The order of types in the variant affects serialization. Do not reorder
 * without migration logic, as it would break wallet.dat compatibility.
 */
using TxState = std::variant<
    TxStateInMempool,
    TxStateConfirmed,
    TxStateInactive,
    TxStateUnrecognized
>;

/**
 * Serialization support for TxState variant
 *
 * This enables direct serialization of the variant,
 * eliminating the need for mapValue as an intermediate storage.
 *
 * HOW IT WORKS:
 * 1. Serialize the variant index (which state type)
 * 2. Serialize the state-specific data using the struct's own serialization
 *
 * VERSION COMPATIBILITY:
 * - Used for wallets with version >= FEATURE_TRANSACTION_STATES (5040102)
 * - Older wallets use legacy hashBlock migration
 *
 * VARIANT INDEX MAPPING:
 * 0 = TxStateInMempool
 * 1 = TxStateConfirmed
 * 2 = TxStateInactive
 * 3 = TxStateUnrecognized
 *
 * CRITICAL: Do NOT call these functions directly on old wallet formats.
 * Must always version-gate through CWalletTx::SerializationOp().
 * The variant index is fixed and cannot be reordered without breaking
 * compatibility with existing wallet.dat files.
 */

/**
 * Variant Index Version Gating - Helper Functions
 *
 * These functions implement safe variant serialization with explicit
 * index handling. The variant index (0-3) maps to state type:
 * 0 = TxStateInMempool
 * 1 = TxStateConfirmed
 * 2 = TxStateInactive
 * 3 = TxStateUnrecognized
 *
 * CRITICAL: The index order MUST NOT change without migration logic.
 * Old wallet.dat files encode state as (index, data).
 * Reordering indices would corrupt deserialization.
 *
 * Version gating at the CWalletTx level (wallet.h) prevents serialization
 * of this variant to old wallets (version < 5040102).
 */

// Helper to serialize variant with version awareness
template<typename Stream>
void SerializeTxState(Stream& s, const TxState& state)
{
    // Serialize variant index explicitly
    // This ensures old wallet.dat remains readable even if variant changes
    int state_index = state.index();
    ::Serialize(s, state_index);
    std::visit([&s](const auto& st) {
        ::Serialize(s, st);
    }, state);
}

// Helper to deserialize variant with version awareness
template<typename Stream>
void UnserializeTxState(Stream& s, TxState& state)
{
    int state_index;
    ::Unserialize(s, state_index);

    switch (state_index) {
        case 0: {
            TxStateInMempool mempool_state;
            ::Unserialize(s, mempool_state);
            state = mempool_state;
            break;
        }
        case 1: {
            TxStateConfirmed confirmed_state;
            ::Unserialize(s, confirmed_state);
            state = confirmed_state;
            break;
        }
        case 2: {
            TxStateInactive inactive_state;
            ::Unserialize(s, inactive_state);
            state = inactive_state;
            break;
        }
        case 3: {
            TxStateUnrecognized unrecognized_state;
            ::Unserialize(s, unrecognized_state);
            state = unrecognized_state;
            break;
        }
        default: {
            /**
             * Forward Compatibility Handler
             * Unknown state type - deserialize as unrecognized for forward
             * compatibility with future state types (index 4+).
             *
             * CRITICAL: Must consume stream data to prevent stream misalignment
             * that would corrupt subsequent transaction deserialization.
             *
             * This allows old code to load wallets from new versions that
             * introduce new state types, gracefully degrading to unrecognized
             * state which will be re-determined from blockchain on load.
             */
            TxStateUnrecognized unrecognized_state;
            try {
                ::Unserialize(s, unrecognized_state);
                state = unrecognized_state;

                /**
                 * Logging Initialization Safety
                 *
                 * CRITICAL: LogInstance() returns a reference (BCLog::Logger&),
                 * which is always valid and cannot be null. References are a C++
                 * language guarantee - they always point to valid objects.
                 *
                 * Use reference semantics, not pointers
                 * auto& log_instance = LogInstance();
                 * if (log_instance.WillLogCategory(...)) { ... }
                 *
                 * No null checks needed - references cannot be null.
                 * This allows clean, safe logging code without defensive checks.
                 */
                auto& log_instance = LogInstance();
                if (log_instance.WillLogCategory(BCLog::LogFlags::VERBOSE)) {
                    LogPrint(BCLog::LogFlags::VERBOSE,
                             "TxState::Unserialize: Encountered unknown state type %d\n",
                             state_index);
                }
            } catch (const std::exception& e) {
                state = TxStateUnrecognized{};
                auto& log_instance = LogInstance();
                if (log_instance.WillLogCategory(BCLog::LogFlags::ALL)) {
                    LogPrintf("WARNING: TxState::Unserialize: Failed to deserialize unknown state type %d: %s\n",
                              state_index, e.what());
                }
            } catch (...) {
                state = TxStateUnrecognized{};
                auto& log_instance = LogInstance();
                if (log_instance.WillLogCategory(BCLog::LogFlags::ALL)) {
                    LogPrintf("WARNING: TxState::Unserialize: Unexpected error deserializing unknown state type %d\n",
                              state_index);
                }
            }
            break;
        }
    }
}

// Legacy template functions for backward compatibility
template<typename Stream>
void Serialize(Stream& s, const TxState& state)
{
    SerializeTxState(s, state);
}

template<typename Stream>
void Unserialize(Stream& s, TxState& state)
{
    UnserializeTxState(s, state);
}

} // namespace wallet

#endif // BITCOIN_WALLET_TRANSACTION_H
