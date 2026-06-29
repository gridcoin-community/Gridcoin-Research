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

BOOST_AUTO_TEST_SUITE_END()
