// Copyright (c) 2015-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <consensus/merkle.h>
#include "random.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(merkle_tests)

/* This implements a constant-space merkle root/path calculator, limited to 2^32 leaves. */
static void MerkleComputation(const std::vector<uint256>& leaves, uint256* proot, bool* pmutated, uint32_t branchpos, std::vector<uint256>* pbranch) {
    if (pbranch) pbranch->clear();
    if (leaves.size() == 0) {
        if (pmutated) *pmutated = false;
        if (proot) *proot = uint256();
        return;
    }
    bool mutated = false;
    // count is the number of leaves processed so far.
    uint32_t count = 0;
    // inner is an array of eagerly computed subtree hashes, indexed by tree
    // level (0 being the leaves).
    // For example, when count is 25 (11001 in binary), inner[4] is the hash of
    // the first 16 leaves, inner[3] of the next 8 leaves, and inner[0] equal to
    // the last leaf. The other inner entries are undefined.
    uint256 inner[32];
    // Which position in inner is a hash that depends on the matching leaf.
    int matchlevel = -1;
    // First process all leaves into 'inner' values.
    while (count < leaves.size()) {
        uint256 h = leaves[count];
        bool matchh = count == branchpos;
        count++;
        int level;
        // For each of the lower bits in count that are 0, do 1 step. Each
        // corresponds to an inner value that existed before processing the
        // current leaf, and each needs a hash to combine it.
        for (level = 0; !(count & (((uint32_t)1) << level)); level++) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            mutated |= (inner[level] == h);
            CHash256().Write(inner[level]).Write(h).Finalize(h);
        }
        // Store the resulting hash at inner position level.
        inner[level] = h;
        if (matchh) {
            matchlevel = level;
        }
    }
    // Do a final 'sweep' over the rightmost branch of the tree to process
    // odd levels, and reduce everything to a single top value.
    // Level is the level (counted from the bottom) up to which we've sweeped.
    int level = 0;
    // As long as bit number level in count is zero, skip it. It means there
    // is nothing left at this level.
    while (!(count & (((uint32_t)1) << level))) {
        level++;
    }
    uint256 h = inner[level];
    bool matchh = matchlevel == level;
    while (count != (((uint32_t)1) << level)) {
        // If we reach this point, h is an inner value that is not the top.
        // We combine it with itself (Bitcoin's special rule for odd levels in
        // the tree) to produce a higher level one.
        if (pbranch && matchh) {
            pbranch->push_back(h);
        }
        CHash256().Write(h).Write(h).Finalize(h);
        // Increment count to the value it would have if two entries at this
        // level had existed.
        count += (((uint32_t)1) << level);
        level++;
        // And propagate the result upwards accordingly.
        while (!(count & (((uint32_t)1) << level))) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            CHash256().Write(inner[level]).Write(h).Finalize(h);
            level++;
        }
    }
    // Return result.
    if (pmutated) *pmutated = mutated;
    if (proot) *proot = h;
}

uint256 ComputeMerkleRoot(const std::vector<uint256>& leaves, bool* mutated) {
    uint256 hash;
    MerkleComputation(leaves, &hash, mutated, -1, nullptr);
    return hash;
}

std::vector<uint256> ComputeMerkleBranch(const std::vector<uint256>& leaves, uint32_t position) {
    std::vector<uint256> ret;
    MerkleComputation(leaves, nullptr, nullptr, position, &ret);
    return ret;
}

uint256 ComputeMerkleRootFromBranch(const uint256& leaf, const std::vector<uint256>& vMerkleBranch, uint32_t nIndex) {
    uint256 hash = leaf;
    for (std::vector<uint256>::const_iterator it = vMerkleBranch.begin(); it != vMerkleBranch.end(); ++it) {
        if (nIndex & 1) {
            hash = Hash(*it, hash);
        } else {
            hash = Hash(hash, *it);
        }
        nIndex >>= 1;
    }
    return hash;
}

std::vector<uint256> BlockMerkleBranch(const CBlock& block, uint32_t position)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s].GetHash();
    }
    return ComputeMerkleBranch(leaves, position);
}

// Older version of the merkle root computation code, for comparison.
static uint256 BlockBuildMerkleTree(const CBlock& block, std::vector<uint256>& vMerkleTree)
{
    vMerkleTree.clear();
    for (auto const& tx : block.vtx)
        vMerkleTree.push_back(tx.GetHash());
    int j = 0;
    for (int nSize = block.vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            vMerkleTree.push_back(Hash(vMerkleTree[j+i], vMerkleTree[j+i2]));
        }
        j += nSize;
    }
    return (vMerkleTree.empty() ? uint256() : vMerkleTree.back());
}


static inline int ctz(uint32_t i) {
    if (i == 0) return 0;
    int j = 0;
    while (!(i & 1)) {
        j++;
        i >>= 1;
    }
    return j;
}

BOOST_AUTO_TEST_CASE(merkle_test)
{
    for (int i = 0; i < 32; i++) {
        // Try 32 block sizes: all sizes from 0 to 16 inclusive, and then 15 random sizes.
        int ntx = (i <= 16) ? i : 17 + (GetRand(4000));
        // Try up to 3 mutations.
        for (int mutate = 0; mutate <= 3; mutate++) {
            int duplicate1 = mutate >= 1 ? 1 << ctz(ntx) : 0; // The last how many transactions to duplicate first.
            if (duplicate1 >= ntx) break; // Duplication of the entire tree results in a different root (it adds a level).
            int ntx1 = ntx + duplicate1; // The resulting number of transactions after the first duplication.
            int duplicate2 = mutate >= 2 ? 1 << ctz(ntx1) : 0; // Likewise for the second mutation.
            if (duplicate2 >= ntx1) break;
            int ntx2 = ntx1 + duplicate2;
            int duplicate3 = mutate >= 3 ? 1 << ctz(ntx2) : 0; // And for the third mutation.
            if (duplicate3 >= ntx2) break;
            int ntx3 = ntx2 + duplicate3;
            // Build a block with ntx different transactions.
            CBlock block;
            block.vtx.resize(ntx);
            for (int j = 0; j < ntx; j++) {
                CTransaction tx;
                tx.nLockTime = j;
                block.vtx[j] = tx;
            }
            // Compute the root of the block before mutating it.
            bool unmutatedMutated = false;
            uint256 unmutatedRoot = BlockMerkleRoot(block, &unmutatedMutated);
            BOOST_CHECK(unmutatedMutated == false);
            // Optionally mutate by duplicating the last transactions, resulting in the same merkle root.
            block.vtx.resize(ntx3);
            for (int j = 0; j < duplicate1; j++) {
                block.vtx[ntx + j] = block.vtx[ntx + j - duplicate1];
            }
            for (int j = 0; j < duplicate2; j++) {
                block.vtx[ntx1 + j] = block.vtx[ntx1 + j - duplicate2];
            }
            for (int j = 0; j < duplicate3; j++) {
                block.vtx[ntx2 + j] = block.vtx[ntx2 + j - duplicate3];
            }
            // Compute the merkle root and merkle tree using the old mechanism.
            std::vector<uint256> merkleTree;
            uint256 oldRoot = BlockBuildMerkleTree(block, merkleTree);
            // Compute the merkle root using the new mechanism.
            bool newMutated = false;
            uint256 newRoot = BlockMerkleRoot(block, &newMutated);
            BOOST_CHECK(oldRoot == newRoot);
            BOOST_CHECK(newRoot == unmutatedRoot);
            BOOST_CHECK((newRoot == uint256()) == (ntx == 0));
            BOOST_CHECK(newMutated == !!mutate);
        }
    }
}


BOOST_AUTO_TEST_CASE(merkle_test_empty_block)
{
    bool mutated = false;
    CBlock block;
    uint256 root = BlockMerkleRoot(block, &mutated);

    BOOST_CHECK_EQUAL(root.IsNull(), true);
    BOOST_CHECK_EQUAL(mutated, false);
}

BOOST_AUTO_TEST_CASE(merkle_test_oneTx_block)
{
    bool mutated = false;
    CBlock block;

    block.vtx.resize(1);
    CTransaction tx;
    tx.nLockTime = 0;
    block.vtx[0] = tx;
    uint256 root = BlockMerkleRoot(block, &mutated);
    BOOST_CHECK(root == block.vtx[0].GetHash());
    BOOST_CHECK_EQUAL(mutated, false);
}

BOOST_AUTO_TEST_CASE(merkle_test_OddTxWithRepeatedLastTx_block)
{
    bool mutated;
    CBlock block, blockWithRepeatedLastTx;

    block.vtx.resize(3);

    for (std::size_t pos = 0; pos < block.vtx.size(); pos++) {
        CTransaction tx;
        tx.nLockTime = pos;
        block.vtx[pos] = tx;
    }

    blockWithRepeatedLastTx = block;
    blockWithRepeatedLastTx.vtx.push_back(blockWithRepeatedLastTx.vtx.back());

    uint256 rootofBlock = BlockMerkleRoot(block, &mutated);
    BOOST_CHECK_EQUAL(mutated, false);

    uint256 rootofBlockWithRepeatedLastTx = BlockMerkleRoot(blockWithRepeatedLastTx, &mutated);
    BOOST_CHECK(rootofBlock == rootofBlockWithRepeatedLastTx);
    BOOST_CHECK_EQUAL(mutated, true);
}

BOOST_AUTO_TEST_CASE(merkle_test_LeftSubtreeRightSubtree)
{
    CBlock block, leftSubtreeBlock, rightSubtreeBlock;

    block.vtx.resize(4);
    std::size_t pos;
    for (pos = 0; pos < block.vtx.size(); pos++) {
        CTransaction tx;
        tx.nLockTime = pos;
        block.vtx[pos] = tx;
    }

    for (pos = 0; pos < block.vtx.size() / 2; pos++)
        leftSubtreeBlock.vtx.push_back(block.vtx[pos]);

    for (pos = block.vtx.size() / 2; pos < block.vtx.size(); pos++)
        rightSubtreeBlock.vtx.push_back(block.vtx[pos]);

    uint256 root = BlockMerkleRoot(block);
    uint256 rootOfLeftSubtree = BlockMerkleRoot(leftSubtreeBlock);
    uint256 rootOfRightSubtree = BlockMerkleRoot(rightSubtreeBlock);
    std::vector<uint256> leftRight;
    leftRight.push_back(rootOfLeftSubtree);
    leftRight.push_back(rootOfRightSubtree);
    uint256 rootOfLR = ComputeMerkleRoot(leftRight);

    BOOST_CHECK(root == rootOfLR);
}

BOOST_AUTO_TEST_SUITE_END()
