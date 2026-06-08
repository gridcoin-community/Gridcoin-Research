// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_INTERPRETER_H
#define BITCOIN_SCRIPT_INTERPRETER_H

#include "script/script.h"

class CTransaction;

bool EvalScript(std::vector<std::vector<unsigned char>>& stack, const CScript& script, unsigned int flags, const CTransaction& txTo, unsigned int nIn);
uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);
bool CheckSig(std::vector<unsigned char> vchSig, std::vector<unsigned char> vchPubKey, CScript scriptCode, const CTransaction& txTo, unsigned int nIn);
bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, unsigned int flags, const CTransaction& txTo, unsigned int nIn);
bool VerifySignature(const CTransaction& txFrom, const CTransaction& txTo, unsigned int flags, unsigned int nIn, int nHashType);

#endif
