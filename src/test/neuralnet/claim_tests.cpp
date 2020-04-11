#include "neuralnet/claim.h"

#include "key.h"
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
NN::Claim GetInvestorClaim()
{
    NN::Claim claim;

    claim.m_mining_id = NN::MiningId::ForInvestor();
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
NN::Claim GetResearcherClaim()
{
    NN::Claim claim;

    claim.m_mining_id = NN::Cpid::Parse("00010203040506070809101112131415");
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
NN::Superblock GetTestSuperblock(uint32_t version = NN::Superblock::CURRENT_VERSION)
{
    NN::Superblock superblock;

    superblock.m_version = version;
    superblock.m_cpids.Add(NN::Cpid(), NN::Magnitude::RoundFrom(123));
    superblock.m_projects.Add("project", NN::Superblock::ProjectStats());

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
    const NN::Claim claim;

    BOOST_CHECK(claim.m_version == NN::Claim::CURRENT_VERSION);
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
    BOOST_CHECK(claim.m_superblock.m_cpids.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_specified_version)
{
    const NN::Claim claim(1);

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
    BOOST_CHECK(claim.m_superblock.m_cpids.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_parses_a_legacy_boincblock_string_for_researcher)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const std::string quorum_address = "mk8PmpcTGLCZky8YqFHEEwXs5px3hGfQBG";

    const NN::Superblock superblock = GetTestSuperblock(1);

    // Legacy claims only contain MD5 quorum hashes:
    const NN::QuorumHash quorum_hash(NN::QuorumHash::Md5Sum {
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

    const NN::Claim claim = NN::Claim::Parse(
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
    BOOST_CHECK(claim.m_superblock.GetHash() == superblock.GetHash());
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_claim_is_well_formed)
{
    const NN::Claim claim = GetInvestorClaim();

    BOOST_CHECK(claim.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_is_a_research_reward_claim)
{
    NN::Claim claim = GetResearcherClaim();

    BOOST_CHECK(claim.HasResearchReward() == true);

    claim = GetInvestorClaim();

    BOOST_CHECK(claim.HasResearchReward() == false);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_contains_a_superblock)
{
    NN::Claim claim = GetInvestorClaim();

    BOOST_CHECK(claim.ContainsSuperblock() == false);

    claim.m_superblock = GetTestSuperblock();

    BOOST_CHECK(claim.ContainsSuperblock() == true);
}

BOOST_AUTO_TEST_CASE(it_sums_the_block_and_research_reward_subsidies)
{
    NN::Claim claim = GetInvestorClaim();

    BOOST_CHECK(claim.TotalSubsidy() == 10.0 * COIN);

    claim = GetResearcherClaim();

    BOOST_CHECK(claim.TotalSubsidy() == 13345600000);
}

BOOST_AUTO_TEST_CASE(it_signs_itself_with_the_supplied_beacon_private_key)
{
    NN::Claim claim = GetResearcherClaim();

    const uint256 last_block_hash;
    CKey private_key = GetTestPrivateKey();

    BOOST_CHECK(claim.Sign(private_key, last_block_hash) == true);

    NN::Cpid cpid = claim.m_mining_id.TryCpid().get();

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
    NN::Claim claim = GetResearcherClaim();

    CKey private_key;
    uint256 last_block_hash;

    BOOST_CHECK(claim.Sign(private_key, last_block_hash) == false);
    BOOST_CHECK(claim.m_signature.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_refuses_to_sign_an_investor_claim)
{
    NN::Claim claim = GetInvestorClaim();

    const uint256 last_block_hash;
    CKey private_key = GetTestPrivateKey();

    BOOST_CHECK(claim.Sign(private_key, last_block_hash) == false);
    BOOST_CHECK(claim.m_signature.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_verifies_a_signature_for_a_research_reward_claim)
{
    NN::Claim claim = GetResearcherClaim();

    const uint256 last_block_hash;
    CKey private_key = GetTestPrivateKey();

    NN::Cpid cpid = claim.m_mining_id.TryCpid().get();

    const uint256 hashed = Hash(
        cpid.Raw().begin(),
        cpid.Raw().end(),
        last_block_hash.begin(),
        last_block_hash.end());

    private_key.Sign(hashed, claim.m_signature);

    BOOST_CHECK(claim.VerifySignature(private_key.GetPubKey(), last_block_hash));
}

BOOST_AUTO_TEST_CASE(it_generates_a_hash_for_an_investor_claim)
{
    NN::Claim claim = GetInvestorClaim();

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
    NN::Claim claim = GetResearcherClaim();

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

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_legacy_boincblock_string)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const std::string quorum_address = "mk8PmpcTGLCZky8YqFHEEwXs5px3hGfQBG";

    const NN::Superblock superblock = GetTestSuperblock(1);

    // Legacy claims only contain MD5 quorum hashes:
    const NN::QuorumHash quorum_hash(NN::QuorumHash::Md5Sum {
        0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08,
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
    });

    const std::vector<unsigned char> signature {
        0x7b, 0x85, 0xc8, 0x3c, 0x92, 0xd9, 0x74, 0x8e,
        0xa3, 0xd2, 0x26, 0x16, 0x6f, 0x9a, 0x00, 0x6c,
        0x6f, 0x0a, 0x97, 0x97, 0xa9, 0x3a, 0x52, 0xd0,
        0xb9, 0x4f, 0xbb, 0x29, 0x61, 0xbe, 0xd5, 0xcc,
    };

    const std::string sig64 = EncodeBase64(signature.data(), signature.size());

    NN::Claim claim;

    claim.m_mining_id = cpid;
    claim.m_client_version = "v4.0.4.6-unk";
    claim.m_organization = "Example Org";
    claim.m_block_subsidy = 10.0 * COIN;
    claim.m_research_subsidy = 47.24638888 * COIN;
    claim.m_magnitude = 123;
    claim.m_magnitude_unit = 0.123456;
    claim.m_signature = signature;
    claim.m_quorum_hash = quorum_hash;
    claim.m_quorum_address = quorum_address;
    claim.m_superblock = superblock;

    BOOST_CHECK(claim.ToString(8) ==
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
        "<|>0"                           // Last block hash           (obsolete)
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
        "<|>"                            // Current Neural hash       (obsolete)
        "<|>"                            // Public key                (obsolete)
        "<|>" + sig64                    // Beacon signature
    );
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_investor)
{
    NN::Claim claim = GetInvestorClaim();

    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    expected << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_quorum_hash;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << claim;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_investor_with_superblock)
{
    NN::Claim claim = GetInvestorClaim();

    claim.m_superblock = GetTestSuperblock();
    claim.m_quorum_hash = claim.m_superblock.GetHash();

    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    expected << claim.m_version
        << claim.m_mining_id
        << claim.m_client_version
        << claim.m_organization
        << claim.m_block_subsidy
        << claim.m_quorum_hash
        << claim.m_superblock;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << claim;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_investor)
{
    NN::Claim expected = GetInvestorClaim();
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << expected.m_version
        << expected.m_mining_id
        << expected.m_client_version
        << expected.m_organization
        << expected.m_block_subsidy
        << expected.m_quorum_hash;

    NN::Claim claim;

    stream >> claim;

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
    BOOST_CHECK(claim.m_superblock.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_investor_with_superblock)
{
    NN::Claim expected = GetInvestorClaim();

    expected.m_superblock = GetTestSuperblock();
    expected.m_quorum_hash = expected.m_superblock.GetHash();

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << expected.m_version
        << expected.m_mining_id
        << expected.m_client_version
        << expected.m_organization
        << expected.m_block_subsidy
        << expected.m_quorum_hash
        << expected.m_superblock;

    NN::Claim claim;

    stream >> claim;

    BOOST_CHECK(claim.m_version == expected.m_version);
    BOOST_CHECK(claim.m_mining_id == expected.m_mining_id);
    BOOST_CHECK(claim.m_client_version == expected.m_client_version);
    BOOST_CHECK(claim.m_organization == expected.m_organization);
    BOOST_CHECK(claim.m_block_subsidy == expected.m_block_subsidy);

    BOOST_CHECK(claim.m_quorum_hash == expected.m_quorum_hash);
    BOOST_CHECK(claim.m_quorum_address.empty() == true);
    BOOST_CHECK(claim.m_superblock.WellFormed() == true);
    BOOST_CHECK(claim.m_superblock.GetHash() == expected.m_superblock.GetHash());

    BOOST_CHECK(claim.m_research_subsidy == 0);
    BOOST_CHECK(claim.m_magnitude == 0.0);
    BOOST_CHECK(claim.m_magnitude_unit == 0.0);
    BOOST_CHECK(claim.m_signature.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_researcher)
{
    NN::Claim claim = GetResearcherClaim();

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
    stream << claim;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_researcher_with_superblock)
{
    NN::Claim claim = GetResearcherClaim();

    claim.m_superblock = GetTestSuperblock();
    claim.m_quorum_hash = claim.m_superblock.GetHash();

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
    stream << claim;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_researcher)
{
    NN::Claim expected = GetResearcherClaim();

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << expected.m_version
        << expected.m_mining_id
        << expected.m_client_version
        << expected.m_organization
        << expected.m_block_subsidy
        << expected.m_research_subsidy
        << expected.m_signature
        << expected.m_quorum_hash;

    NN::Claim claim;

    stream >> claim;

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
    BOOST_CHECK(claim.m_superblock.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_researcher_with_superblock)
{
    NN::Claim expected = GetResearcherClaim();

    expected.m_superblock = GetTestSuperblock();
    expected.m_quorum_hash = expected.m_superblock.GetHash();

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

    NN::Claim claim;

    stream >> claim;

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
    BOOST_CHECK(claim.m_superblock.WellFormed() == true);
    BOOST_CHECK(claim.m_superblock.GetHash() == expected.m_superblock.GetHash());
}

BOOST_AUTO_TEST_SUITE_END()
