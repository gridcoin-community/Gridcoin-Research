// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_TXDETAIL_H
#define GRIDCOIN_WALLET_TXDETAIL_H

#include "interfaces/wallet_tx_channel.h"

class CWallet;
class CWalletTx;

namespace GRC {

//! Fill the structured transaction-detail DTO for one part (\p vout) of a
//! wallet transaction — the node-side half of what TransactionDesc::toHTML
//! used to compute and render in one piece. Every branch mirrors the old
//! renderer exactly; the GUI-side TransactionDesc now renders (and
//! translates) the returned value instead (multiprocess design §4.1).
//!
//! Takes LOCK2(cs_main, cs_wallet) internally, exactly as toHTML did — safe
//! to call with or without the locks already held (they are recursive).
WalletTxDetail FillWalletTxDetail(CWallet* wallet, CWalletTx& wtx, unsigned int vout);

} // namespace GRC

#endif // GRIDCOIN_WALLET_TXDETAIL_H
