// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TXORDER_H
#define BITCOIN_QT_TXORDER_H

#include "uint256.h"

#include <cstdint>

//!
//! \file txorder.h
//!
//! Qt-free transaction ordering key + comparator for the windowed
//! transaction-table model's producer-side store.
//!
//! This is the single source of truth for the cached-wallet ordering that was
//! introduced in PR #3003 as `TxRecordOrder` (time DESC, hash ASC, idx ASC).
//! It is extracted here, operating on a minimal Qt-free key, so that:
//!
//!   1. The producer-side `GRC::WalletTxStore` (which is Qt-coupled, because
//!      `TransactionRecord` pulls in `<QList>`/`qint64`) can compute insert
//!      positions and contiguous removal ranges through this one comparator.
//!   2. The position/clustering logic gets unit-test coverage in the GUI-OFF
//!      build configuration that CI exercises — the blind spot that hid
//!      PR #2944's heap corruption — since `uint256` is Qt-free.
//!
//! `WalletTxStore::TxRecordOrder` projects a `TransactionRecord` into a
//! `TxOrderKey` and defers to `TxOrderLess`, so there is exactly one ordering
//! definition.
//!

namespace GRC {

//!
//! \brief The three fields that define a transaction row's position in the
//! cached, ordered wallet view: receive time, tx hash, and the decomposition
//! sub-index.
//!
struct TxOrderKey {
    //! TransactionRecord::time (CWalletTx::GetTxTime), seconds since epoch.
    int64_t time = 0;
    //! TransactionRecord::hash.
    uint256 hash;
    //! TransactionRecord::idx — the decomposeTransaction sub-index, unique
    //! within a single tx, making the order a true total order.
    int idx = 0;
};

//!
//! \brief Strict weak ordering: (time DESC, hash ASC, idx ASC).
//!
//! - time DESCENDING — newest first, the default user-facing order.
//! - hash ASCENDING  — clusters all decomposed records of one tx into a
//!   contiguous range (the removal path relies on this clustering).
//! - idx ASCENDING   — within one tx, preserves decompose order; idx is unique
//!   within a tx, so no two keys ever compare equal.
//!
//! \return true if \p a sorts strictly before \p b.
//!
bool TxOrderLess(const TxOrderKey& a, const TxOrderKey& b);

} // namespace GRC

#endif // BITCOIN_QT_TXORDER_H
