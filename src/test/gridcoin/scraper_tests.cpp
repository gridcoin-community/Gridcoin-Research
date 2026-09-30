// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/scraper/scraper.h"
#include "test/gridcoin/rsa_test_vectors.h"
#include <util/strencodings.h>
#include <util/string.h>

#include <boost/test/unit_test.hpp>

#include <map>
#include <string>
#include <vector>

namespace {

const std::string PROJECT_A = "https://project-a.example.org/";
const std::string PROJECT_B = "https://project-b.example.org/";
const std::string CPID = "00010203040506070809101112131415";

// The account ID and beacon key that rsa_test_vectors::g_test_message is made of.
const uint32_t ACCOUNT_ID = 19100117;
const std::string BEACON_KEY_HEX = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab";
const std::string OTHER_BEACON_KEY_HEX = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef01";

ScraperPendingBeaconEntry MakeCandidate(const std::string& master_url,
                                        uint32_t account_id,
                                        const std::string& beacon_key_hex,
                                        const std::vector<uint8_t>& signature)
{
    ScraperPendingBeaconEntry entry;

    entry.cpid = CPID;
    entry.timestamp = 0;
    entry.has_ownership_proof = true;
    entry.ownership_master_url = master_url;
    entry.ownership_account_id = account_id;
    entry.ownership_rsa_signature = signature;
    entry.beacon_public_key_hex = beacon_key_hex;

    return entry;
}

std::vector<const ScraperPendingBeaconMap::value_type*> Candidates(const ScraperPendingBeaconMap& pending)
{
    std::vector<const ScraperPendingBeaconMap::value_type*> candidates;

    for (const auto& entry : pending) {
        candidates.push_back(&entry);
    }

    return candidates;
}

std::vector<uint8_t> ValidSignature()
{
    return ParseHex(rsa_test_vectors::g_test_signature_hex);
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(scraper_tests)

BOOST_AUTO_TEST_CASE(test_vector_is_an_ownership_proof_message)
{
    // The cases below depend on the shared signature covering exactly this account ID and beacon key.
    BOOST_CHECK_EQUAL(ToString(ACCOUNT_ID) + " " + BEACON_KEY_HEX, rsa_test_vectors::g_test_message);
}

BOOST_AUTO_TEST_CASE(it_verifies_a_proof_for_the_project_being_processed)
{
    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    const auto* verified = FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID, keys,
                                                            signature_failures);

    BOOST_REQUIRE(verified != nullptr);
    BOOST_CHECK_EQUAL(verified->first, "code_a");
    BOOST_CHECK_EQUAL(signature_failures, 0U);
}

BOOST_AUTO_TEST_CASE(it_matches_the_proof_url_to_the_project_being_processed)
{
    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };

    // The stats being processed are project B's.
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_B, ACCOUNT_ID, keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 0U);

    // The proof names project A, whichever keys are known.
    const std::map<std::string, std::string> both_keys {
        {PROJECT_A, rsa_test_vectors::g_test_pem_pubkey},
        {PROJECT_B, rsa_test_vectors::g_test_pem_pubkey},
    };

    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_B, ACCOUNT_ID, both_keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 0U);
}

BOOST_AUTO_TEST_CASE(it_returns_the_candidate_whose_signature_verifies)
{
    // Two candidates for the same CPID and account ID; the test signature covers code_2's beacon key.
    const ScraperPendingBeaconMap pending {
        {"code_1", MakeCandidate(PROJECT_A, ACCOUNT_ID, OTHER_BEACON_KEY_HEX, ValidSignature())},
        {"code_2", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    const auto* verified = FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID, keys,
                                                            signature_failures);

    BOOST_REQUIRE(verified != nullptr);
    BOOST_CHECK_EQUAL(verified->first, "code_2");
    BOOST_CHECK_EQUAL(verified->second.beacon_public_key_hex, BEACON_KEY_HEX);
    BOOST_CHECK_EQUAL(signature_failures, 1U);
}

BOOST_AUTO_TEST_CASE(it_matches_the_account_id_to_the_user_record)
{
    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID + 1, keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 0U);
}

BOOST_AUTO_TEST_CASE(it_counts_signature_failures)
{
    std::vector<uint8_t> tampered = ValidSignature();
    tampered[0] ^= 0xFF;

    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, tampered)},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID, keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 1U);
}

BOOST_AUTO_TEST_CASE(it_removes_duplicate_project_keys)
{
    const std::string PROJECT_C = "https://project-c.example.org/";

    // Projects A and B serve the same key, in different PEM text; project C serves its own.
    std::map<std::string, std::string> keys {
        {PROJECT_A, rsa_test_vectors::g_test_pem_pubkey},
        {PROJECT_B, rsa_test_vectors::g_test_pem_pubkey_rewrapped},
        {PROJECT_C, rsa_test_vectors::g_test_pem_pubkey_2},
    };

    const std::vector<std::string> removed = RemoveDuplicateProjectPublicKeys(keys);

    BOOST_CHECK(removed == std::vector<std::string>({PROJECT_A, PROJECT_B}));
    BOOST_CHECK_EQUAL(keys.size(), 1U);
    BOOST_CHECK(keys.count(PROJECT_C) == 1);
}

BOOST_AUTO_TEST_CASE(it_keeps_distinct_project_keys)
{
    std::map<std::string, std::string> keys {
        {PROJECT_A, rsa_test_vectors::g_test_pem_pubkey},
        {PROJECT_B, rsa_test_vectors::g_test_pem_pubkey_2},
    };

    BOOST_CHECK(RemoveDuplicateProjectPublicKeys(keys).empty());
    BOOST_CHECK_EQUAL(keys.size(), 2U);
}

BOOST_AUTO_TEST_SUITE_END()
