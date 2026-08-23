// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_INTERPRETER_H
#define BITCOIN_SCRIPT_INTERPRETER_H

#include "script/script.h"

#include <cstdint>

class CTransaction;

//! Default -maxsigcachesize, in entries. Historically 10 MB of cache at
//! roughly SIG_CACHE_ENTRY_BYTES per entry. A block admits at most 20,000
//! signature operations, so this holds well over a block's worth.
static constexpr int64_t DEFAULT_MAX_SIG_CACHE_SIZE = 50000;

//! Approximate resident cost of one cache entry: a 32-byte hash, a ~72-byte
//! signature and a ~33-byte pubkey, plus the set node and the two vector
//! allocations that carry them.
static constexpr int64_t SIG_CACHE_ENTRY_BYTES = 200;

//! Ceiling on what -maxsigcachesize may ask for. Not a tuning knob -- it is
//! there so an operator typo cannot size an unbounded in-memory set. Well
//! above any useful setting: 512 MB is 50x the default cache.
static constexpr int64_t MAX_SIG_CACHE_BYTES = 512 * 1024 * 1024;
static constexpr int64_t MAX_SIG_CACHE_ENTRIES = MAX_SIG_CACHE_BYTES / SIG_CACHE_ENTRY_BYTES;

//! Resolve -maxsigcachesize into an entry count, clamped to
//! MAX_SIG_CACHE_ENTRIES. Zero or negative disables the cache and yields 0.
//! Declared here so the clamp can be tested directly; the cache itself is
//! file-local to interpreter.cpp.
int64_t ResolveMaxSigCacheSize();

bool EvalScript(std::vector<std::vector<unsigned char>>& stack, const CScript& script, unsigned int flags, const CTransaction& txTo, unsigned int nIn);
uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);
bool CheckSig(std::vector<unsigned char> vchSig, std::vector<unsigned char> vchPubKey, CScript scriptCode, const CTransaction& txTo, unsigned int nIn);
bool CheckSignatureEncoding(const std::vector<unsigned char>& vchSig, unsigned int flags);
bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, unsigned int flags, const CTransaction& txTo, unsigned int nIn);
bool VerifySignature(const CTransaction& txFrom, const CTransaction& txTo, unsigned int flags, unsigned int nIn, int nHashType);

#endif
