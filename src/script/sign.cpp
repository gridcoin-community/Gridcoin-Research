#include "script/sign.h"
#include "script/interpreter.h"
#include "script/standard.h"
#include "keystore.h"
#include "primitives/transaction.h"
#include <policy/policy.h>

using namespace std;

// ---------------------------------------------------------------------------
// Signature creator implementations
// ---------------------------------------------------------------------------

TransactionSignatureCreator::TransactionSignatureCreator(
    const CTransaction& txToIn, unsigned int nInIn, int nHashTypeIn)
    : txTo(txToIn), nIn(nInIn), nHashType(nHashTypeIn)
{
}

bool TransactionSignatureCreator::CreateSig(
    const SigningProvider& provider, std::vector<unsigned char>& vchSig,
    const CKeyID& keyid, const CScript& scriptCode) const
{
    CKey key;
    if (!provider.GetKey(keyid, key))
        return false;

    uint256 hash = SignatureHash(scriptCode, txTo, nIn, nHashType);

    if (!key.Sign(hash, vchSig))
        return false;
    vchSig.push_back((unsigned char)nHashType);
    return true;
}

MutableTransactionSignatureCreator::MutableTransactionSignatureCreator(
    const CMutableTransaction& txToIn, unsigned int nInIn, int nHashTypeIn)
    : txTo(txToIn), nIn(nInIn), nHashType(nHashTypeIn)
{
}

bool MutableTransactionSignatureCreator::CreateSig(
    const SigningProvider& provider, std::vector<unsigned char>& vchSig,
    const CKeyID& keyid, const CScript& scriptCode) const
{
    CKey key;
    if (!provider.GetKey(keyid, key))
        return false;

    CTransaction txToConst(txTo);
    uint256 hash = SignatureHash(scriptCode, txToConst, nIn, nHashType);

    if (!key.Sign(hash, vchSig))
        return false;
    vchSig.push_back((unsigned char)nHashType);
    return true;
}

// ---------------------------------------------------------------------------
// SignStep: core signing dispatch using creator pattern
// ---------------------------------------------------------------------------

static bool SignStep(const SigningProvider& provider,
                     const BaseSignatureCreator& creator,
                     const CScript& scriptPubKey,
                     CScript& scriptSigRet,
                     txnouttype& whichTypeRet)
{
    scriptSigRet.clear();

    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichTypeRet, vSolutions))
        return false;

    CKeyID keyID;
    switch (whichTypeRet)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return false;

    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        {
            vector<unsigned char> vchSig;
            if (!creator.CreateSig(provider, vchSig, keyID, scriptPubKey))
                return false;
            scriptSigRet << vchSig;
        }
        return true;

    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        {
            vector<unsigned char> vchSig;
            if (!creator.CreateSig(provider, vchSig, keyID, scriptPubKey))
                return false;
            scriptSigRet << vchSig;

            CPubKey vch;
            provider.GetPubKey(keyID, vch);
            scriptSigRet << vch;
        }
        return true;

    case TX_SCRIPTHASH:
        return provider.GetCScript(CScriptID(uint160(vSolutions[0])), scriptSigRet);

    case TX_MULTISIG:
        scriptSigRet << OP_0; // workaround CHECKMULTISIG bug
        {
            int nSigned = 0;
            int nRequired = vSolutions.front()[0];
            for (unsigned int i = 1; i < vSolutions.size()-1 && nSigned < nRequired; i++)
            {
                const valtype& pubkey = vSolutions[i];
                CKeyID keyID2 = CPubKey(pubkey).GetID();
                vector<unsigned char> vchSig;
                if (creator.CreateSig(provider, vchSig, keyID2, scriptPubKey))
                {
                    scriptSigRet << vchSig;
                    ++nSigned;
                }
            }
            return nSigned == nRequired;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// ProduceSignature: fills SignatureData without verifying
// ---------------------------------------------------------------------------

bool ProduceSignature(const SigningProvider& provider,
                      const BaseSignatureCreator& creator,
                      const CScript& scriptPubKey,
                      SignatureData& sigdata)
{
    CScript& scriptSigOut = sigdata.scriptSig;

    txnouttype whichType;
    if (!SignStep(provider, creator, scriptPubKey, scriptSigOut, whichType))
        return false;

    if (whichType == TX_SCRIPTHASH)
    {
        // SignStep returns the subscript that needs to be evaluated;
        // the final scriptSig is the signatures from that
        // and then the serialized subscript:
        CScript subscript = scriptSigOut;

        txnouttype subType;
        bool fSolved =
            SignStep(provider, creator, subscript, scriptSigOut, subType) && subType != TX_SCRIPTHASH;
        // Append serialized subscript whether or not it is completely signed:
        scriptSigOut << valtype(subscript.begin(), subscript.end());
        if (!fSolved) return false;
    }

    return true;
}

void UpdateInput(CTxIn& input, const SignatureData& data)
{
    input.scriptSig = data.scriptSig;
}

// ---------------------------------------------------------------------------
// SignSignature: public API using TransactionSignatureCreator
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// CMutableTransaction overloads (primary implementations)
// ---------------------------------------------------------------------------

bool SignSignature(const SigningProvider& provider, const CScript& fromPubKey, CMutableTransaction& txTo, unsigned int nIn, int nHashType)
{
    assert(nIn < txTo.vin.size());

    MutableTransactionSignatureCreator creator(txTo, nIn, nHashType);
    SignatureData sigdata;
    bool ret = ProduceSignature(provider, creator, fromPubKey, sigdata);
    UpdateInput(txTo.vin[nIn], sigdata);

    if (!ret) return false;

    // Test solution
    CTransaction txConst(txTo);
    return VerifyScript(txConst.vin[nIn].scriptSig, fromPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, txConst, nIn);
}

bool SignSignature(const SigningProvider& provider, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, int nHashType)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    return SignSignature(provider, txout.scriptPubKey, txTo, nIn, nHashType);
}


static CScript PushAll(const vector<valtype>& values)
{
    CScript result;
    for (auto const& v : values)
        result << v;
    return result;
}

static CScript CombineMultisig(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                               const vector<valtype>& vSolutions,
                               vector<valtype>& sigs1, vector<valtype>& sigs2)
{
    // Combine all the signatures we've got:
    set<valtype> allsigs;
    for (auto const& v : sigs1)
    {
        if (!v.empty())
            allsigs.insert(v);
    }
    for (auto const& v : sigs2)
    {
        if (!v.empty())
            allsigs.insert(v);
    }

    // Build a map of pubkey -> signature by matching sigs to pubkeys:
    assert(vSolutions.size() > 1);
    unsigned int nSigsRequired = vSolutions.front()[0];
    unsigned int nPubKeys = vSolutions.size()-2;
    map<valtype, valtype> sigs;
    for (auto const& sig : allsigs)
    {
        for (unsigned int i = 0; i < nPubKeys; i++)
        {
            const valtype& pubkey = vSolutions[i+1];
            if (sigs.count(pubkey))
                continue; // Already got a sig for this pubkey

            if (CheckSig(sig, pubkey, scriptPubKey, txTo, nIn))
            {
                sigs[pubkey] = sig;
                break;
            }
        }
    }
    // Now build a merged CScript:
    unsigned int nSigsHave = 0;
    CScript result; result << OP_0; // pop-one-too-many workaround
    for (unsigned int i = 0; i < nPubKeys && nSigsHave < nSigsRequired; i++)
    {
        if (sigs.count(vSolutions[i+1]))
        {
            result << sigs[vSolutions[i+1]];
            ++nSigsHave;
        }
    }
    // Fill any missing with OP_0:
    for (unsigned int i = nSigsHave; i < nSigsRequired; i++)
        result << OP_0;

    return result;
}

static CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                                 const txnouttype txType, const vector<valtype>& vSolutions,
                                 vector<valtype>& sigs1, vector<valtype>& sigs2)
{
    switch (txType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        // Don't know anything about this, assume bigger one is correct:
        if (sigs1.size() >= sigs2.size())
            return PushAll(sigs1);
        return PushAll(sigs2);
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
        // Signatures are bigger than placeholders or empty scripts:
        if (sigs1.empty() || sigs1[0].empty())
            return PushAll(sigs2);
        return PushAll(sigs1);
    case TX_SCRIPTHASH:
        if (sigs1.empty() || sigs1.back().empty())
            return PushAll(sigs2);
        else if (sigs2.empty() || sigs2.back().empty())
            return PushAll(sigs1);
        else
        {
            // Recur to combine:
            valtype spk = sigs1.back();
            CScript pubKey2(spk.begin(), spk.end());

            txnouttype txType2;
            vector<vector<unsigned char> > vSolutions2;
            Solver(pubKey2, txType2, vSolutions2);
            sigs1.pop_back();
            sigs2.pop_back();
            CScript result = CombineSignatures(pubKey2, txTo, nIn, txType2, vSolutions2, sigs1, sigs2);
            result << spk;
            return result;
        }
    case TX_MULTISIG:
        return CombineMultisig(scriptPubKey, txTo, nIn, vSolutions, sigs1, sigs2);
    }

    return CScript();
}

CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                          const CScript& scriptSig1, const CScript& scriptSig2)
{
    txnouttype txType;
    vector<vector<unsigned char> > vSolutions;
    Solver(scriptPubKey, txType, vSolutions);

    vector<valtype> stack1;
    EvalScript(stack1, scriptSig1, STANDARD_SCRIPT_VERIFY_FLAGS, CTransaction(), 0);
    vector<valtype> stack2;
    EvalScript(stack2, scriptSig2, STANDARD_SCRIPT_VERIFY_FLAGS, CTransaction(), 0);

    return CombineSignatures(scriptPubKey, txTo, nIn, txType, vSolutions, stack1, stack2);
}
