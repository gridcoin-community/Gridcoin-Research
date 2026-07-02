// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_PSGT_TEST_HELPERS_H
#define GRIDCOIN_TEST_PSGT_TEST_HELPERS_H

#include <key.h>
#include <primitives/transaction.h>
#include <psgt.h>
#include <script/script.h>
#include <script/standard.h>

#include <vector>

//! Shared builders for PSGT unit tests (psgt_tests, psgt_pool_tests).

inline CKey MakeKey()
{
    CKey key;
    key.MakeNewKey(true);
    return key;
}

inline CScript P2PKH(const CKeyID& id)
{
    CScript s;
    s << OP_DUP << OP_HASH160 << id << OP_EQUALVERIFY << OP_CHECKSIG;
    return s;
}

inline CScript P2SH(const CScriptID& id)
{
    CScript s;
    s << OP_HASH160 << id << OP_EQUAL;
    return s;
}

inline CScript MultisigScript(int required, const std::vector<CPubKey>& pubkeys)
{
    CScript s;
    s << (int64_t)required;
    for (const CPubKey& pubkey : pubkeys)
        s << pubkey;
    s << (int64_t)pubkeys.size() << OP_CHECKMULTISIG;
    return s;
}

inline CScript Multisig1of2(const CKey& key1, const CKey& key2)
{
    return MultisigScript(1, {key1.GetPubKey(), key2.GetPubKey()});
}

//! Build a fake previous transaction that pays to the given key.
inline CTransaction MakePrevTx(const CKey& key, CAmount amount)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000000;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull(); // coinbase-like
    mtx.vin[0].scriptSig = CScript() << 0;
    mtx.vout.push_back(CTxOut(amount, P2PKH(key.GetPubKey().GetID())));
    return CTransaction(mtx);
}

//! Build a fake previous transaction with an arbitrary scriptPubKey.
inline CTransaction MakePrevTxScript(const CScript& scriptPubKey, CAmount amount)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000000;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 0;
    mtx.vout.push_back(CTxOut(amount, scriptPubKey));
    return CTransaction(mtx);
}

//! Build an unsigned one-input spend of prevTx:0 paying amount to a fresh key.
inline PartiallySignedTransaction MakeSpendPSGT(const CTransaction& prevTx, CAmount amount)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700002000;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(amount, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;
    return psgt;
}

#endif // GRIDCOIN_TEST_PSGT_TEST_HELPERS_H
