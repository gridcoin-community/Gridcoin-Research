// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CONSENSUS_MUTABLE_TRANSACTION_H
#define GRIDCOIN_CONSENSUS_MUTABLE_TRANSACTION_H

#include "primitives/transaction.h"

#include <string>
#include <vector>

//! Scaffolding for CMutableTransaction — a mutable transaction builder type.
//!
//! This is a temporary stand-in that will be replaced by the real
//! CMutableTransaction when the CTransaction immutability split lands
//! (PR E of #2877). The purpose is to establish the correct interface
//! boundaries now:
//!
//!   - Construct-mode methods take CMutableTransaction& (mutable builder)
//!   - Validate-mode methods take const CTransaction& (immutable, read-only)
//!
//! This scaffolding is a value type with its own fields, matching the
//! Bitcoin Core convention. The miner uses a copy-out / copy-back pattern:
//!
//!   CMutableTransaction mtx(blocknew.vtx[1]);    // copy out
//!   rules.ConstructSomething(mtx, ...);           // mutate
//!   blocknew.vtx[1] = CTransaction(mtx);         // copy back
//!
//! After PR E, the scaffolding is deleted and the real CMutableTransaction
//! is used. Method signatures in BlockRewardRules require zero changes.
//!
//! IMPORTANT: Do not add serialization, GetHash(), or any methods beyond
//! what is needed for the copy-out / mutate / copy-back pattern. The real
//! CMutableTransaction will provide those.
struct CMutableTransaction
{
    int nVersion;
    unsigned int nTime;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    unsigned int nLockTime;
    std::string hashBoinc;
    std::vector<GRC::Contract> vContracts;

    CMutableTransaction()
        : nVersion(CTransaction::CURRENT_VERSION)
        , nTime(0)
        , vin()
        , vout()
        , nLockTime(0)
        , hashBoinc()
        , vContracts()
    {
    }

    //! Construct from an existing CTransaction (copy fields).
    explicit CMutableTransaction(const CTransaction& tx)
        : nVersion(tx.nVersion)
        , nTime(tx.nTime)
        , vin(tx.vin)
        , vout(tx.vout)
        , nLockTime(tx.nLockTime)
        , hashBoinc(tx.hashBoinc)
        , vContracts(tx.vContracts)
    {
    }
};

//! Convert a CMutableTransaction back to a CTransaction.
//!
//! Today this is a field-by-field copy since CTransaction is still mutable.
//! After PR E, CTransaction's constructor from CMutableTransaction will
//! move the data and compute the cached hash. This free function provides
//! a single conversion point that PR E can update or delete.
inline CTransaction MakeTransaction(const CMutableTransaction& mtx)
{
    CTransaction tx;
    tx.nVersion = mtx.nVersion;
    tx.nTime = mtx.nTime;
    tx.vin = mtx.vin;
    tx.vout = mtx.vout;
    tx.nLockTime = mtx.nLockTime;
    tx.hashBoinc = mtx.hashBoinc;
    tx.vContracts = mtx.vContracts;
    return tx;
}

#endif // GRIDCOIN_CONSENSUS_MUTABLE_TRANSACTION_H
