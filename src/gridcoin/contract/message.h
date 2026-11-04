// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CONTRACT_MESSAGE_H
#define GRIDCOIN_CONTRACT_MESSAGE_H

#include "amount.h"
#include "sync.h"
#include "wallet/wallet.h"

#include <optional>
#include <string>

class StakingKeyAccess;
struct CMutableTransaction;
extern CCriticalSection cs_main;

namespace GRC {

class Contract;
class Cpid;

//!
//! \brief Create and send a transaction that contains the provided contract.
//!
//! \param contract A new contract to publish in a transaction.
//!
//! \return Contains the finalized transaction and error message, if any.
//! TODO: refactor to remove string-based signaling.
//!
//! \p renewal_access is for the ONE caller that must still work on a wallet
//! the user restricted to staking: the hourly automated beacon renewal, which
//! is the only path that can make one. With it the transaction is built in the
//! renewal's fixed shape at the minimum fee; see CWallet::CreateTransaction().
//! Everything else -- every RPC, every GUI action, addkey -- passes nullptr and
//! is refused, which is the point of the restriction. Do not widen it to
//! "contracts are harmless": the GUI contract surface has no unlock guard of
//! its own, so this is the only thing standing in front of it.
std::pair<CWalletTx, std::string> SendContract(Contract contract, const StakingKeyAccess* renewal_access = nullptr)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//!
//! \brief Send a transaction that contains a contract.
//!
//! This overload enables client code to configure aspects of the transaction
//! that contains contract message.
//!
//! \param wtx Transaction to send that contains a contract.
//!
//! \return Contains the finalized transaction and error message, if any.
//! TODO: refactor to remove string-based signaling.
//!
std::pair<CWalletTx, std::string> SendContract(CWalletTx wtx, const StakingKeyAccess* renewal_access = nullptr)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//!
//! \brief The minimum fee for \p tx at the size it has once its inputs are
//! signed.
//!
//! Each input is sized at the largest signature the wallet writes for the
//! coin it spends, so the answer is the same before signing and after, and
//! never less than the minimum for the signed transaction. Every input must
//! spend a coin of \p wallet in a standard form: pay-to-pubkey,
//! pay-to-pubkey-hash, bare multisig, or pay-to-script-hash over one of those.
//!
//! \return The fee, or nullopt when \p tx has no inputs or an input does not
//! meet that.
//!
std::optional<CAmount> RenewalMinimumFee(const CMutableTransaction& tx, const CWallet& wallet)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

//!
//! \brief Whether \p tx is the automated beacon renewal's fixed shape.
//!
//! True iff:
//!  - its contracts are one beacon advertisement for \p cpid, a renewal or a
//!    new beacon;
//!  - \p burn_fee is the burn that contract requires;
//!  - its outputs are one OP_RETURN burn of \p burn_fee and at most one change
//!    output, which \p wallet can spend and which pays the script of one of
//!    the coins the transaction spends;
//!  - it spends only coins of \p wallet, and pays exactly \p fee, which is
//!    RenewalMinimumFee().
//!
//! Reads \p wallet and changes nothing.
//!
bool IsRenewalShape(const CMutableTransaction& tx, const Cpid& cpid, CAmount burn_fee,
                    CAmount fee, const CWallet& wallet) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
}

#endif // GRIDCOIN_CONTRACT_MESSAGE_H
