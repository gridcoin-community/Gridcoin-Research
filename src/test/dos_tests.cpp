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
        CTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = InsecureRand256();
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        AddOrphanTx(tx);
    }

    // ... and 50 that depend on other orphans:
    for (int i = 0; i < 50; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = txPrev.GetHash();
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        BOOST_CHECK(SignSignature(keystore, txPrev, tx, 0));
        AddOrphanTx(tx);
    }

    // This really-big orphan should be ignored:
    for (int i = 0; i < 10; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        tx.vin.resize(500);
        for (unsigned int j = 0; j < tx.vin.size(); j++)
        {
            tx.vin[j].prevout.n = j;
            tx.vin[j].prevout.hash = txPrev.GetHash();
        }
        BOOST_CHECK(SignSignature(keystore, txPrev, tx, 0));
        // Reuse same signature for other inputs
        // (they don't have to be valid for this test)
        for (unsigned int j = 1; j < tx.vin.size(); j++)
            tx.vin[j].scriptSig = tx.vin[0].scriptSig;

        BOOST_CHECK(!AddOrphanTx(tx));
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
        CTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        CValidationState state;
        BOOST_CHECK(!CheckTransaction(tx, state));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 10);
    }

    // Empty vout → DoS 10
    {
        CTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.hash = InsecureRand256();
        tx.vin[0].prevout.n = 0;
        CValidationState state;
        BOOST_CHECK(!CheckTransaction(tx, state));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 10);
    }

    // Negative output value → DoS 100
    {
        CTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.hash = InsecureRand256();
        tx.vin[0].prevout.n = 0;
        tx.vout.resize(1);
        tx.vout[0].nValue = -1;
        CValidationState state;
        BOOST_CHECK(!CheckTransaction(tx, state));
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

BOOST_AUTO_TEST_SUITE_END()
