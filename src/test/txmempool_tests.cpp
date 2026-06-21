// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Phase 1 tests for the modernized mempool (#3029): verify that
// CTxMemPoolEntry caches the right metadata, that the Gridcoin contract
// tags are extracted from a transaction's contracts, and that adding an
// entry keeps mapNextTx / lookup consistent.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <primitives/transaction.h>
#include <txmempool.h>
#include <uint256.h>
#include <gridcoin/beacon.h>
#include <gridcoin/cpid.h>
#include <gridcoin/mrc.h>
#include <gridcoin/sidestake.h>
#include <gridcoin/contract/contract.h>
#include <test/test_gridcoin.h>

namespace {
//! Build a v2 transaction carrying the supplied contracts (if any).
CTransaction MakeTx(const std::vector<GRC::Contract>& contracts = {},
                    const std::vector<CTxIn>& vin = {},
                    const std::vector<CTxOut>& vout = {})
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin = vin;
    mtx.vout = vout;
    mtx.vContracts = contracts;
    return CTransaction(mtx);
}

//! Build a v2 transaction carrying a single MRC contract for the given CPID.
CTransaction MakeMrcTx(const GRC::Cpid& cpid, CAmount fee, const uint256& last_block = uint256())
{
    GRC::MRC mrc;
    mrc.m_mining_id = cpid;
    mrc.m_fee = fee;
    mrc.m_last_block_hash = last_block;
    return MakeTx({GRC::MakeContract<GRC::MRC>(GRC::ContractAction::ADD, mrc)});
}

CTxMemPoolEntry MakeEntry(const CTransaction& tx)
{
    return CTxMemPoolEntry(tx, 0, 0, 0, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));
}
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(txmempool_tests)

BOOST_AUTO_TEST_CASE(entry_caches_fee_size_time_height)
{
    const CTransaction tx = MakeTx();
    const size_t size = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

    CTxMemPoolEntry entry(tx, /*fee=*/5000, /*time=*/111, /*height=*/222, size);

    BOOST_CHECK_EQUAL(entry.GetFee(), 5000);
    BOOST_CHECK_EQUAL(entry.GetTxSize(), size);
    BOOST_CHECK_EQUAL(entry.GetTime(), 111);
    BOOST_CHECK_EQUAL(entry.GetHeight(), 222);
    BOOST_CHECK_EQUAL(entry.GetFeeRate(), size ? 5000 * 1000 / (CAmount)size : 0);

    // A plain transaction has no contract tags.
    BOOST_CHECK(entry.GetContractTypes().empty());
    BOOST_CHECK(!entry.HasMRC());
    BOOST_CHECK(!entry.HasBeacon());
    BOOST_CHECK(!entry.HasMandatorySidestake());
}

BOOST_AUTO_TEST_CASE(entry_tags_mrc_contract)
{
    const GRC::Cpid cpid(InsecureRandBytes(16));

    GRC::MRC mrc;
    mrc.m_mining_id = cpid;
    mrc.m_fee = 1234;
    mrc.m_last_block_hash = uint256S("0x01");

    const GRC::Contract contract = GRC::MakeContract<GRC::MRC>(GRC::ContractAction::ADD, mrc);
    const CTransaction tx = MakeTx({contract});

    CTxMemPoolEntry entry(tx, 0, 0, 0, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));

    BOOST_CHECK(entry.HasMRC());
    BOOST_CHECK(entry.GetMRCCpid() == cpid);
    BOOST_CHECK_EQUAL(entry.GetMRCFee(), 1234);
    BOOST_CHECK(entry.GetMRCLastBlockHash() == uint256S("0x01"));
    BOOST_CHECK(entry.GetContractTypes().count(GRC::ContractType::MRC) == 1);
    BOOST_CHECK(!entry.HasBeacon());
    BOOST_CHECK(!entry.HasMandatorySidestake());
}

BOOST_AUTO_TEST_CASE(entry_tags_beacon_contract)
{
    const GRC::Cpid cpid(InsecureRandBytes(16));

    CKey key;
    key.MakeNewKey(false);

    const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::ADD, cpid, key.GetPubKey());
    const CTransaction tx = MakeTx({contract});

    CTxMemPoolEntry entry(tx, 0, 0, 0, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));

    BOOST_CHECK(entry.HasBeacon());
    BOOST_CHECK(entry.GetBeaconCpid() == cpid);
    BOOST_CHECK(entry.GetContractTypes().count(GRC::ContractType::BEACON) == 1);
    BOOST_CHECK(!entry.HasMRC());
    BOOST_CHECK(!entry.HasMandatorySidestake());
}

BOOST_AUTO_TEST_CASE(entry_tags_mandatory_sidestake_contract)
{
    GRC::SideStakePayload payload;
    const GRC::Contract contract = GRC::MakeContract<GRC::SideStakePayload>(
        GRC::ContractAction::ADD, payload);
    const CTransaction tx = MakeTx({contract});

    CTxMemPoolEntry entry(tx, 0, 0, 0, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));

    BOOST_CHECK(entry.HasMandatorySidestake());
    BOOST_CHECK(entry.GetContractTypes().count(GRC::ContractType::SIDESTAKE) == 1);
    BOOST_CHECK(!entry.HasMRC());
    BOOST_CHECK(!entry.HasBeacon());
}

BOOST_AUTO_TEST_CASE(addunchecked_preserves_mapnexttx_and_lookup)
{
    CTxIn vin;
    vin.prevout = COutPoint(uint256S("0xbeef"), 0);
    CTxOut vout;
    vout.nValue = 100;

    const CTransaction tx = MakeTx({}, {vin}, {vout});
    const uint256 hash = tx.GetHash();

    CTxMemPool pool;
    CTxMemPoolEntry entry(tx, 0, 0, 0, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));
    BOOST_CHECK(pool.addUnchecked(hash, entry));

    // mapNextTx points back at the stored transaction.
    auto it = pool.mapNextTx.find(vin.prevout);
    BOOST_REQUIRE(it != pool.mapNextTx.end());
    BOOST_REQUIRE(it->second.ptx != nullptr);
    BOOST_CHECK(it->second.ptx->GetHash() == hash);

    // lookup returns the original transaction byte-for-byte.
    CTransaction result;
    BOOST_CHECK(pool.lookup(hash, result));
    BOOST_CHECK(result == tx);
    BOOST_CHECK(pool.exists(hash));
    BOOST_CHECK_EQUAL(pool.size(), 1UL);
}

// ---------------------------------------------------------------------------
// Phase 2 (#3029): contract indexes
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(indexes_populate_on_add_and_tear_down_on_remove)
{
    const GRC::Cpid mrc_cpid(InsecureRandBytes(16));
    const GRC::Cpid beacon_cpid(InsecureRandBytes(16));

    CKey key;
    key.MakeNewKey(false);

    const CTransaction mrc_tx = MakeMrcTx(mrc_cpid, 100);
    const CTransaction beacon_tx = MakeTx(
        {GRC::MakeContract<GRC::BeaconPayload>(GRC::ContractAction::ADD, beacon_cpid, key.GetPubKey())});
    GRC::SideStakePayload ss_payload;
    const CTransaction sidestake_tx = MakeTx(
        {GRC::MakeContract<GRC::SideStakePayload>(GRC::ContractAction::ADD, ss_payload)});

    CTxMemPool pool;
    pool.addUnchecked(mrc_tx.GetHash(), MakeEntry(mrc_tx));
    pool.addUnchecked(beacon_tx.GetHash(), MakeEntry(beacon_tx));
    pool.addUnchecked(sidestake_tx.GetHash(), MakeEntry(sidestake_tx));

    BOOST_CHECK_EQUAL(pool.m_mrc_by_cpid.size(), 1U);
    BOOST_CHECK_EQUAL(pool.m_mrc_by_fee.size(), 1U);
    BOOST_CHECK_EQUAL(pool.m_beacon_by_cpid.size(), 1U);
    BOOST_CHECK_EQUAL(pool.m_mandatory_sidestake_count, 1U);

    pool.remove(mrc_tx);
    pool.remove(beacon_tx);
    pool.remove(sidestake_tx);

    BOOST_CHECK(pool.m_mrc_by_cpid.empty());
    BOOST_CHECK(pool.m_mrc_by_fee.empty());
    BOOST_CHECK(pool.m_beacon_by_cpid.empty());
    BOOST_CHECK_EQUAL(pool.m_mandatory_sidestake_count, 0U);
    BOOST_CHECK_EQUAL(pool.size(), 0UL);
}

BOOST_AUTO_TEST_CASE(hasmrcforcpid_reports_collision_hash)
{
    const GRC::Cpid cpid_a(InsecureRandBytes(16));
    const GRC::Cpid cpid_b(InsecureRandBytes(16));

    const CTransaction mrc_tx = MakeMrcTx(cpid_a, 50);

    CTxMemPool pool;
    pool.addUnchecked(mrc_tx.GetHash(), MakeEntry(mrc_tx));

    uint256 existing;
    BOOST_CHECK(pool.HasMRCForCpid(cpid_a, &existing));
    BOOST_CHECK(existing == mrc_tx.GetHash());
    BOOST_CHECK(!pool.HasMRCForCpid(cpid_b));
}

BOOST_AUTO_TEST_CASE(getmrcqueue_is_fee_descending)
{
    CTxMemPool pool;
    for (CAmount fee : {10, 30, 20}) {
        const CTransaction tx = MakeMrcTx(GRC::Cpid(InsecureRandBytes(16)), fee);
        pool.addUnchecked(tx.GetHash(), MakeEntry(tx));
    }

    const auto queue = pool.GetMRCQueue();
    BOOST_REQUIRE_EQUAL(queue.size(), 3U);
    BOOST_CHECK_EQUAL(queue[0].first, 30);
    BOOST_CHECK_EQUAL(queue[1].first, 20);
    BOOST_CHECK_EQUAL(queue[2].first, 10);
}

BOOST_AUTO_TEST_CASE(getstalemrcs_selects_by_anchor_block)
{
    const uint256 best = uint256S("0xaa");
    const uint256 old = uint256S("0xbb");

    const CTransaction fresh_tx = MakeMrcTx(GRC::Cpid(InsecureRandBytes(16)), 10, best);
    const CTransaction stale_tx = MakeMrcTx(GRC::Cpid(InsecureRandBytes(16)), 10, old);

    CTxMemPool pool;
    pool.addUnchecked(fresh_tx.GetHash(), MakeEntry(fresh_tx));
    pool.addUnchecked(stale_tx.GetHash(), MakeEntry(stale_tx));

    const auto stale = pool.GetStaleMRCs(best);
    BOOST_REQUIRE_EQUAL(stale.size(), 1U);
    BOOST_CHECK(stale[0] == stale_tx.GetHash());
}

BOOST_AUTO_TEST_SUITE_END()
