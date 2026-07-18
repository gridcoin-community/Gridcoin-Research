// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// chain_trust.h and spam.h rely on their includer supplying a complete
// CBlockIndex first (the ChainTrustCache constructor news one inline), so
// primitives/block.h must precede them here.
#include "primitives/block.h"

#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/spam.h"

// Definitions moved out of main.cpp (issue #3125 C9). Declarations live in
// the headers above; the many per-TU ad-hoc externs were removed in the same
// change.

GRC::SeenStakes g_seen_stakes GUARDED_BY(cs_main);
GRC::ChainTrustCache g_chain_trust GUARDED_BY(cs_main);

//!
//! \brief Re-exports chain trust values for reporting.
//!
arith_uint256 GetChainTrust(const CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return g_chain_trust.GetTrust(pindex);
}
