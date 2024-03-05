// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_MERKLE_H
#define BITCOIN_CONSENSUS_MERKLE_H

#include <vector>

#include "main.h"
#include <uint256.h>

uint256 ComputeMerkleRoot(std::vector<uint256> leaves, bool* mutated = nullptr);

/*
 * Compute the Merkle root of the transactions in a block.
 * *mutated is set to true if a duplicated subtree was found.
 */
uint256 BlockMerkleRoot(const CBlock& block, bool* mutated = nullptr);

#endif // BITCOIN_CONSENSUS_MERKLE_H
