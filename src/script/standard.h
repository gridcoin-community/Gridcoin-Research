// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_STANDARD_H
#define BITCOIN_SCRIPT_STANDARD_H

#include "script/script.h"
#include "wallet/ismine.h"

class SigningProvider;

// Note: txnouttype enum and CNoDestination/CTxDestination are in script/script.h
// since CScript::SetDestination depends on CTxDestination

bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char>>& vSolutionsRet);
int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char>>& vSolutions);
unsigned int HaveKeys(const std::vector<valtype>& pubkeys, const SigningProvider& provider);

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

IsMineResult IsMineInner(const SigningProvider &provider, const CScript& scriptPubKey);
isminetype IsMine(const SigningProvider &provider, const CScript& scriptPubKey);
isminetype IsMine(const SigningProvider& provider, const CTxDestination &dest);
void ExtractAffectedKeys(const SigningProvider &provider, const CScript& scriptPubKey, std::vector<CKeyID> &vKeys);
bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet);
bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

#endif // BITCOIN_SCRIPT_STANDARD_H
