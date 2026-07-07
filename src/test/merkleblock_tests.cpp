// Copyright (c) 2012-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "merkleblock.h"
#include "consensus/merkle.h"
#include "random.h"
#include "streams.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cmath>

BOOST_AUTO_TEST_SUITE(merkleblock_tests)

/**
 * Subclass exposing the internal hashes so a test can deliberately corrupt the
 * tree and confirm the authentication breaks.
 */
class CPartialMerkleTreeTester : public CPartialMerkleTree
{
public:
    // flip one bit in one of the hashes - this should break the authentication
    void Damage(FastRandomContext& rng)
    {
        unsigned int n = rng.randrange(vHash.size());
        int bit = rng.randbits(8);
        *(vHash[n].begin() + (bit >> 3)) ^= 1 << (bit & 7);
    }
};

BOOST_AUTO_TEST_CASE(pmt_test1)
{
    FastRandomContext rng(/*fDeterministic=*/true);

    static const unsigned int nTxCounts[] = {1, 4, 7, 17, 56, 100, 127, 256, 312, 513, 1000, 4095};

    for (int i = 0; i < 12; i++) {
        unsigned int nTx = nTxCounts[i];

        // build a list of dummy txids
        std::vector<uint256> vTxid(nTx, uint256());
        for (unsigned int j = 0; j < nTx; j++) {
            vTxid[j] = rng.rand256();
        }

        // calculate actual merkle root (reuse the consensus implementation)
        bool unused = false;
        uint256 merkleRoot1 = ComputeMerkleRoot(vTxid, &unused);

        std::vector<bool> vMatch;
        std::vector<uint256> vMatchTxid1;

        for (unsigned int att = 1; att < 15; att++) {
            // build random subset of txids
            vMatch.assign(nTx, false);
            vMatchTxid1.clear();
            for (unsigned int j = 0; j < nTx; j++) {
                bool fInclude = rng.randbits(att / 2) == 0;
                vMatch[j] = fInclude;
                if (fInclude) {
                    vMatchTxid1.push_back(vTxid[j]);
                }
            }

            // build the partial merkle tree
            CPartialMerkleTree pmt1(vTxid, vMatch);

            // serialize
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << pmt1;

            // verify CPartialMerkleTree's size guarantees (height is ceil(log2(nTx)))
            unsigned int n = std::min<unsigned int>(nTx, 1 + vMatchTxid1.size() * (unsigned int)std::ceil(std::log2((double)nTx)));
            BOOST_CHECK(ss.size() <= 10 + (258 * n + 7) / 8);

            // deserialize into a tester copy
            CPartialMerkleTreeTester pmt2;
            ss >> pmt2;

            // extract merkle root and matched txids from copy
            std::vector<uint256> vMatchTxid2;
            std::vector<unsigned int> vIndex;
            uint256 merkleRoot2 = pmt2.ExtractMatches(vMatchTxid2, vIndex);

            // check that it has the same merkle root as the original, and a valid one
            BOOST_CHECK(merkleRoot1 == merkleRoot2);
            BOOST_CHECK(!merkleRoot2.IsNull());

            // check that it contains the matched transactions (in the same order)
            BOOST_CHECK(vMatchTxid1 == vMatchTxid2);

            // check that random bit flips break the authentication
            for (int j = 0; j < 4; j++) {
                CPartialMerkleTreeTester pmt3(pmt2);
                pmt3.Damage(rng);
                std::vector<uint256> vMatchTxid3;
                uint256 merkleRoot3 = pmt3.ExtractMatches(vMatchTxid3, vIndex);
                BOOST_CHECK(merkleRoot3 != merkleRoot1);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(pmt_malleability)
{
    std::vector<uint256> vTxid = {
        uint256{1}, uint256{2},
        uint256{3}, uint256{4},
        uint256{5}, uint256{6},
        uint256{7}, uint256{8},
        uint256{9}, uint256{10},
        uint256{9}, uint256{10},
    };
    std::vector<bool> vMatch = {false, false, false, false, false, false, false, false, false, true, true, false};

    CPartialMerkleTree tree(vTxid, vMatch);
    std::vector<unsigned int> vIndex;
    std::vector<uint256> vTxid2;
    // The malleated tree (duplicated leaves 9 and 10) must be rejected: ExtractMatches
    // returns a null root because the duplicate-subtree guard trips.
    BOOST_CHECK(tree.ExtractMatches(vTxid2, vIndex).IsNull());
}

// Regression test for the verifytxoutproof inclusion-forgery: a proof that claims
// nTransactions=1 with the single leaf set to a real multi-tx block's merkle ROOT
// makes ExtractMatches surface that INTERNAL node as both the computed root and a
// "matched txid" -- a forged inclusion claim. The defense is a shape cross-check:
// the proof's GetNumTransactions() must equal the real block's transaction count.
BOOST_AUTO_TEST_CASE(pmt_forged_internal_node_shape_mismatch)
{
    FastRandomContext rng(/*fDeterministic=*/true);

    // A real multi-transaction block and its merkle root.
    const unsigned int nTx = 8;
    std::vector<uint256> vTxid(nTx);
    for (unsigned int j = 0; j < nTx; j++)
        vTxid[j] = rng.rand256();
    bool unused = false;
    uint256 root = ComputeMerkleRoot(vTxid, &unused);

    // Forge a single-"transaction" proof whose one leaf is that merkle root.
    CPartialMerkleTree forged(std::vector<uint256>{root}, std::vector<bool>{true});

    std::vector<uint256> vMatch;
    std::vector<unsigned int> vIndex;
    uint256 extracted = forged.ExtractMatches(vMatch, vIndex);

    // The forgery passes the merkle self-check: the internal node comes back as
    // both the computed root and the single matched "txid".
    BOOST_CHECK(extracted == root);
    BOOST_REQUIRE_EQUAL(vMatch.size(), 1u);
    BOOST_CHECK(vMatch[0] == root);

    // ...but the proof's declared transaction count does not match the real
    // block, which is exactly what verifytxoutproof now rejects on.
    BOOST_CHECK_EQUAL(forged.GetNumTransactions(), 1u);
    BOOST_CHECK(forged.GetNumTransactions() != nTx);
}

BOOST_AUTO_TEST_SUITE_END()
