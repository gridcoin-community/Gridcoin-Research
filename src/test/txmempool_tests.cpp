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
#include <node/mempool_persist.h>
#include <fs.h>
#include <gridcoin/beacon.h>
#include <gridcoin/cpid.h>
#include <gridcoin/mrc.h>
#include <gridcoin/sidestake.h>
#include <gridcoin/contract/contract.h>
#include <rpc/server.h>
#include <test/test_gridcoin.h>

#include <univalue.h>

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

// ---------------------------------------------------------------------------
// Phase 3 (#3029): diagnostic RPCs
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(rpc_mempool_diagnostics)
{
    const GRC::Cpid cpid(InsecureRandBytes(16));
    const CTransaction mrc_tx = MakeMrcTx(cpid, 777);
    const std::string txid = mrc_tx.GetHash().ToString();

    mempool.clear();
    mempool.addUnchecked(mrc_tx.GetHash(), MakeEntry(mrc_tx));

    // getmempoolinfo
    const UniValue info = getmempoolinfo(UniValue(UniValue::VARR));
    BOOST_CHECK_EQUAL(info["size"].get_int(), 1);
    BOOST_CHECK_EQUAL(info["mrc_count"].get_int(), 1);
    BOOST_CHECK_EQUAL(info["beacon_count"].get_int(), 0);

    // getrawmempool (non-verbose) -> array of txids
    const UniValue ids = getrawmempool(UniValue(UniValue::VARR));
    BOOST_REQUIRE_EQUAL(ids.size(), 1U);
    BOOST_CHECK_EQUAL(ids[0].get_str(), txid);

    // getrawmempool (verbose) -> object keyed by txid with metadata
    UniValue vparams(UniValue::VARR);
    vparams.push_back(UniValue(true));
    const UniValue verbose = getrawmempool(vparams);
    BOOST_REQUIRE(verbose.exists(txid));
    BOOST_CHECK_EQUAL(verbose[txid]["mrc_cpid"].get_str(), cpid.ToString());

    // getmempoolentry
    UniValue eparams(UniValue::VARR);
    eparams.push_back(UniValue(txid));
    const UniValue entry = getmempoolentry(eparams);
    BOOST_CHECK_EQUAL(entry["height"].get_int(), 0);
    BOOST_CHECK_EQUAL(entry["mrc_cpid"].get_str(), cpid.ToString());

    // getmempoolentry on a missing hash throws.
    UniValue mparams(UniValue::VARR);
    mparams.push_back(UniValue(uint256().ToString()));
    BOOST_CHECK_THROW(getmempoolentry(mparams), UniValue);

    mempool.clear();
}

// ---------------------------------------------------------------------------
// Phase 4 (#3029): bounded size + eviction
// ---------------------------------------------------------------------------

namespace {
//! Distinct plain (no-contract) transaction, made unique by its output value.
CTransaction MakePlainTx(CAmount out_value)
{
    CTxOut vout;
    vout.nValue = out_value;
    return MakeTx({}, {}, {vout});
}

CTxMemPoolEntry MakeEntryFee(const CTransaction& tx, CAmount fee)
{
    return CTxMemPoolEntry(tx, fee, 0, 0, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));
}
} // anonymous namespace

BOOST_AUTO_TEST_CASE(trim_evicts_lowest_feerate_first_then_contracts_last)
{
    CTxMemPool pool;

    const CTransaction lo = MakePlainTx(1);
    const CTransaction mid = MakePlainTx(2);
    const CTransaction hi = MakePlainTx(3);
    pool.addUnchecked(lo.GetHash(), MakeEntryFee(lo, 10));
    pool.addUnchecked(mid.GetHash(), MakeEntryFee(mid, 20));
    pool.addUnchecked(hi.GetHash(), MakeEntryFee(hi, 30));

    // A contract (MRC) tx with no fee -- lowest feerate of all, but protected.
    const CTransaction mrc = MakeMrcTx(GRC::Cpid(InsecureRandBytes(16)), 5);
    pool.addUnchecked(mrc.GetHash(), MakeEntryFee(mrc, 0));

    BOOST_CHECK_EQUAL(pool.size(), 4UL);

    std::vector<CTransaction> removed;
    pool.TrimToSize(0, &removed);

    // Order: non-contract by feerate ascending (lo, mid, hi), contract evicted last.
    BOOST_REQUIRE_EQUAL(removed.size(), 4U);
    BOOST_CHECK(removed[0].GetHash() == lo.GetHash());
    BOOST_CHECK(removed[1].GetHash() == mid.GetHash());
    BOOST_CHECK(removed[2].GetHash() == hi.GetHash());
    BOOST_CHECK(removed[3].GetHash() == mrc.GetHash());

    // Every index drained and consistent.
    BOOST_CHECK_EQUAL(pool.size(), 0UL);
    BOOST_CHECK_EQUAL(pool.m_total_tx_size, 0U);
    BOOST_CHECK(pool.m_eviction_index.empty());
    BOOST_CHECK(pool.m_mrc_by_cpid.empty());
    BOOST_CHECK(pool.m_mrc_by_fee.empty());
}

BOOST_AUTO_TEST_CASE(trim_protects_contract_when_room_for_one)
{
    CTxMemPool pool;

    const CTransaction plain = MakePlainTx(7);
    const CTransaction mrc = MakeMrcTx(GRC::Cpid(InsecureRandBytes(16)), 9);
    pool.addUnchecked(plain.GetHash(), MakeEntryFee(plain, 50)); // higher fee, but not protected
    pool.addUnchecked(mrc.GetHash(), MakeEntryFee(mrc, 0));      // no fee, but a contract

    // Leave room for exactly one entry. The non-contract plain tx is evicted even
    // though it pays more, because contract txns are protected (evicted last).
    const size_t mrc_size = ::GetSerializeSize(mrc, SER_NETWORK, PROTOCOL_VERSION);
    pool.TrimToSize(mrc_size + CTxMemPool::PER_ENTRY_OVERHEAD);

    BOOST_CHECK(!pool.exists(plain.GetHash()));
    BOOST_CHECK(pool.exists(mrc.GetHash()));
    BOOST_CHECK_EQUAL(pool.size(), 1UL);
    BOOST_CHECK_EQUAL(pool.m_eviction_index.size(), 1U);
    BOOST_CHECK_EQUAL(pool.m_mrc_by_cpid.size(), 1U);
}

BOOST_AUTO_TEST_CASE(trim_never_evicts_the_protected_transaction)
{
    CTxMemPool pool;

    // Two transactions in the same feerate tier.
    const CTransaction a = MakePlainTx(11);
    const CTransaction b = MakePlainTx(12);
    pool.addUnchecked(a.GetHash(), MakeEntryFee(a, 10));
    pool.addUnchecked(b.GetHash(), MakeEntryFee(b, 10));

    // Whichever sits at the front of the eviction set is the natural first victim.
    // Protect exactly that one: TrimToSize must evict the OTHER transaction instead
    // and keep the protected one -- this is the AcceptToMemoryPool self-eviction
    // guard that stops a just-accepted tx (newest, so first in the tie-break) from
    // being dropped as its own victim.
    const uint256 natural_victim = pool.m_eviction_index.begin()->hash;

    // Room for exactly one entry (both txs serialize to the same size).
    const size_t one_entry = ::GetSerializeSize(a, SER_NETWORK, PROTOCOL_VERSION)
                             + CTxMemPool::PER_ENTRY_OVERHEAD;
    pool.TrimToSize(one_entry, /*removed=*/nullptr, /*protect=*/&natural_victim);

    BOOST_CHECK(pool.exists(natural_victim));
    BOOST_CHECK_EQUAL(pool.size(), 1UL);
    BOOST_CHECK_EQUAL(pool.m_eviction_index.size(), 1U);
    BOOST_CHECK_EQUAL(pool.m_total_tx_size,
                      ::GetSerializeSize(a, SER_NETWORK, PROTOCOL_VERSION));
}

// ---------------------------------------------------------------------------
// Phase 5 (#3029): unbroadcast set + persistence
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(unbroadcast_tracks_local_origination)
{
    CTxMemPool pool;

    const CTransaction a = MakePlainTx(21);
    const CTransaction b = MakePlainTx(22);

    // AddUnbroadcast is a no-op for a tx that is not in the pool.
    pool.AddUnbroadcast(a.GetHash());
    BOOST_CHECK(pool.GetUnbroadcast().empty());

    // Once the tx is in the pool, it can be marked unbroadcast.
    pool.addUnchecked(a.GetHash(), MakeEntryFee(a, 10));
    pool.addUnchecked(b.GetHash(), MakeEntryFee(b, 10));
    pool.AddUnbroadcast(a.GetHash());
    pool.AddUnbroadcast(b.GetHash());
    BOOST_CHECK_EQUAL(pool.GetUnbroadcast().size(), 2U);

    // Removal (e.g. a peer requested it) drops just that entry.
    pool.RemoveUnbroadcast(a.GetHash());
    const auto set = pool.GetUnbroadcast();
    BOOST_CHECK_EQUAL(set.size(), 1U);
    BOOST_CHECK(set.count(b.GetHash()) == 1);
}

BOOST_AUTO_TEST_CASE(unbroadcast_cleared_when_tx_leaves_pool)
{
    CTxMemPool pool;

    const CTransaction a = MakePlainTx(31);
    pool.addUnchecked(a.GetHash(), MakeEntryFee(a, 10));
    pool.AddUnbroadcast(a.GetHash());
    BOOST_CHECK_EQUAL(pool.GetUnbroadcast().size(), 1U);

    // A tx confirmed/evicted/conflicted out of the pool is no longer ours to
    // rebroadcast: remove() -> eraseIndexes() drops it from the unbroadcast set.
    pool.remove(a, /*fRecursive=*/true, MemPoolRemovalReason::BLOCK);
    BOOST_CHECK(pool.GetUnbroadcast().empty());
}

BOOST_AUTO_TEST_CASE(unbroadcast_persist_roundtrip)
{
    // The on-disk (tx, entry_time) round-trip, exercised without AcceptToMemoryPool
    // (the reload's re-acceptance path is covered by functional testing).
    node::MempoolPersistEntries entries;
    const CTransaction a = MakePlainTx(41);
    const CTransaction b = MakePlainTx(42);
    entries.emplace_back(a, 111);
    entries.emplace_back(b, 222);

    const fs::path path = fs::temp_directory_path() / "gridcoin_unbroadcast_roundtrip.dat";
    BOOST_REQUIRE(node::WriteMempoolEntries(path, entries));

    node::MempoolPersistEntries loaded;
    BOOST_REQUIRE(node::ReadMempoolEntries(path, loaded));
    BOOST_REQUIRE_EQUAL(loaded.size(), 2U);
    BOOST_CHECK(loaded[0].first.GetHash() == a.GetHash());
    BOOST_CHECK_EQUAL(loaded[0].second, 111);
    BOOST_CHECK(loaded[1].first.GetHash() == b.GetHash());
    BOOST_CHECK_EQUAL(loaded[1].second, 222);

    fs::remove(path);
}

BOOST_AUTO_TEST_CASE(unbroadcast_dump_reflects_pool)
{
    // DumpUnbroadcast must persist exactly the pool's unbroadcast members (looked up
    // in mapTx), and nothing else. Exercises the pool->disk half directly.
    CTxMemPool pool;
    const CTransaction ours = MakePlainTx(51);
    const CTransaction other = MakePlainTx(52);
    pool.addUnchecked(ours.GetHash(), MakeEntryFee(ours, 10));
    pool.addUnchecked(other.GetHash(), MakeEntryFee(other, 10));
    pool.AddUnbroadcast(ours.GetHash()); // only `ours` is in the unbroadcast set

    const fs::path path = fs::temp_directory_path() / "gridcoin_unbroadcast_dump.dat";
    BOOST_REQUIRE(node::DumpUnbroadcast(pool, path));

    node::MempoolPersistEntries loaded;
    BOOST_REQUIRE(node::ReadMempoolEntries(path, loaded));
    BOOST_REQUIRE_EQUAL(loaded.size(), 1U); // `other` is in the pool but not the set
    BOOST_CHECK(loaded[0].first.GetHash() == ours.GetHash());

    fs::remove(path);
}

BOOST_AUTO_TEST_CASE(unbroadcast_cleared_on_size_eviction)
{
    // A tx evicted under the -maxmempool cap (#3093 TrimToSize) leaves the pool via
    // remove()->eraseIndexes(), so it must also drop out of the unbroadcast set.
    CTxMemPool pool;
    const CTransaction a = MakePlainTx(51);
    pool.addUnchecked(a.GetHash(), MakeEntryFee(a, 10));
    pool.AddUnbroadcast(a.GetHash());
    BOOST_CHECK_EQUAL(pool.GetUnbroadcast().size(), 1U);

    pool.TrimToSize(0); // evict everything
    BOOST_CHECK(pool.GetUnbroadcast().empty());
    BOOST_CHECK_EQUAL(pool.size(), 0UL);
}

BOOST_AUTO_TEST_CASE(unbroadcast_reset_on_clear)
{
    CTxMemPool pool;
    const CTransaction a = MakePlainTx(61);
    pool.addUnchecked(a.GetHash(), MakeEntryFee(a, 10));
    pool.AddUnbroadcast(a.GetHash());
    BOOST_CHECK_EQUAL(pool.GetUnbroadcast().size(), 1U);

    pool.clear();
    BOOST_CHECK(pool.GetUnbroadcast().empty());
}

BOOST_AUTO_TEST_SUITE_END()
