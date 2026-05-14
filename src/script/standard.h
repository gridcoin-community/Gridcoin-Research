#ifndef BITCOIN_SCRIPT_STANDARD_H
#define BITCOIN_SCRIPT_STANDARD_H

#include "script/script.h"
#include "wallet/ismine.h"

class CKeyStore;

// Note: txnouttype enum and CNoDestination/CTxDestination are in script/script.h
// since CScript::SetDestination depends on CTxDestination

bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char>>& vSolutionsRet);
int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char>>& vSolutions);
unsigned int HaveKeys(const std::vector<valtype>& pubkeys, const CKeyStore& keystore);

/**
 * This is an internal representation of isminetype + invalidity.
 * Its order is significant, as we return the max of all explored
 * possibilities.
 */
enum class IsMineResult
{
    NO = 0,         //!< Not ours
    WATCH_ONLY = 1, //!< Included in watch-only balance
    SPENDABLE = 2,  //!< Included in all balances
};

IsMineResult IsMineInner(const CKeyStore &keystore, const CScript& scriptPubKey);
isminetype IsMine(const CKeyStore &keystore, const CScript& scriptPubKey);
isminetype IsMine(const CKeyStore& keystore, const CTxDestination &dest);
void ExtractAffectedKeys(const CKeyStore &keystore, const CScript& scriptPubKey, std::vector<CKeyID> &vKeys);
bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet);
bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

#endif // BITCOIN_SCRIPT_STANDARD_H
