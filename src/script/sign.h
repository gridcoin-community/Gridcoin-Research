#ifndef BITCOIN_SCRIPT_SIGN_H
#define BITCOIN_SCRIPT_SIGN_H

#include "script/script.h"

#include <vector>

class CKeyStore;
class CTransaction;
class SigningProvider;
struct CMutableTransaction;

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

bool SignSignature(const CKeyStore& keystore, const CScript& fromPubKey, CTransaction& txTo, unsigned int nIn, int nHashType=SIGHASH_ALL);
bool SignSignature(const CKeyStore& keystore, const CTransaction& txFrom, CTransaction& txTo, unsigned int nIn, int nHashType=SIGHASH_ALL);

// Given two sets of signatures for scriptPubKey, possibly with OP_0 placeholders,
// combine them intelligently and return the result.
CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn, const CScript& scriptSig1, const CScript& scriptSig2);

#endif // BITCOIN_SCRIPT_SIGN_H
