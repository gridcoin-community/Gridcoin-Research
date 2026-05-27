// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/orphan_blocks.h"

#include <boost/test/unit_test.hpp>

// Tests construct OrphanBlockManager instances locally and exercise them
// single-threaded. The handler methods are EXCLUSIVE_LOCKS_REQUIRED(cs_main)
// under the thread-safety annotation rollout; suppress the analyzer for
// this file rather than take a lock the tests do not need.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

namespace {

//! Create a minimal test block with a given previous hash and a unique nTime
//! so that each block produces a distinct hash. nVersion is set to 14 so that
//! GetHash() uses SerializeHash (header-only, no scrypt).
CBlock MakeTestBlock(const uint256& prev_hash, uint32_t unique_id)
{
    CBlock block;
    block.nVersion = 14;
    block.hashPrevBlock = prev_hash;
    block.nTime = 1700000000 + unique_id;
    block.nBits = 0x1d00ffff;
    return block;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(orphan_block_tests)

// ---------------------------------------------------------------------------
// Basic add / contains / size
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(add_and_contains)
{
    OrphanBlockManager mgr;

    CBlock block = MakeTestBlock(uint256(1), 1);
    uint256 hash = block.GetHash();

    BOOST_CHECK(mgr.Add(hash, block, 1000));
    BOOST_CHECK(mgr.Contains(hash));
    BOOST_CHECK_EQUAL(mgr.Size(), 1u);

    // Duplicate rejected.
    BOOST_CHECK(!mgr.Add(hash, block, 1000));
    BOOST_CHECK_EQUAL(mgr.Size(), 1u);

    mgr.Clear();
}

BOOST_AUTO_TEST_CASE(has_children_of)
{
    OrphanBlockManager mgr;

    uint256 parent_hash(42);
    CBlock block = MakeTestBlock(parent_hash, 2);
    uint256 hash = block.GetHash();

    mgr.Add(hash, block, 1000);

    BOOST_CHECK(mgr.HasChildrenOf(parent_hash));
    BOOST_CHECK(!mgr.HasChildrenOf(hash));

    mgr.Clear();
}

// ---------------------------------------------------------------------------
// BFS orphan walker
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(process_queue_linear_chain)
{
    OrphanBlockManager mgr;

    // Build a chain: A -> B -> C -> D
    // A is the "accepted" block (not in the orphan map). B, C, D are orphans.
    CBlock block_a = MakeTestBlock(uint256(), 10);
    uint256 hash_a = block_a.GetHash();

    CBlock block_b = MakeTestBlock(hash_a, 11);
    uint256 hash_b = block_b.GetHash();

    CBlock block_c = MakeTestBlock(hash_b, 12);
    uint256 hash_c = block_c.GetHash();

    CBlock block_d = MakeTestBlock(hash_c, 13);
    uint256 hash_d = block_d.GetHash();

    mgr.Add(hash_b, block_b, 1000);
    mgr.Add(hash_c, block_c, 1000);
    mgr.Add(hash_d, block_d, 1000);

    BOOST_CHECK_EQUAL(mgr.Size(), 3u);

    std::vector<uint256> accepted_order;
    size_t count = mgr.ProcessQueue(hash_a, [&](CBlock& blk) -> bool {
        accepted_order.push_back(blk.GetHash());
        return true;
    });

    BOOST_CHECK_EQUAL(count, 3u);
    BOOST_CHECK_EQUAL(mgr.Size(), 0u);

    // BFS order: B first, then C, then D.
    BOOST_REQUIRE_EQUAL(accepted_order.size(), 3u);
    BOOST_CHECK(accepted_order[0] == hash_b);
    BOOST_CHECK(accepted_order[1] == hash_c);
    BOOST_CHECK(accepted_order[2] == hash_d);

    mgr.Clear();
}

BOOST_AUTO_TEST_CASE(process_queue_branching)
{
    OrphanBlockManager mgr;

    // A -> B -> C
    // A -> B -> D  (fork at B)
    CBlock block_a = MakeTestBlock(uint256(), 20);
    uint256 hash_a = block_a.GetHash();

    CBlock block_b = MakeTestBlock(hash_a, 21);
    uint256 hash_b = block_b.GetHash();

    CBlock block_c = MakeTestBlock(hash_b, 22);
    uint256 hash_c = block_c.GetHash();

    CBlock block_d = MakeTestBlock(hash_b, 23);
    uint256 hash_d = block_d.GetHash();

    mgr.Add(hash_b, block_b, 1000);
    mgr.Add(hash_c, block_c, 1000);
    mgr.Add(hash_d, block_d, 1000);

    std::vector<uint256> accepted;
    size_t count = mgr.ProcessQueue(hash_a, [&](CBlock& blk) -> bool {
        accepted.push_back(blk.GetHash());
        return true;
    });

    BOOST_CHECK_EQUAL(count, 3u);
    BOOST_CHECK_EQUAL(mgr.Size(), 0u);

    // B must be first. C and D can be in either order.
    BOOST_REQUIRE_EQUAL(accepted.size(), 3u);
    BOOST_CHECK(accepted[0] == hash_b);
    BOOST_CHECK((accepted[1] == hash_c && accepted[2] == hash_d) ||
                (accepted[1] == hash_d && accepted[2] == hash_c));

    mgr.Clear();
}

BOOST_AUTO_TEST_CASE(process_queue_rejection_stops_subtree)
{
    OrphanBlockManager mgr;

    // A -> B -> C -> D.  Reject C — D should still be removed but not accepted.
    CBlock block_a = MakeTestBlock(uint256(), 30);
    uint256 hash_a = block_a.GetHash();

    CBlock block_b = MakeTestBlock(hash_a, 31);
    uint256 hash_b = block_b.GetHash();

    CBlock block_c = MakeTestBlock(hash_b, 32);
    uint256 hash_c = block_c.GetHash();

    CBlock block_d = MakeTestBlock(hash_c, 33);
    uint256 hash_d = block_d.GetHash();

    mgr.Add(hash_b, block_b, 1000);
    mgr.Add(hash_c, block_c, 1000);
    mgr.Add(hash_d, block_d, 1000);

    std::vector<uint256> accepted;
    size_t count = mgr.ProcessQueue(hash_a, [&](CBlock& blk) -> bool {
        uint256 h = blk.GetHash();
        if (h == hash_c) return false;  // reject C
        accepted.push_back(h);
        return true;
    });

    // B accepted, C rejected (not in accepted list), D never reached by walker
    // because C's hash was not added to the work queue.
    BOOST_CHECK_EQUAL(count, 1u);
    BOOST_REQUIRE_EQUAL(accepted.size(), 1u);
    BOOST_CHECK(accepted[0] == hash_b);

    // B and C removed from maps. D remains as an orphan since its parent (C)
    // was never accepted — its hash was never queued for processing.
    BOOST_CHECK_EQUAL(mgr.Size(), 1u);
    BOOST_CHECK(mgr.Contains(hash_d));

    mgr.Clear();
}

BOOST_AUTO_TEST_CASE(process_queue_no_orphans)
{
    OrphanBlockManager mgr;

    // Processing a hash with no dependents is a no-op.
    size_t count = mgr.ProcessQueue(uint256(99), [](CBlock&) { return true; });
    BOOST_CHECK_EQUAL(count, 0u);
}

// ---------------------------------------------------------------------------
// Root block lookup
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(get_root_block_chain)
{
    OrphanBlockManager mgr;

    // Chain: (missing parent) -> A -> B -> C
    uint256 missing(100);

    CBlock block_a = MakeTestBlock(missing, 40);
    uint256 hash_a = block_a.GetHash();

    CBlock block_b = MakeTestBlock(hash_a, 41);
    uint256 hash_b = block_b.GetHash();

    CBlock block_c = MakeTestBlock(hash_b, 42);
    uint256 hash_c = block_c.GetHash();

    mgr.Add(hash_a, block_a, 1000);
    mgr.Add(hash_b, block_b, 1000);
    mgr.Add(hash_c, block_c, 1000);

    // Root of C's chain is A (the earliest orphan).
    const CBlock* root = mgr.GetRootBlock(hash_c);
    BOOST_REQUIRE(root != nullptr);
    BOOST_CHECK(root->GetHash() == hash_a);

    // Root of A is itself.
    root = mgr.GetRootBlock(hash_a);
    BOOST_REQUIRE(root != nullptr);
    BOOST_CHECK(root->GetHash() == hash_a);

    // Unknown hash returns nullptr.
    BOOST_CHECK(mgr.GetRootBlock(uint256(99)) == nullptr);

    mgr.Clear();
}

// ---------------------------------------------------------------------------
// Size cap and eviction
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(size_cap_enforced)
{
    OrphanBlockManager mgr;

    // Fill to maximum.
    for (size_t i = 0; i < OrphanBlockManager::MAX_ORPHAN_BLOCKS; ++i) {
        CBlock block = MakeTestBlock(uint256(static_cast<uint8_t>(i % 256)), static_cast<uint32_t>(i + 50));
        mgr.Add(block.GetHash(), block, 1000);
    }
    BOOST_CHECK_EQUAL(mgr.Size(), OrphanBlockManager::MAX_ORPHAN_BLOCKS);

    // Adding one more should evict one, keeping size at max.
    CBlock extra = MakeTestBlock(uint256(255), 9999);
    BOOST_CHECK(mgr.Add(extra.GetHash(), extra, 1000));
    BOOST_CHECK_EQUAL(mgr.Size(), OrphanBlockManager::MAX_ORPHAN_BLOCKS);
    BOOST_CHECK(mgr.Contains(extra.GetHash()));

    mgr.Clear();
}

// ---------------------------------------------------------------------------
// Time-based expiry
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(expire_old_orphans)
{
    OrphanBlockManager mgr;

    int64_t t0 = 1000;

    CBlock old_block = MakeTestBlock(uint256(1), 60);
    uint256 old_hash = old_block.GetHash();
    mgr.Add(old_hash, old_block, t0);

    CBlock new_block = MakeTestBlock(uint256(2), 61);
    uint256 new_hash = new_block.GetHash();
    mgr.Add(new_hash, new_block, t0 + OrphanBlockManager::MAX_ORPHAN_AGE_SECONDS);

    BOOST_CHECK_EQUAL(mgr.Size(), 2u);

    // Expire at a time that makes old_block stale but new_block still fresh.
    int64_t expire_time = t0 + OrphanBlockManager::MAX_ORPHAN_AGE_SECONDS + 1;
    size_t evicted = mgr.EraseExpired(expire_time);

    BOOST_CHECK_EQUAL(evicted, 1u);
    BOOST_CHECK(!mgr.Contains(old_hash));
    BOOST_CHECK(mgr.Contains(new_hash));

    mgr.Clear();
}

BOOST_AUTO_TEST_CASE(expire_cleans_reverse_index)
{
    OrphanBlockManager mgr;

    uint256 parent(200);
    CBlock block = MakeTestBlock(parent, 70);
    uint256 hash = block.GetHash();

    mgr.Add(hash, block, 1000);
    BOOST_CHECK(mgr.HasChildrenOf(parent));

    mgr.EraseExpired(1000 + OrphanBlockManager::MAX_ORPHAN_AGE_SECONDS + 1);

    BOOST_CHECK(!mgr.HasChildrenOf(parent));
    BOOST_CHECK_EQUAL(mgr.Size(), 0u);

    mgr.Clear();
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(clear_empties_everything)
{
    OrphanBlockManager mgr;

    for (uint32_t i = 0; i < 10; ++i) {
        CBlock block = MakeTestBlock(uint256(i), 80 + i);
        mgr.Add(block.GetHash(), block, 1000);
    }
    BOOST_CHECK_EQUAL(mgr.Size(), 10u);

    mgr.Clear();
    BOOST_CHECK_EQUAL(mgr.Size(), 0u);
}

BOOST_AUTO_TEST_SUITE_END()

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
