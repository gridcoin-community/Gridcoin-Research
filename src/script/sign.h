// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SIGN_H
#define BITCOIN_SCRIPT_SIGN_H

#include "script/script.h"

#include <vector>

class CTransaction;
class SigningProvider;
struct CMutableTransaction;
class CTxIn;

/** Interface for signature creators. */
class BaseSignatureCreator
{
public:
    virtual ~BaseSignatureCreator() {}

    /** Create a signature for the given keyid and scriptCode. */
    virtual bool CreateSig(const SigningProvider& provider,
                           std::vector<unsigned char>& vchSig,
                           const CKeyID& keyid,
                           const CScript& scriptCode) const = 0;
};

/** A signature creator that signs with an immutable transaction. */
class TransactionSignatureCreator : public BaseSignatureCreator
{
    const CTransaction& txTo;
    unsigned int nIn;
    int nHashType;

public:
    TransactionSignatureCreator(const CTransaction& txToIn, unsigned int nInIn,
                                int nHashTypeIn = SIGHASH_ALL);

    bool CreateSig(const SigningProvider& provider,
                   std::vector<unsigned char>& vchSig,
                   const CKeyID& keyid,
                   const CScript& scriptCode) const override;
};

/** A signature creator that signs with a mutable transaction. */
class MutableTransactionSignatureCreator : public BaseSignatureCreator
{
    const CMutableTransaction& txTo;
    unsigned int nIn;
    int nHashType;

public:
    MutableTransactionSignatureCreator(const CMutableTransaction& txToIn,
                                       unsigned int nInIn,
                                       int nHashTypeIn = SIGHASH_ALL);

    bool CreateSig(const SigningProvider& provider,
                   std::vector<unsigned char>& vchSig,
                   const CKeyID& keyid,
                   const CScript& scriptCode) const override;
};

/** Holds signing result: scriptSig + any redeem scripts used. */
struct SignatureData
{
    CScript scriptSig;

    SignatureData() {}
    explicit SignatureData(const CScript& script) : scriptSig(script) {}
};

/** Produce a satisfying script (SignatureData) for the given scriptPubKey. */
bool ProduceSignature(const SigningProvider& provider,
                      const BaseSignatureCreator& creator,
                      const CScript& scriptPubKey,
                      SignatureData& sigdata);

/** Apply SignatureData to a CTxIn. */
void UpdateInput(CTxIn& input, const SignatureData& data);

bool SignSignature(const SigningProvider& provider, const CScript& fromPubKey, CMutableTransaction& txTo, unsigned int nIn, int nHashType=SIGHASH_ALL);
bool SignSignature(const SigningProvider& provider, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, int nHashType=SIGHASH_ALL);

// Given two sets of signatures for scriptPubKey, possibly with OP_0 placeholders,
// combine them intelligently and return the result.
CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn, const CScript& scriptSig1, const CScript& scriptSig2);

#endif // BITCOIN_SCRIPT_SIGN_H
