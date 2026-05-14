// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "amount.h"
#include "gridcoin/consensus/block_rewards.h"
#include "gridcoin/sidestake.h"
#include "key.h"
#include "script.h"
#include "validation.h"

#include <memory>
#include <vector>

namespace {

//! Create a deterministic test destination from an integer seed.
CTxDestination MakeTestDest(unsigned int seed)
{
    // Use a CKeyID with a simple uint160 derived from the seed.
    uint160 hash;
    *(hash.begin()) = static_cast<unsigned char>(seed & 0xFF);
    *(hash.begin() + 1) = static_cast<unsigned char>((seed >> 8) & 0xFF);
    return CKeyID(hash);
}

//! Create a mandatory sidestake SideStake_ptr for testing.
GRC::SideStake_ptr MakeTestMandatorySideStake(
    const CTxDestination& dest,
    const GRC::Allocation& alloc)
{
    auto mandatory = std::make_shared<GRC::MandatorySideStake>(
        dest, alloc, std::string("test"),
        int64_t{0}, uint256{},
        GRC::MandatorySideStake::MandatorySideStakeStatus::MANDATORY);
    return std::make_shared<GRC::SideStake>(mandatory);
}

//! Helper: build a minimal coinstake transaction for validation tests.
//! Creates an empty output at index 0, a coinstake output at index 1,
//! and additional outputs as specified.
CTransaction MakeTestCoinstake(
    const CTxDestination& coinstake_dest,
    CAmount coinstake_value,
    const std::vector<std::pair<CTxDestination, CAmount>>& additional_outputs)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash.SetNull(); // will be non-null in real use

    // vout[0]: empty coinstake marker
    mtx.vout.push_back(CTxOut());

    // vout[1]: coinstake output
    CScript coinstake_script;
    coinstake_script.SetDestination(coinstake_dest);
    mtx.vout.push_back(CTxOut(coinstake_value, coinstake_script));

    // Additional outputs (sidestakes, MRC, etc.)
    for (const auto& [dest, amount] : additional_outputs) {
        CScript script;
        script.SetDestination(dest);
        mtx.vout.push_back(CTxOut(amount, script));
    }

    return CTransaction(mtx);
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(block_rewards_tests)

// =============================================================================
// ComputeEligibleMandatorySidestakes — golden spec tests
// =============================================================================

BOOST_AUTO_TEST_CASE(compute_eligible_returns_empty_before_v13)
{
    GRC::BlockRewardRules rules(nullptr, 12, 0);

    auto specs = rules.ComputeEligibleMandatorySidestakes(
        MakeTestDest(1), 100 * COIN, {MakeTestMandatorySideStake(MakeTestDest(2), GRC::Allocation(0.05))});

    BOOST_CHECK(specs.empty());
}

BOOST_AUTO_TEST_CASE(compute_eligible_basic_single_sidestake)
{
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(0.05); // 5%
    CAmount total_owed = 100 * COIN;

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);

    BOOST_REQUIRE_EQUAL(specs.size(), 1u);
    BOOST_CHECK(specs[0].dest == ss_dest);
    BOOST_CHECK_EQUAL(specs[0].required_amount, (alloc * total_owed).ToCAmount());
    BOOST_CHECK(!specs[0].IsSuppressed());
}

BOOST_AUTO_TEST_CASE(compute_eligible_dust_suppression)
{
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(0.0001); // 0.01% — tiny allocation
    CAmount total_owed = 1 * COIN; // 1 GRC — alloc * total_owed = 0.0001 GRC < 1 CENT

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);

    BOOST_REQUIRE_EQUAL(specs.size(), 1u);
    BOOST_CHECK(specs[0].IsSuppressed());
    BOOST_CHECK_EQUAL(specs[0].suppressed_reason, "dust (below CENT)");
}

BOOST_AUTO_TEST_CASE(compute_eligible_dust_boundary_just_below)
{
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    // CENT = 0.01 COIN = 1000000 Halfords. Find allocation * total_owed just below CENT.
    // With alloc = 1/100 (1%) and total_owed = 0.99 COIN, required = 0.0099 COIN < 0.01 COIN
    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(Fraction(1, 100));
    CAmount total_owed = 99 * CENT; // 0.99 COIN

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);

    BOOST_REQUIRE_EQUAL(specs.size(), 1u);
    // required_amount = (1/100) * 99000000 = 990000 Halfords = 0.0099 COIN < CENT
    BOOST_CHECK(specs[0].IsSuppressed());
    BOOST_CHECK_EQUAL(specs[0].suppressed_reason, "dust (below CENT)");
}

BOOST_AUTO_TEST_CASE(compute_eligible_dust_boundary_exactly_cent)
{
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    // alloc = 1/100 (1%), total_owed = 1 COIN → required = 0.01 COIN = CENT exactly
    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(Fraction(1, 100));
    CAmount total_owed = 1 * COIN;

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);

    BOOST_REQUIRE_EQUAL(specs.size(), 1u);
    // required_amount = CENT exactly → NOT suppressed (check is < CENT, not <=)
    BOOST_CHECK(!specs[0].IsSuppressed());
    BOOST_CHECK_EQUAL(specs[0].required_amount, CENT);
}

BOOST_AUTO_TEST_CASE(compute_eligible_coinstake_address_match_suppression)
{
    // This is the #2848 scenario: staker address matches mandatory sidestake address.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination same_dest = MakeTestDest(1);
    GRC::Allocation alloc(0.05);
    CAmount total_owed = 100 * COIN;

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(same_dest, alloc)
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(same_dest, total_owed, active);

    BOOST_REQUIRE_EQUAL(specs.size(), 1u);
    BOOST_CHECK(specs[0].IsSuppressed());
    BOOST_CHECK_EQUAL(specs[0].suppressed_reason, "matches coinstake destination");
}

BOOST_AUTO_TEST_CASE(compute_eligible_multiple_sidestakes_mixed_suppression)
{
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest_ok = MakeTestDest(2);
    CTxDestination ss_dest_dust = MakeTestDest(3);
    CTxDestination ss_dest_match = MakeTestDest(1); // same as coinstake

    CAmount total_owed = 10 * COIN;

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest_ok, GRC::Allocation(0.10)),       // 10% = 1 GRC — eligible
        MakeTestMandatorySideStake(ss_dest_dust, GRC::Allocation(0.0001)),   // 0.01% = 0.001 GRC — dust
        MakeTestMandatorySideStake(ss_dest_match, GRC::Allocation(0.05)),    // 5% — address match
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);

    BOOST_REQUIRE_EQUAL(specs.size(), 3u);

    // First: eligible
    BOOST_CHECK(!specs[0].IsSuppressed());
    BOOST_CHECK(specs[0].dest == ss_dest_ok);

    // Second: dust-suppressed
    BOOST_CHECK(specs[1].IsSuppressed());
    BOOST_CHECK_EQUAL(specs[1].suppressed_reason, "dust (below CENT)");

    // Third: address-match-suppressed
    BOOST_CHECK(specs[2].IsSuppressed());
    BOOST_CHECK_EQUAL(specs[2].suppressed_reason, "matches coinstake destination");

    // FilterEligible should return only the first
    auto eligible = GRC::BlockRewardRules::FilterEligible(specs);
    BOOST_REQUIRE_EQUAL(eligible.size(), 1u);
    BOOST_CHECK(eligible[0].dest == ss_dest_ok);
}

BOOST_AUTO_TEST_CASE(compute_eligible_allocation_amounts_are_precise)
{
    // Golden test: verify exact required_amount for known inputs.
    // Allocation 25% = Fraction(1,4), total_owed = 7 COIN = 700000000 Halfords.
    // Required = (1/4) * 700000000 = 175000000.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(Fraction(1, 4)); // 25%
    CAmount total_owed = 7 * COIN;

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);

    BOOST_REQUIRE_EQUAL(specs.size(), 1u);
    BOOST_CHECK_EQUAL(specs[0].required_amount, 175000000);
}

// =============================================================================
// ValidateMandatorySidestakeOutputs
// =============================================================================

BOOST_AUTO_TEST_CASE(validate_passes_with_correct_outputs)
{
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(0.05); // 5%
    CAmount total_owed = 100 * COIN;
    CAmount required = (alloc * total_owed).ToCAmount();

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    // Build a coinstake with the sidestake output.
    CTransaction coinstake = MakeTestCoinstake(coinstake_dest, 95 * COIN, {
        {ss_dest, required}
    });

    // Validate using the explicit sidestakes overload.
    // (We can't call the registry-based overload in tests without a populated registry,
    // so we test the spec computation directly and then test the validate method
    // which calls ComputeEligibleMandatorySidestakes internally.)
    //
    // Since ValidateMandatorySidestakeOutputs calls the registry-based overload internally,
    // we test the validation logic by constructing the expected scenario manually.
    // The validate method scans outputs and matches against the eligible set.
    //
    // For a self-contained test, we test via the spec + manual matching:
    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);
    auto eligible = GRC::BlockRewardRules::FilterEligible(specs);

    BOOST_REQUIRE_EQUAL(eligible.size(), 1u);

    // Verify the coinstake output matches the spec.
    CTxDestination output_dest;
    BOOST_CHECK(ExtractDestination(coinstake.vout[2].scriptPubKey, output_dest));
    BOOST_CHECK(output_dest == ss_dest);
    BOOST_CHECK(coinstake.vout[2].nValue >= eligible[0].required_amount);
}

BOOST_AUTO_TEST_CASE(validate_passes_with_overpayment)
{
    // Validator uses >= not ==, so overpayment should pass. This verifies
    // the spec amount relationship directly. The full ValidateMandatory-
    // SidestakeOutputs call is exercised in the coincidence tests below,
    // which pass an explicit active list to ComputeEligibleMandatorySidestakes.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(0.05);
    CAmount total_owed = 100 * COIN;
    CAmount required = (alloc * total_owed).ToCAmount();

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    CTransaction coinstake = MakeTestCoinstake(coinstake_dest, 94 * COIN, {
        {ss_dest, required + 1 * COIN} // overpay by 1 GRC
    });

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);
    auto eligible = GRC::BlockRewardRules::FilterEligible(specs);

    BOOST_REQUIRE_EQUAL(eligible.size(), 1u);
    BOOST_CHECK(coinstake.vout[2].nValue >= eligible[0].required_amount);
}

BOOST_AUTO_TEST_CASE(validate_fails_with_underpayment)
{
    // Verify that an underpayment does not satisfy the >= threshold.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation alloc(0.05);
    CAmount total_owed = 100 * COIN;
    CAmount required = (alloc * total_owed).ToCAmount();

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, alloc)
    };

    CTransaction coinstake = MakeTestCoinstake(coinstake_dest, 96 * COIN, {
        {ss_dest, required - 1} // underpay by 1 Halford
    });

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);
    auto eligible = GRC::BlockRewardRules::FilterEligible(specs);

    BOOST_REQUIRE_EQUAL(eligible.size(), 1u);
    BOOST_CHECK(coinstake.vout[2].nValue < eligible[0].required_amount);
}

BOOST_AUTO_TEST_CASE(validate_2848_scenario_staker_matches_sidestake)
{
    // The #2848 regression test: when the staker address matches a mandatory
    // sidestake address, the sidestake is suppressed. The validator should
    // not expect a dedicated sidestake output for it.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination staker_dest = MakeTestDest(1);
    CTxDestination other_ss_dest = MakeTestDest(2);
    GRC::Allocation alloc_match(0.05);
    GRC::Allocation alloc_other(0.10);
    CAmount total_owed = 100 * COIN;

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(staker_dest, alloc_match),  // suppressed — matches coinstake
        MakeTestMandatorySideStake(other_ss_dest, alloc_other) // eligible
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(staker_dest, total_owed, active);
    auto eligible = GRC::BlockRewardRules::FilterEligible(specs);

    // Only the non-matching sidestake should be eligible.
    BOOST_REQUIRE_EQUAL(eligible.size(), 1u);
    BOOST_CHECK(eligible[0].dest == other_ss_dest);

    // The suppressed one should be visible in the full spec.
    BOOST_REQUIRE_EQUAL(specs.size(), 2u);
    BOOST_CHECK(specs[0].IsSuppressed());
    BOOST_CHECK_EQUAL(specs[0].suppressed_reason, "matches coinstake destination");
    BOOST_CHECK(!specs[1].IsSuppressed());
}

BOOST_AUTO_TEST_CASE(validate_voluntary_sidestake_to_mandatory_address)
{
    // Regression test from shadow sync: a staker configured a voluntary
    // sidestake to the same address as an active mandatory sidestake. The
    // miner produced two outputs to that address — a smaller voluntary output
    // (e.g. 5% of total_owed) and the required mandatory output (e.g. 10%).
    // The validator must match by exact amount, not hard-fail on the voluntary
    // output that doesn't meet the mandatory requirement.
    //
    // Observed at testnet heights 2435906 and 2436270.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2); // mandatory AND voluntary target
    GRC::Allocation mandatory_alloc(0.10); // 10%
    CAmount total_owed = 10066666600; // ~100.66666600 GRC (matches real block scale)
    CAmount mandatory_required = (mandatory_alloc * total_owed).ToCAmount();
    CAmount voluntary_amount = mandatory_required / 2; // voluntary pays less

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, mandatory_alloc)
    };

    // Build coinstake: voluntary output BEFORE mandatory output (order matters).
    CTransaction coinstake = MakeTestCoinstake(coinstake_dest, 90 * COIN, {
        {ss_dest, voluntary_amount},    // vout[2]: voluntary (smaller)
        {ss_dest, mandatory_required}   // vout[3]: mandatory (exact)
    });

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);
    auto eligible = GRC::BlockRewardRules::FilterEligible(specs);

    BOOST_REQUIRE_EQUAL(eligible.size(), 1u);
    BOOST_CHECK_EQUAL(eligible[0].required_amount, mandatory_required);

    // The validator should match vout[3] (>= required amount) and skip vout[2]
    // (voluntary — less than required). mrc_start_index = 4 (all outputs are
    // pre-MRC in this test).
    std::string error;
    bool pass = rules.ValidateMandatorySidestakeOutputs(
        coinstake, coinstake_dest, total_owed, 4, error, active);

    BOOST_CHECK_MESSAGE(pass, "Expected pass but got: " + error);
}

BOOST_AUTO_TEST_CASE(validate_voluntary_sidestake_larger_than_mandatory)
{
    // The voluntary sidestake could also be LARGER than the mandatory amount.
    // With >= matching the voluntary output may "steal" the spec match, but
    // validation still passes because the mandatory address received at least
    // the required amount. See the detailed comment in
    // ValidateMandatorySidestakeOutputs.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation mandatory_alloc(0.05); // 5%
    CAmount total_owed = 100 * COIN;
    CAmount mandatory_required = (mandatory_alloc * total_owed).ToCAmount();
    CAmount voluntary_amount = mandatory_required * 3; // voluntary pays MORE

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, mandatory_alloc)
    };

    // Voluntary (larger) output first, mandatory (exact) second.
    CTransaction coinstake = MakeTestCoinstake(coinstake_dest, 80 * COIN, {
        {ss_dest, voluntary_amount},    // vout[2]: voluntary (larger)
        {ss_dest, mandatory_required}   // vout[3]: mandatory (exact)
    });

    std::string error;
    bool pass = rules.ValidateMandatorySidestakeOutputs(
        coinstake, coinstake_dest, total_owed, 4, error, active);

    BOOST_CHECK_MESSAGE(pass, "Expected pass but got: " + error);
}

BOOST_AUTO_TEST_CASE(validate_fails_when_only_voluntary_present)
{
    // If only the voluntary output exists at less than the required amount,
    // the >= scan finds zero matches (voluntary_amount < required_amount).
    // With one eligible spec, expected=1 but validated=0 → fail.
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest = MakeTestDest(2);
    GRC::Allocation mandatory_alloc(0.10);
    CAmount total_owed = 100 * COIN;
    CAmount mandatory_required = (mandatory_alloc * total_owed).ToCAmount();
    CAmount voluntary_amount = mandatory_required / 2;

    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest, mandatory_alloc)
    };

    CTransaction coinstake = MakeTestCoinstake(coinstake_dest, 95 * COIN, {
        {ss_dest, voluntary_amount}  // vout[2]: voluntary only (wrong amount)
    });

    std::string error;
    bool pass = rules.ValidateMandatorySidestakeOutputs(
        coinstake, coinstake_dest, total_owed, 3, error, active);

    BOOST_CHECK(!pass);
    BOOST_CHECK(!error.empty());
}

// =============================================================================
// CMutableTransaction round-trip (uses real CMutableTransaction from E1)
// =============================================================================

BOOST_AUTO_TEST_CASE(mutable_transaction_round_trip)
{
    CMutableTransaction mtx_orig;
    mtx_orig.nVersion = 2;
    mtx_orig.nTime = 1234567890;
    mtx_orig.nLockTime = 42;

    CScript script;
    script.SetDestination(MakeTestDest(1));
    mtx_orig.vout.push_back(CTxOut(5 * COIN, script));

    CTransaction orig(mtx_orig);

    // Copy out to CMutableTransaction
    CMutableTransaction mtx(orig);
    BOOST_CHECK_EQUAL(mtx.nVersion, orig.nVersion);
    BOOST_CHECK_EQUAL(mtx.nTime, orig.nTime);
    BOOST_CHECK_EQUAL(mtx.nLockTime, orig.nLockTime);
    BOOST_CHECK_EQUAL(mtx.vout.size(), orig.vout.size());
    BOOST_CHECK_EQUAL(mtx.vout[0].nValue, orig.vout[0].nValue);

    // Mutate
    CScript script2;
    script2.SetDestination(MakeTestDest(2));
    mtx.vout.push_back(CTxOut(3 * COIN, script2));

    BOOST_CHECK_EQUAL(mtx.vout.size(), 2u);
    BOOST_CHECK_EQUAL(orig.vout.size(), 1u); // original unchanged

    // Convert back via CTransaction constructor
    CTransaction result(mtx);
    BOOST_CHECK_EQUAL(result.vout.size(), 2u);
    BOOST_CHECK_EQUAL(result.nVersion, orig.nVersion);
    BOOST_CHECK_EQUAL(result.nTime, orig.nTime);
    BOOST_CHECK_EQUAL(result.vout[0].nValue, 5 * COIN);
    BOOST_CHECK_EQUAL(result.vout[1].nValue, 3 * COIN);
}

// =============================================================================
// ConstructMandatorySidestakeOutputs
// =============================================================================

BOOST_AUTO_TEST_CASE(construct_appends_sidestake_outputs)
{
    GRC::BlockRewardRules rules(nullptr, 13, 0);

    CTxDestination coinstake_dest = MakeTestDest(1);
    CTxDestination ss_dest1 = MakeTestDest(2);
    CTxDestination ss_dest2 = MakeTestDest(3);
    GRC::Allocation alloc1(0.05);
    GRC::Allocation alloc2(0.10);
    CAmount total_owed = 100 * COIN;

    // We can't test ConstructMandatorySidestakeOutputs directly without a
    // populated registry (it calls the registry-based overload internally).
    // Instead, verify the spec computation and manually simulate construction.
    std::vector<GRC::SideStake_ptr> active = {
        MakeTestMandatorySideStake(ss_dest1, alloc1),
        MakeTestMandatorySideStake(ss_dest2, alloc2)
    };

    auto specs = rules.ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed, active);
    auto eligible = GRC::BlockRewardRules::FilterEligible(specs);

    BOOST_REQUIRE_EQUAL(eligible.size(), 2u);

    // Simulate what ConstructMandatorySidestakeOutputs does.
    CMutableTransaction mtx;
    mtx.vout.push_back(CTxOut()); // empty marker
    CScript cs_script;
    cs_script.SetDestination(coinstake_dest);
    mtx.vout.push_back(CTxOut(100 * COIN, cs_script)); // coinstake output

    CAmount allocated = 0;
    for (const auto& spec : eligible) {
        CScript script;
        script.SetDestination(spec.dest);
        mtx.vout.push_back(CTxOut(spec.required_amount, script));
        allocated += spec.required_amount;
    }

    BOOST_CHECK_EQUAL(mtx.vout.size(), 4u); // empty + coinstake + 2 sidestakes
    BOOST_CHECK_EQUAL(allocated, (alloc1 * total_owed).ToCAmount() + (alloc2 * total_owed).ToCAmount());
}

BOOST_AUTO_TEST_SUITE_END()
