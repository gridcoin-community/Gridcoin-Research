// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/claim.h"

#include "key.h"
#include "main.h"
#include "streams.h"

#include <boost/test/unit_test.hpp>
#include <iostream>
#include <vector>

namespace {
//!
//! \brief Get a complete, valid claim for an investor.
//!
//! \return Investor fields initialized except the superblock-related fields.
//!
GRC::Claim GetInvestorClaim()
{
    GRC::Claim claim;

    claim.m_mining_id = GRC::MiningId::ForInvestor();
    claim.m_client_version = "v4.0.4.6-unk";
    claim.m_organization = "Example Org";
    claim.m_block_subsidy = 10.0 * COIN;

    return claim;
}

//!
//! \brief Get a complete, valid claim for a researcher.
//!
//! \return Researcher fields initialized except the superblock-related fields.
//!
GRC::Claim GetResearcherClaim()
{
    GRC::Claim claim;

    claim.m_mining_id = GRC::Cpid::Parse("00010203040506070809101112131415");
    claim.m_client_version = "v4.0.4.6-unk";
    claim.m_organization = "Example Org";
    claim.m_block_subsidy = 10.0 * COIN;
    claim.m_research_subsidy = 123.456 * COIN;
    claim.m_magnitude = 123;
    claim.m_magnitude_unit = 0.123456;
    claim.m_signature = {
        0x7b, 0x85, 0xc8, 0x3c, 0x92, 0xd9, 0x74, 0x8e,
        0xa3, 0xd2, 0x26, 0x16, 0x6f, 0x9a, 0x00, 0x6c,
        0x6f, 0x0a, 0x97, 0x97, 0xa9, 0x3a, 0x52, 0xd0,
        0xb9, 0x4f, 0xbb, 0x29, 0x61, 0xbe, 0xd5, 0xcc,
    };

    return claim;
}

//!
//! \brief Get a basic superblock to use for testing.
//!
//! \return A superblock with one CPID/magnitude pair and one project.
//!
GRC::Superblock GetTestSuperblock(uint32_t version = GRC::Superblock::CURRENT_VERSION)
{
    GRC::Superblock superblock;

    superblock.m_version = version;
    superblock.m_cpids.Add(GRC::Cpid(), GRC::Magnitude::RoundFrom(123));
    superblock.m_projects.Add("project", GRC::Superblock::ProjectStats());

    return superblock;
}

//!
//! \brief Create a valid private key for tests.
//!
//! \return This is actually the shared message private key.
//!
static CKey GetTestPrivateKey()
{
    std::vector<unsigned char> private_key = ParseHex(
        "308201130201010420fbd45ffb02ff05a3322c0d77e1e7aea264866c24e81e5ab6"
        "a8e150666b4dc6d8a081a53081a2020101302c06072a8648ce3d0101022100ffff"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604"
        "010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959"
        "f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47"
        "d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03b"
        "bfd25e8cd0364141020101a144034200044b2938fbc38071f24bede21e838a0758"
        "a52a0085f2e034e7f971df445436a252467f692ec9c5ba7e5eaa898ab99cbd9949"
        "496f7e3cafbf56304b1cc2e5bdf06e");

    CKey key;
    key.SetPrivKey(CPrivKey(private_key.begin(), private_key.end()));

    return key;
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Claim
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Claim)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_claim)
{
    const GRC::Claim claim;

    BOOST_CHECK(claim.m_version == GRC::Claim::CURRENT_VERSION);
    BOOST_CHECK(claim.m_mining_id.Valid() == false);
    BOOST_CHECK(claim.m_client_version.empty() == true);
    BOOST_CHECK(claim.m_organization.empty() == true);

    BOOST_CHECK(claim.m_block_subsidy == 0);

    BOOST_CHECK(claim.m_magnitude == 0);
    BOOST_CHECK(claim.m_research_subsidy == 0);
    BOOST_CHECK(claim.m_magnitude_unit == 0.0);

    BOOST_CHECK(claim.m_signature.empty() == true);

    BOOST_CHECK(claim.m_quorum_hash.Valid() == false);
    BOOST_CHECK(claim.m_quorum_address.empty() == true);
    BOOST_CHECK(claim.m_superblock->m_cpids.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_specified_version)
{
    const GRC::Claim claim(1);

    BOOST_CHECK(claim.m_version == 1);
    BOOST_CHECK(claim.m_mining_id.Valid() == false);
    BOOST_CHECK(claim.m_client_version.empty() == true);
    BOOST_CHECK(claim.m_organization.empty() == true);

    BOOST_CHECK(claim.m_block_subsidy == 0);

    BOOST_CHECK(claim.m_magnitude == 0);
    BOOST_CHECK(claim.m_research_subsidy == 0);
    BOOST_CHECK(claim.m_magnitude_unit == 0.0);

    BOOST_CHECK(claim.m_signature.empty() == true);

    BOOST_CHECK(claim.m_quorum_hash.Valid() == false);
    BOOST_CHECK(claim.m_quorum_address.empty() == true);
    BOOST_CHECK(claim.m_superblock->m_cpids.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_parses_a_legacy_boincblock_string_for_researcher)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const std::string quorum_address = "mk8PmpcTGLCZky8YqFHEEwXs5px3hGfQBG";

    const GRC::Superblock superblock = GetTestSuperblock(1);

    // Legacy claims only contain MD5 quorum hashes:
    const GRC::QuorumHash quorum_hash(GRC::QuorumHash::Md5Sum {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    const std::vector<unsigned char> signature {
        0x7b, 0x85, 0xc8, 0x3c, 0x92, 0xd9, 0x74, 0x8e,
        0xa3, 0xd2, 0x26, 0x16, 0x6f, 0x9a, 0x00, 0x6c,
        0x6f, 0x0a, 0x97, 0x97, 0xa9, 0x3a, 0x52, 0xd0,
        0xb9, 0x4f, 0xbb, 0x29, 0x61, 0xbe, 0xd5, 0xcc,
    };

    const std::string sig64 = EncodeBase64(signature.data(), signature.size());

    const GRC::Claim claim = GRC::Claim::Parse(
        cpid.ToString() +                // Mining ID
        "<|>"                            // Project name              (obsolete)
        "<|>"                            // AES Skein                 (obsolete)
        "<|>"                            // Recent average credit     (obsolete)
        "<|>"                            // Proof-of-BOINC difficulty (obsolete)
        "<|>"                            // Difficulty bytes          (obsolete)
        "<|>"                            // Encrypted CPID            (obsolete)
        "<|>"                            // For encrypted CPIDs?      (obsolete)
        "<|>"                            // Nonce                     (obsolete)
        "<|>"                            // Network RAC               (obsolete)
        "<|>v4.0.4.6-unk"                // Client version
        "<|>47.24638888"                 // Research subsidy
        "<|>"                            // Last payment time         (obsolete)
        "<|>"                            // RSA weight                (obsolete)
        "<|>"                            // CPID "v2"                 (obsolete)
        "<|>123"                         // Magnitude
        "<|>" + quorum_address +         // Quorum address
        "<|>"                            // Last block hash           (obsolete)
        "<|>10.00000000"                 // Block subsidy
        "<|>Example Org"                 // Organization
        "<|>"                            // Organization key          (obsolete)
        "<|>" + quorum_hash.ToString() + // Neural hash
        "<|>" + superblock.PackLegacy() +// Superblock
        "<|>"                            // Research subsidy 2        (obsolete)
        "<|>"                            // Research age              (obsolete)
        "<|>0.123456"                    // Magnitude unit
        "<|>"                            // Average magnitude         (obsolete)
        "<|>"                            // Last PoR block hash       (obsolete)
        "<|>" + quorum_hash.ToString() + // Current Neural hash       (obsolete)
        "<|>"                            // Public key                (obsolete)
        "<|>" + sig64,                   // Beacon signature
        8 // block version
    );

    // Legacy string claims (BoincBlocks) always parse to version 1:
    BOOST_CHECK(claim.m_version == 1);
    BOOST_CHECK(claim.WellFormed() == true);

    BOOST_CHECK(claim.m_mining_id == cpid);
    BOOST_CHECK(claim.m_client_version == "v4.0.4.6-unk");
    BOOST_CHECK(claim.m_organization == "Example Org");

    BOOST_CHECK(claim.m_block_subsidy == 10.0 * COIN);

    BOOST_CHECK(claim.m_magnitude == 123);
    BOOST_CHECK(claim.m_research_subsidy == 47.25 * COIN);
    BOOST_CHECK(claim.m_magnitude_unit == 0.123456);

    BOOST_CHECK(claim.m_signature == signature);

    BOOST_CHECK(claim.m_quorum_hash == quorum_hash);
    BOOST_CHECK(claim.m_quorum_address == quorum_address);
    BOOST_CHECK(claim.m_superblock->GetHash() == superblock.GetHash());
}

BOOST_AUTO_TEST_CASE(it_behaves_like_a_contract_payload)
{
    const GRC::Claim claim = GetResearcherClaim();

    BOOST_CHECK(claim.ContractType() == GRC::ContractType::CLAIM);
    BOOST_CHECK(claim.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(claim.LegacyKeyString().empty() == true);
    BOOST_CHECK(claim.LegacyValueString().empty() == true);
    BOOST_CHECK(claim.RequiredBurnAmount() > 0);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_claim_is_well_formed)
{
    const GRC::Claim claim = GetInvestorClaim();

    BOOST_CHECK(claim.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_is_a_research_reward_claim)
{
    GRC::Claim claim = GetResearcherClaim();

    BOOST_CHECK(claim.HasResearchReward() == true);

    claim = GetInvestorClaim();

    BOOST_CHECK(claim.HasResearchReward() == false);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_contains_a_superblock)
{
    GRC::Claim claim = GetInvestorClaim();

    BOOST_CHECK(claim.ContainsSuperblock() == false);

    claim.m_superblock.Replace(GetTestSuperblock());

    BOOST_CHECK(claim.ContainsSuperblock() == true);
}

BOOST_AUTO_TEST_CASE(it_sums_the_block_and_research_reward_subsidies)
{
    GRC::Claim claim = GetInvestorClaim();

    BOOST_CHECK(claim.TotalSubsidy() == 10.0 * COIN);

    claim = GetResearcherClaim();

    BOOST_CHECK(claim.TotalSubsidy() == 13345600000);
}

BOOST_AUTO_TEST_CASE(it_signs_itself_with_the_supplied_beacon_private_key)
{
    GRC::Claim claim = GetResearcherClaim();

    const uint256 last_block_hash;
    const CTransaction coinstake_tx;
    CKey private_key = GetTestPrivateKey();

    BOOST_CHECK(claim.Sign(private_key, last_block_hash, coinstake_tx) == true);

    GRC::Cpid cpid = claim.m_mining_id.TryCpid().get();

    const uint256 hashed = (CHashWriter(SER_NETWORK, PROTOCOL_VERSION)
        << cpid
        << last_block_hash
        << coinstake_tx)
        .GetHash();

    private_key = GetTestPrivateKey();

    BOOST_CHECK(private_key.Verify(hashed, claim.m_signature));
}

BOOST_AUTO_TEST_CASE(it_signs_a_v2_claim_with_the_supplied_beacon_private_key)
{
    GRC::Claim claim = GetResearcherClaim();
    claim.m_version = 2;

    const uint256 last_block_hash;
    const CTransaction coinstake_tx;
    CKey private_key = GetTestPrivateKey();

    BOOST_CHECK(claim.Sign(private_key, last_block_hash, coinstake_tx) == true);

    GRC::Cpid cpid = claim.m_mining_id.TryCpid().get();

    const uint256 hashed = Hash(
        cpid.Raw().begin(),
        cpid.Raw().end(),
        last_block_hash.begin(),
        last_block_hash.end());

    private_key = GetTestPrivateKey();

    BOOST_CHECK(private_key.Verify(hashed, claim.m_signature));
}

BOOST_AUTO_TEST_CASE(it_refuses_to_sign_itself_with_an_invalid_private_key)
{
    GRC::Claim claim = GetResearcherClaim();

    const uint256 last_block_hash;
    const CTransaction coinstake_tx;
    CKey private_key;

    BOOST_CHECK(claim.Sign(private_key, last_block_hash, coinstake_tx) == false);
    BOOST_CHECK(claim.m_signature.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_refuses_to_sign_an_investor_claim)
{
    GRC::Claim claim = GetInvestorClaim();

    const uint256 last_block_hash;
    const CTransaction coinstake_tx;
    CKey private_key = GetTestPrivateKey();

    BOOST_CHECK(claim.Sign(private_key, last_block_hash, coinstake_tx) == false);
    BOOST_CHECK(claim.m_signature.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_verifies_a_signature_for_a_research_reward_claim)
{
    GRC::Claim claim = GetResearcherClaim();

    const uint256 last_block_hash;
    const CTransaction coinstake_tx;
    CKey private_key = GetTestPrivateKey();

    GRC::Cpid cpid = claim.m_mining_id.TryCpid().get();

    const uint256 hashed = (CHashWriter(SER_NETWORK, PROTOCOL_VERSION)
        << cpid
        << last_block_hash
        << coinstake_tx)
        .GetHash();

    private_key.Sign(hashed, claim.m_signature);

    BOOST_CHECK(claim.VerifySignature(
        private_key.GetPubKey(),
        last_block_hash,
        coinstake_tx));
}

BOOST_AUTO_TEST_CASE(it_verifies_a_signature_for_a_v2_research_reward_claim)
{
    GRC::Claim claim = GetResearcherClaim();
    claim.m_version = 2;

    const uint256 last_block_hash;
    const CTransaction coinstake_tx;
    CKey private_key = GetTestPrivateKey();

    GRC::Cpid cpid = claim.m_mining_id.TryCpid().get();

    const uint256 hashed = Hash(
        cpid.Raw().begin(),
        cpid.Raw().end(),
        last_block_hash.begin(),
        last_block_hash.end());

    private_key.Sign(hashed, claim.m_signature);

    BOOST_CHECK(claim.VerifySignature(
        private_key.GetPubKey(),
        last_block_hash,
        coinstake_tx));
}

BOOST_AUTO_TEST_CASE(it_generates_a_hash_for_an_investor_claim)
{
    GRC::Claim claim = GetInvestorClaim();

    CHashWriter hasher(SER_GETHASH, claim.m_version);

    hasher << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_quorum_hash;

    BOOST_CHECK(claim.GetHash() == hasher.GetHash());
}

BOOST_AUTO_TEST_CASE(it_generates_a_hash_for_a_research_reward_claim)
{
    GRC::Claim claim = GetResearcherClaim();

    CHashWriter hasher(SER_GETHASH, claim.m_version);

    hasher << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_research_subsidy
        << claim.m_signature
        << claim.m_quorum_hash;

    BOOST_CHECK(claim.GetHash() == hasher.GetHash());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_investor)
{
    GRC::Claim claim = GetInvestorClaim();

    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    expected << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_quorum_hash;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    claim.Serialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_investor_with_superblock)
{
    GRC::Claim claim = GetInvestorClaim();

    claim.m_superblock.Replace(GetTestSuperblock());
    claim.m_quorum_hash = claim.m_superblock->GetHash();

    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    expected << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_quorum_hash
        << claim.m_superblock;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    claim.Serialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_investor)
{
    GRC::Claim expected = GetInvestorClaim();
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << expected.m_version
        << expected.m_mining_id
        << expected.m_client_version
        << expected.m_organization
        << expected.m_block_subsidy
        << expected.m_quorum_hash;

    GRC::Claim claim;

    claim.Unserialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK(claim.m_version == expected.m_version);
    BOOST_CHECK(claim.m_mining_id == expected.m_mining_id);
    BOOST_CHECK(claim.m_client_version == expected.m_client_version);
    BOOST_CHECK(claim.m_organization == expected.m_organization);
    BOOST_CHECK(claim.m_block_subsidy == expected.m_block_subsidy);
    BOOST_CHECK(claim.m_quorum_hash == expected.m_quorum_hash);

    BOOST_CHECK(claim.m_research_subsidy == 0);
    BOOST_CHECK(claim.m_magnitude == 0.0);
    BOOST_CHECK(claim.m_magnitude_unit == 0.0);
    BOOST_CHECK(claim.m_signature.empty() == true);
    BOOST_CHECK(claim.m_quorum_address.empty() == true);
    BOOST_CHECK(claim.m_superblock->WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_investor_with_superblock)
{
    GRC::Claim expected = GetInvestorClaim();

    expected.m_superblock.Replace(GetTestSuperblock());
    expected.m_quorum_hash = expected.m_superblock->GetHash();

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << expected.m_version
        << expected.m_mining_id
        << expected.m_client_version
        << expected.m_organization
        << expected.m_block_subsidy
        << expected.m_quorum_hash
        << expected.m_superblock;

    GRC::Claim claim;

    claim.Unserialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK(claim.m_version == expected.m_version);
    BOOST_CHECK(claim.m_mining_id == expected.m_mining_id);
    BOOST_CHECK(claim.m_client_version == expected.m_client_version);
    BOOST_CHECK(claim.m_organization == expected.m_organization);
    BOOST_CHECK(claim.m_block_subsidy == expected.m_block_subsidy);

    BOOST_CHECK(claim.m_quorum_hash == expected.m_quorum_hash);
    BOOST_CHECK(claim.m_quorum_address.empty() == true);
    BOOST_CHECK(claim.m_superblock->WellFormed() == true);
    BOOST_CHECK(claim.m_superblock->GetHash() == expected.m_superblock->GetHash());

    BOOST_CHECK(claim.m_research_subsidy == 0);
    BOOST_CHECK(claim.m_magnitude == 0.0);
    BOOST_CHECK(claim.m_magnitude_unit == 0.0);
    BOOST_CHECK(claim.m_signature.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_researcher)
{
    GRC::Claim claim = GetResearcherClaim();

    CDataStream expected(SER_NETWORK, claim.m_version);

    expected << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_research_subsidy
        << claim.m_signature
        << claim.m_quorum_hash;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    claim.Serialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_researcher_with_superblock)
{
    GRC::Claim claim = GetResearcherClaim();

    claim.m_superblock.Replace(GetTestSuperblock());
    claim.m_quorum_hash = claim.m_superblock->GetHash();

    CDataStream expected(SER_NETWORK, claim.m_version);

    expected << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_research_subsidy
        << claim.m_signature
        << claim.m_quorum_hash
        << claim.m_superblock;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    claim.Serialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_researcher)
{
    GRC::Claim expected = GetResearcherClaim();

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << expected.m_version
        << expected.m_mining_id
        << expected.m_client_version
        << expected.m_organization
        << expected.m_block_subsidy
        << expected.m_research_subsidy
        << expected.m_signature
        << expected.m_quorum_hash;

    GRC::Claim claim;

    claim.Unserialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK(claim.m_version == expected.m_version);
    BOOST_CHECK(claim.m_mining_id == expected.m_mining_id);
    BOOST_CHECK(claim.m_client_version == expected.m_client_version);
    BOOST_CHECK(claim.m_organization == expected.m_organization);
    BOOST_CHECK(claim.m_block_subsidy == expected.m_block_subsidy);

    BOOST_CHECK(claim.m_research_subsidy == expected.m_research_subsidy);
    BOOST_CHECK(claim.m_magnitude == 0.0);
    BOOST_CHECK(claim.m_magnitude_unit == 0.0);
    BOOST_CHECK(claim.m_signature == expected.m_signature);

    BOOST_CHECK(claim.m_quorum_hash == expected.m_quorum_hash);
    BOOST_CHECK(claim.m_quorum_address.empty() == true);
    BOOST_CHECK(claim.m_superblock->WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_researcher_with_superblock)
{
    GRC::Claim expected = GetResearcherClaim();

    expected.m_superblock.Replace(GetTestSuperblock());
    expected.m_quorum_hash = expected.m_superblock->GetHash();

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << expected.m_version
        << expected.m_mining_id
        << expected.m_client_version
        << expected.m_organization
        << expected.m_block_subsidy
        << expected.m_research_subsidy
        << expected.m_signature
        << expected.m_quorum_hash
        << expected.m_superblock;

    GRC::Claim claim;

    claim.Unserialize(stream, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK(claim.m_version == expected.m_version);
    BOOST_CHECK(claim.m_mining_id == expected.m_mining_id);
    BOOST_CHECK(claim.m_client_version == expected.m_client_version);
    BOOST_CHECK(claim.m_organization == expected.m_organization);
    BOOST_CHECK(claim.m_block_subsidy == expected.m_block_subsidy);

    BOOST_CHECK(claim.m_research_subsidy == expected.m_research_subsidy);
    BOOST_CHECK(claim.m_magnitude == 0.0);
    BOOST_CHECK(claim.m_magnitude_unit == 0.0);
    BOOST_CHECK(claim.m_signature == expected.m_signature);

    BOOST_CHECK(claim.m_quorum_hash == expected.m_quorum_hash);
    BOOST_CHECK(claim.m_quorum_address.empty() == true);
    BOOST_CHECK(claim.m_superblock->WellFormed() == true);
    BOOST_CHECK(claim.m_superblock->GetHash() == expected.m_superblock->GetHash());
}

BOOST_AUTO_TEST_SUITE_END()
