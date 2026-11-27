// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_TRANSACTIONRECORD_H
#define GRIDCOIN_WALLET_TRANSACTIONRECORD_H

#include "interfaces/wallet_tx_record.h"
#include "wallet/wallet.h"

#include <cstdint>
#include <vector>

// The node-side work that fills TransactionRecord values from the wallet. It reads
// chain and wallet state, so its lock requirements are stated here, apart from the
// record type that the GUI shares.

/* Return positive answer if transaction should be shown in list. */
bool showTransaction(const CWalletTx& wtx, bool datetime_limit_flag = false, const int64_t& datetime_limit = 0)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/* Decompose CWallet transaction to model transaction records. */
std::vector<TransactionRecord> decomposeTransaction(const CWallet* wallet, const CWalletTx& wtx)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet);

/* Update the status of rec from core wallet tx. */
void updateStatus(TransactionRecord& rec, const CWalletTx& wtx) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Populate rec.label from the wallet address book. Called producer-side at the
 *  fill points after updateStatus() so the off-cs_wallet cursor can sort the
 *  Address column and filter by label without re-touching the wallet
 *  (windowed-model PR4).
 */
void populateDisplayLabel(TransactionRecord& rec, const CWallet& wallet) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

#endif // GRIDCOIN_WALLET_TRANSACTIONRECORD_H
