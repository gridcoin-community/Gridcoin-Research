//
// Unit tests for denial-of-service detection/prevention code
//
#include <algorithm>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/test/unit_test.hpp>

#include "main.h"
#include "wallet/wallet.h"
#include "net.h"
#include "util.h"
#include "random.h"
#include "banman.h"

#include <test/test_gridcoin.h>

#include <stdint.h>

// Tests this internal-to-main.cpp method:
extern bool AddOrphanTx(const CTransaction& tx);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
extern std::map<uint256, CTransaction> mapOrphanTransactions;
extern std::map<uint256, std::set<uint256> > mapOrphanTransactionsByPrev;

CService ip(uint32_t i)
{
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), GetDefaultPort());
}

BOOST_AUTO_TEST_SUITE(DoS_tests)

BOOST_AUTO_TEST_CASE(DoS_banning)
{
    g_banman->ClearBanned();
    CAddress addr1(ip(0xa0b0c001));
    CNode dummyNode1(INVALID_SOCKET, addr1, "", true);
    dummyNode1.Misbehaving(100); // Should get banned
    BOOST_CHECK(g_banman->IsBanned(addr1));
    BOOST_CHECK(!g_banman->IsBanned(ip(0xa0b0c001|0x0000ff00))); // Different IP, not banned

    CAddress addr2(ip(0xa0b0c002));
    CNode dummyNode2(INVALID_SOCKET, addr2, "", true);
    dummyNode2.Misbehaving(50);
    BOOST_CHECK(!g_banman->IsBanned(addr2)); // 2 not banned yet...
    BOOST_CHECK(g_banman->IsBanned(addr1));  // ... but 1 still should be
    dummyNode2.Misbehaving(50);
    BOOST_CHECK(g_banman->IsBanned(addr2));
}

BOOST_AUTO_TEST_CASE(DoS_banscore)
{
    CNodeStats nodestats;
    g_banman->ClearBanned();
    gArgs.ForceSetArg("-banscore", "111"); // because 11 is my favorite number
    CAddress addr1(ip(0xa0b0c001));
    CNode dummyNode1(INVALID_SOCKET, addr1, "", true);
    dummyNode1.Misbehaving(100);
    BOOST_CHECK(!g_banman->IsBanned(addr1));
    dummyNode1.copyStats(nodestats);
    BOOST_CHECK(nodestats.nMisbehavior == 100); // nMisbehavior should be 100.
    dummyNode1.Misbehaving(10);
    BOOST_CHECK(!g_banman->IsBanned(addr1));
    dummyNode1.copyStats(nodestats);
    BOOST_CHECK(nodestats.nMisbehavior == 110); // nMisbehavior should be 110.
    dummyNode1.Misbehaving(1);
    BOOST_CHECK(g_banman->IsBanned(addr1));

    // TODO: Why no ClearArg?
    gArgs.ForceSetArg("-banscore", "100");
}

BOOST_AUTO_TEST_CASE(DoS_bantime)
{
    CNodeStats nodestats;
    g_banman->ClearBanned();
    int64_t nStartTime = GetTime();
    SetMockTime(nStartTime); // Overrides future calls to GetTime()

    CAddress addr(ip(0xa0b0c001));
    CNode dummyNode(INVALID_SOCKET, addr, "", true);

    dummyNode.Misbehaving(100);
    BOOST_CHECK(g_banman->IsBanned(addr));

    SetMockTime(nStartTime+60*60);
    BOOST_CHECK(g_banman->IsBanned(addr));

    SetMockTime(nStartTime+60*60*24+1);
    BOOST_CHECK(!g_banman->IsBanned(addr));
    dummyNode.copyStats(nodestats);
    BOOST_CHECK(!nodestats.nMisbehavior); // nMisbehavior should be back to zero.
}

BOOST_AUTO_TEST_CASE(DoS_misbehavior_decay)
{
    CNodeStats nodestats;
    g_banman->ClearBanned();
    int64_t nStartTime = GetTime();
    SetMockTime(nStartTime); // Overrides future calls to GetTime()

    CAddress addr(ip(0xa0b0c001));
    CNode dummyNode(INVALID_SOCKET, addr, "", true);
    dummyNode.Misbehaving(50);
    dummyNode.copyStats(nodestats);
    BOOST_CHECK(nodestats.nMisbehavior == 50); // nMisbehavior should be 50.

    SetMockTime(nStartTime + 431); // Advance time 431 seconds.
    dummyNode.copyStats(nodestats);
    BOOST_CHECK(nodestats.nMisbehavior == 50); // nMisbehavior should still be 50.

    SetMockTime(nStartTime + 432); // Advance time 431 seconds.
    dummyNode.copyStats(nodestats);
    BOOST_CHECK(nodestats.nMisbehavior == 49); // nMisbehavior should still be 49.

    SetMockTime(nStartTime + 6*60*60); // Advance time 6 hours.
    dummyNode.copyStats(nodestats);
    BOOST_CHECK(nodestats.nMisbehavior == 25); // nMisbehavior should be 25.

    SetMockTime(nStartTime + 12*60*60); // Advance time 12 hours.
    dummyNode.copyStats(nodestats);
    BOOST_CHECK(nodestats.nMisbehavior == 0); // nMisbehavior should be 0.
}

CTransaction RandomOrphan()
{
    auto it = mapOrphanTransactions.lower_bound(InsecureRand256());
    if (it == mapOrphanTransactions.end())
        it = mapOrphanTransactions.begin();
    return it->second;
}

BOOST_AUTO_TEST_CASE(DoS_mapOrphans)
{
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    BOOST_CHECK(keystore.AddKey(key));

    // 50 orphan transactions:
    for (int i = 0; i < 50; i++)
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.n = 0;
        mtx.vin[0].prevout.hash = InsecureRand256();
        mtx.vin[0].scriptSig << OP_1;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 1*CENT;
        mtx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        AddOrphanTx(CTransaction(mtx));
    }

    // ... and 50 that depend on other orphans:
    for (int i = 0; i < 50; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.n = 0;
        mtx.vin[0].prevout.hash = txPrev.GetHash();
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 1*CENT;
        mtx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        BOOST_CHECK(SignSignature(keystore, txPrev, mtx, 0));
        AddOrphanTx(CTransaction(mtx));
    }

    // This really-big orphan should be ignored:
    for (int i = 0; i < 10; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CMutableTransaction mtx;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 1*CENT;
        mtx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        mtx.vin.resize(500);
        for (unsigned int j = 0; j < mtx.vin.size(); j++)
        {
            mtx.vin[j].prevout.n = j;
            mtx.vin[j].prevout.hash = txPrev.GetHash();
        }
        BOOST_CHECK(SignSignature(keystore, txPrev, mtx, 0));
        // Reuse same signature for other inputs
        // (they don't have to be valid for this test)
        for (unsigned int j = 1; j < mtx.vin.size(); j++)
            mtx.vin[j].scriptSig = mtx.vin[0].scriptSig;

        BOOST_CHECK(!AddOrphanTx(CTransaction(mtx)));
    }

    // Test LimitOrphanTxSize() function:
    LimitOrphanTxSize(40);
    BOOST_CHECK(mapOrphanTransactions.size() <= 40);
    LimitOrphanTxSize(10);
    BOOST_CHECK(mapOrphanTransactions.size() <= 10);
    LimitOrphanTxSize(0);
    BOOST_CHECK(mapOrphanTransactions.empty());
    BOOST_CHECK(mapOrphanTransactionsByPrev.empty());
}

BOOST_AUTO_TEST_CASE(DoS_validation_state)
{
    // --- Part 1: CValidationState unit behavior ---

    // Fresh state is valid, DoS = 0
    {
        CValidationState state;
        BOOST_CHECK(state.IsValid());
        BOOST_CHECK(!state.IsInvalid());
        BOOST_CHECK(!state.IsError());
        BOOST_CHECK_EQUAL(state.GetDoS(), 0);
        int nDoS = -1;
        BOOST_CHECK(!state.IsInvalid(nDoS));
        BOOST_CHECK_EQUAL(nDoS, -1); // unchanged when valid
    }

    // DoS() sets INVALID mode and accumulates score
    {
        CValidationState state;
        state.DoS(10, false, "reason1");
        BOOST_CHECK(!state.IsValid());
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 10);
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "reason1");

        // Accumulates
        state.DoS(25, false, "reason2");
        BOOST_CHECK_EQUAL(state.GetDoS(), 35);

        // IsInvalid(nDoS) extracts accumulated score
        int nDoS = 0;
        BOOST_CHECK(state.IsInvalid(nDoS));
        BOOST_CHECK_EQUAL(nDoS, 35);
    }

    // Invalid() sets INVALID with DoS = 0
    {
        CValidationState state;
        state.Invalid(false, "soft-invalid");
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 0);
    }

    // Error() sets ERROR mode (not INVALID)
    {
        CValidationState state;
        state.Error("runtime-error");
        BOOST_CHECK(state.IsError());
        BOOST_CHECK(!state.IsInvalid());
        BOOST_CHECK(!state.IsValid());
        int nDoS = -1;
        BOOST_CHECK(!state.IsInvalid(nDoS));
        BOOST_CHECK_EQUAL(nDoS, -1);
    }

    // --- Part 2: CheckTransaction sets DoS via state ---

    // Empty vin → DoS 10
    {
        CMutableTransaction mtx;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 1 * CENT;
        CValidationState state;
        BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 10);
    }

    // Empty vout → DoS 10
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.hash = InsecureRand256();
        mtx.vin[0].prevout.n = 0;
        CValidationState state;
        BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 10);
    }

    // Negative output value → DoS 100
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.hash = InsecureRand256();
        mtx.vin[0].prevout.n = 0;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = -1;
        CValidationState state;
        BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }

    // --- Part 3: P2P glue pattern works ---
    // Simulates the exact code path from main.cpp ProcessMessage("tx"/BLOCK)
    {
        CTransaction tx; // empty vin+vout
        CValidationState state;
        CheckTransaction(tx, state);

        int nDoS = 0;
        if (state.IsInvalid(nDoS) && nDoS > 0)
        {
            // This would call pfrom->Misbehaving(nDoS) in production
            BOOST_CHECK(nDoS > 0);
        }
        else
        {
            BOOST_ERROR("Expected IsInvalid with nDoS > 0 for empty transaction");
        }
    }
}

// Helper: create a minimal block that passes CheckBlock structural checks.
// Uses height=0 (≤ nGrandfather) and disables PoW/merkle/sig checks.
// The block has a single coinbase tx with a valid scriptSig.
static CBlock CreateMinimalBlock()
{
    CBlock block;
    block.nVersion = 10; // pre-contract version avoids claim contract checks
    block.hashPrevBlock = uint256S("0x1"); // non-null to skip genesis bypass
    block.nTime = GetAdjustedTime();
    block.nBits = 0x1d00ffff;

    CMutableTransaction mtxCoinbase;
    mtxCoinbase.vin.resize(1);
    mtxCoinbase.vin[0].prevout.SetNull();
    mtxCoinbase.vin[0].scriptSig = CScript() << 0 << 0;
    mtxCoinbase.vout.resize(1);
    mtxCoinbase.vout[0].nValue = 0;
    block.vtx.push_back(CTransaction(std::move(mtxCoinbase)));

    return block;
}

BOOST_AUTO_TEST_CASE(DoS_checkblock_validation_state)
{
    // All tests use height=0 and disable PoW/merkle/sig to isolate structural checks.
    // CheckBlock requires cs_main.

    // Baseline: minimal block passes
    {
        LOCK(cs_main);
        CBlock block = CreateMinimalBlock();
        CValidationState state;
        BOOST_CHECK_MESSAGE(
            CheckBlock(block, state, 0, false, false, false),
            "Minimal valid block should pass CheckBlock");
    }

    // Empty vtx → DoS 100 (size limits)
    {
        LOCK(cs_main);
        CBlock block = CreateMinimalBlock();
        block.vtx.clear();
        CValidationState state;
        BOOST_CHECK(!CheckBlock(block, state, 0, false, false, false));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }

    // First tx not coinbase → DoS 100
    {
        LOCK(cs_main);
        CBlock block = CreateMinimalBlock();
        // Replace coinbase with a regular tx (non-null prevout)
        CMutableTransaction mtxReplace(block.vtx[0]);
        mtxReplace.vin[0].prevout.hash = uint256S("0xdead");
        mtxReplace.vin[0].prevout.n = 0;
        block.vtx[0] = CTransaction(std::move(mtxReplace));
        CValidationState state;
        BOOST_CHECK(!CheckBlock(block, state, 0, false, false, false));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }

    // Multiple coinbases → DoS 100
    {
        LOCK(cs_main);
        CBlock block = CreateMinimalBlock();
        block.vtx.push_back(block.vtx[0]); // duplicate coinbase
        CValidationState state;
        BOOST_CHECK(!CheckBlock(block, state, 0, false, false, false));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }

    // Duplicate transactions → DoS 100
    {
        LOCK(cs_main);
        CBlock block = CreateMinimalBlock();
        // Add a regular tx, then duplicate it
        CMutableTransaction mtxDup;
        mtxDup.vin.resize(1);
        mtxDup.vin[0].prevout.hash = uint256S("0xbeef");
        mtxDup.vin[0].prevout.n = 0;
        mtxDup.vout.resize(1);
        mtxDup.vout[0].nValue = 1 * CENT;
        CTransaction tx(mtxDup);
        block.vtx.push_back(tx);
        block.vtx.push_back(tx); // same hash → duplicate
        CValidationState state;
        BOOST_CHECK(!CheckBlock(block, state, 0, false, false, false));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }

    // Merkle root mismatch → DoS 100 (enable fCheckMerkleRoot)
    {
        LOCK(cs_main);
        CBlock block = CreateMinimalBlock();
        block.hashMerkleRoot = uint256S("0xbad"); // wrong merkle root
        CValidationState state;
        BOOST_CHECK(!CheckBlock(block, state, 0, false, /*fCheckMerkleRoot=*/true, false));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }

    // Block with invalid transaction propagates DoS from CheckTransaction
    {
        LOCK(cs_main);
        CBlock block = CreateMinimalBlock();
        // Add a tx with negative output value → CheckTransaction assigns DoS 100
        CMutableTransaction mtxBad;
        mtxBad.vin.resize(1);
        mtxBad.vin[0].prevout.hash = uint256S("0xcafe");
        mtxBad.vin[0].prevout.n = 0;
        mtxBad.vout.resize(1);
        mtxBad.vout[0].nValue = -1;
        block.vtx.push_back(CTransaction(std::move(mtxBad)));
        CValidationState state;
        BOOST_CHECK(!CheckBlock(block, state, 0, false, false, false));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }
}

BOOST_AUTO_TEST_SUITE_END()
