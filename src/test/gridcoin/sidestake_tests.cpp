// Copyright (c) 2024 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <util.h>
#include <gridcoin/sidestake.h>
#include <interfaces/sidestake.h>
#include <key_io.h>

BOOST_AUTO_TEST_SUITE(sidestake_tests)

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_trivial)
{
    GRC::Allocation allocation;

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), true);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), false);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_below_minimum)
{
    GRC::Allocation allocation((double) 0.0000499999);

    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), true);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), false);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_minimum)
{
    GRC::Allocation allocation((double) 0.0001);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double)
{
    GRC::Allocation allocation((double) 0.0005);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 2000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_one_percent)
{
    GRC::Allocation allocation((double) 0.01);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 100);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_just_below_unity)
{
    GRC::Allocation allocation((double) 0.9999);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 9999);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_maximum_before_multiplication)
{
    GRC::Allocation allocation((double) 1.0);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 1);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_fraction)
{
    GRC::Allocation allocation(Fraction(2500, 10000));

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 2500);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), false);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);

    allocation.Simplify();

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 4);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_ToPercent)
{
    GRC::Allocation allocation((double) 0.0005);

    BOOST_CHECK(std::abs(allocation.ToPercent() - (double) 0.05) < 1e-08);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_multiplication_and_derivation_of_allocation)
{
    // Multiplication is a very common operation with Allocations, because
    // the general pattern is to multiply the allocation times a CAmount rewards
    // to determine the rewards in Halfords (CAmount) to put on the output.

    // Allocations that are initialized from doubles are rounded to the nearest 1/10000. This is the worst case
    // therefore, in terms of numerator and denominator.
    GRC::Allocation allocation(0.9999);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 9999);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);

    CAmount max_accrual = 16384 * COIN;

    CAmount actual_output = (allocation * max_accrual).ToCAmount();
    BOOST_CHECK_EQUAL(actual_output, int64_t {1638236160000});
}

// Single-field edits must leave the other fields exactly as STORED, not as the
// caller last observed them.
//
// The bug these guard against is silent: setAllocation/setDescription used to read
// the whole entry, rebuild it, and write it back through NonContractAdd(). The read
// and the write were separate acquisitions of the registry lock, so a
// LoadLocalSideStakesFromConfig() in between (RwSettingsUpdated, e.g. from the
// changesettings RPC) was reverted by the fields the caller never meant to touch --
// editing a description would quietly restore the old allocation. Nothing failed;
// the value just reappeared.
//
// These tests do not need to reproduce the race to lock the fix in. They assert the
// property that makes the race impossible: the surviving fields come from the entry
// as stored at write time.
//
// SCOPE, stated honestly: these exercise the REGISTRY methods directly, so on their
// own they would still pass if the interface-layer editors regressed to reading the
// entry and writing it back through NonContractAdd(). The
// sidestake_editors_do_not_revert_the_field_not_being_edited case below covers that
// half, by driving the real editors.
BOOST_AUTO_TEST_CASE(sidestake_NonContractSetAllocation_preserves_other_fields)
{
    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();
    // Built directly rather than decoded from a literal address (same idiom as
    // block_rewards_tests.cpp): these tests are about registry bookkeeping, not
    // address encoding, and a literal would bind them to whichever chain params
    // the test fixture happens to select.
    uint160 hash;
    *(hash.begin()) = 0x5a;
    const CTxDestination dest = CKeyID(hash);

    registry.NonContractAdd(GRC::LocalSideStake(dest,
                                                GRC::Allocation(0.10),
                                                "original description",
                                                GRC::LocalSideStake::LocalSideStakeStatus::ACTIVE),
                            /*save_to_file=*/false);

    BOOST_REQUIRE(registry.NonContractSetAllocation(dest, GRC::Allocation(0.25), /*save_to_file=*/false));

    const auto entries = registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL);
    BOOST_REQUIRE_EQUAL(entries.size(), 1u);
    BOOST_CHECK_EQUAL(entries.front()->GetAllocation().ToPercent(), 25.0);
    BOOST_CHECK_EQUAL(entries.front()->GetDescription(), "original description");

    registry.NonContractDelete(dest, /*save_to_file=*/false);
}

BOOST_AUTO_TEST_CASE(sidestake_NonContractSetDescription_preserves_other_fields)
{
    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();
    // Built directly rather than decoded from a literal address (same idiom as
    // block_rewards_tests.cpp): these tests are about registry bookkeeping, not
    // address encoding, and a literal would bind them to whichever chain params
    // the test fixture happens to select.
    uint160 hash;
    *(hash.begin()) = 0x5a;
    const CTxDestination dest = CKeyID(hash);

    registry.NonContractAdd(GRC::LocalSideStake(dest,
                                                GRC::Allocation(0.35),
                                                "before",
                                                GRC::LocalSideStake::LocalSideStakeStatus::ACTIVE),
                            /*save_to_file=*/false);

    BOOST_REQUIRE(registry.NonContractSetDescription(dest, "after", /*save_to_file=*/false));

    const auto entries = registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL);
    BOOST_REQUIRE_EQUAL(entries.size(), 1u);
    BOOST_CHECK_EQUAL(entries.front()->GetDescription(), "after");
    // The allocation must survive a description edit untouched.
    BOOST_CHECK_EQUAL(entries.front()->GetAllocation().ToPercent(), 35.0);

    registry.NonContractDelete(dest, /*save_to_file=*/false);
}

// Both setters report "no such entry" rather than creating one, so a caller whose
// lookup raced a delete gets INVALID_ADDRESS instead of silently resurrecting it.
BOOST_AUTO_TEST_CASE(sidestake_NonContractSet_returns_false_for_missing_entry)
{
    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();
    // Built directly rather than decoded from a literal address (same idiom as
    // block_rewards_tests.cpp): these tests are about registry bookkeeping, not
    // address encoding, and a literal would bind them to whichever chain params
    // the test fixture happens to select.
    uint160 hash;
    *(hash.begin()) = 0x5a;
    const CTxDestination dest = CKeyID(hash);

    registry.NonContractDelete(dest, /*save_to_file=*/false); // ensure absent

    BOOST_CHECK(!registry.NonContractSetAllocation(dest, GRC::Allocation(0.5), /*save_to_file=*/false));
    BOOST_CHECK(!registry.NonContractSetDescription(dest, "x", /*save_to_file=*/false));
    BOOST_CHECK(registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL).empty());
}

// Editor-level behaviour: a single-field edit through the interface must leave the
// other field intact.
//
// WHAT THIS DOES NOT COVER, verified by experiment rather than assumed: it does NOT
// fail if setAllocation/setDescription regress to reading the entry and writing it
// back through NonContractAdd(). Those two implementations are observationally
// identical in a single thread -- the stale read only loses data when something
// mutates the entry BETWEEN the read and the write, and there is no injection point
// to force that interleaving here. Reverting the fix and re-running this case leaves
// it green.
//
// It still earns its place: it pins the end-state contract of the editors (a
// description edit does not disturb the allocation, and vice versa), which catches a
// regression that writes a wrong or default value for the untouched field. The
// read-under-lock property that actually closes the race is pinned by the
// registry-level cases above.
BOOST_AUTO_TEST_CASE(sidestake_editors_do_not_revert_the_field_not_being_edited)
{
    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

    uint160 hash;
    *(hash.begin()) = 0x5b;
    const CTxDestination dest = CKeyID(hash);
    const std::string address = EncodeDestination(dest);

    registry.NonContractDelete(dest, /*save_to_file=*/false); // start clean

    // The editors persist through SaveLocalSideStakesToConfig(), which needs a real
    // data directory. TestingSetup points -datadir at a temp path but never creates
    // it, so GetDataDir() is empty and the settings write asserts. Create it here
    // rather than reaching into the shared fixture.
    const fs::path datadir = gArgs.GetArg("-datadir", "");
    BOOST_REQUIRE(!datadir.empty());
    fs::create_directories(datadir);
    gArgs.ClearPathCache();

    std::unique_ptr<interfaces::SideStakeManager> manager = interfaces::MakeSideStakeManager();
    BOOST_REQUIRE(manager != nullptr);

    BOOST_REQUIRE(manager->addLocal(address, 10.0, "first description").status
                  == interfaces::SideStakeEditStatus::OK);

    // Editing the description must not disturb the allocation.
    BOOST_REQUIRE(manager->setDescription(address, "second description").status
                  == interfaces::SideStakeEditStatus::OK);
    {
        const auto entries = registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL);
        BOOST_REQUIRE_EQUAL(entries.size(), 1u);
        BOOST_CHECK_EQUAL(entries.front()->GetDescription(), "second description");
        BOOST_CHECK_CLOSE(entries.front()->GetAllocation().ToPercent(), 10.0, 1e-6);
    }

    // Editing the allocation must not disturb the description.
    BOOST_REQUIRE(manager->setAllocation(address, 20.0).status
                  == interfaces::SideStakeEditStatus::OK);
    {
        const auto entries = registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL);
        BOOST_REQUIRE_EQUAL(entries.size(), 1u);
        BOOST_CHECK_CLOSE(entries.front()->GetAllocation().ToPercent(), 20.0, 1e-6);
        BOOST_CHECK_EQUAL(entries.front()->GetDescription(), "second description");
    }

    registry.NonContractDelete(dest, /*save_to_file=*/false);
}

BOOST_AUTO_TEST_SUITE_END()
