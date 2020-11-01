// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "key.h"
#include "main.h"
#include "gridcoin/claim.h"
#include "util.h"

using namespace GRC;

namespace {
//!
//! \brief Convert a block hash to a hex-encoded string for legacy serialized
//! claims.
//!
//! \param block_hash SHA256 hash of the block to format a string for.
//!
//! \return Hex-encoded bytes in the hash, or "0" for a blank hash to conserve
//! space.
//!
std::string BlockHashToString(const uint256& block_hash)
{
    if (block_hash.IsNull()) {
        return "0";
    }

    return block_hash.ToString();
}

//!
//! \brief Get the hash of a subset of the data in the claim object used as
//! input to sign or verify a research reward claim.
//!
//! \param claim           Claim to generate a hash for.
//! \param last_block_hash Hash of the block that precedes the block that
//! contains the claim.
//! \param coinstake_tx    Coinstake transaction of the block that contains
//! the claim.
//!
//! \return Hash of the CPID and last block hash contained in the claim.
//!
uint256 GetClaimHash(
    const Claim& claim,
    const uint256& last_block_hash,
    const CTransaction& coinstake_tx)
{
    const CpidOption cpid = claim.m_mining_id.TryCpid();

    if (!cpid) {
        return uint256();
    }

    if (claim.m_version >= 2) {
        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher << *cpid << last_block_hash;

        if (claim.m_version >= 3) {
            hasher << coinstake_tx;
        }

        return hasher.GetHash();
    }

    const std::string cpid_hex = cpid->ToString();
    const std::string hash_hex = BlockHashToString(last_block_hash);

    return Hash(cpid_hex.begin(), cpid_hex.end(), hash_hex.begin(), hash_hex.end());
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Class: Claim
// -----------------------------------------------------------------------------

Claim::Claim() : Claim(CURRENT_VERSION)
{
}

Claim::Claim(uint32_t version)
    : m_version(version)
    , m_block_subsidy(0)
    , m_research_subsidy(0)
    , m_magnitude(0)
    , m_magnitude_unit(0)
{
}

Claim Claim::Parse(const std::string& claim, int block_version)
{
    const int subsidy_places = block_version < 8 ? 2 : 8;
    std::vector<std::string> s = split(claim, "<|>");

    // Legacy string claims (BoincBlocks) always parse to version 1:
    //
    Claim c(1);

    // Note: Commented-out items recorded to document removed fields:
    //
    switch (std::min<size_t>(s.size() - 1, 30)) {
        case 30: c.m_signature = DecodeBase64(s[30].c_str());
        case 29: //c.m_public_key = CPubKey::Parse(s[29]);
        case 28: c.m_quorum_hash = QuorumHash::Parse(s[28]);
        case 27: //c.m_last_por_block_hash = uint256(s[27]);
        case 26: //c.m_average_magnitude = RoundFromString(s[26], 2);
        case 25: c.m_magnitude_unit = RoundFromString(s[25], MAG_UNIT_PLACES);
        case 24: //c.m_research_age = RoundFromString(s[24], 6);
        case 23: //c.ResearchSubsidy2 = RoundFromString(s[23], subsidy_places);
        case 22: c.m_superblock.Replace(Superblock::UnpackLegacy(s[22]));
        case 21: if (!c.m_quorum_hash.Valid())
                    c.m_quorum_hash = QuorumHash::Parse(s[21]);
        case 20: //c.OrganizationKey = s[20];
        case 19: c.m_organization = std::move(s[19]);
        case 18: c.m_block_subsidy = RoundFromString(s[18], subsidy_places) * COIN;
        case 17: //c.m_last_block_hash = uint256(s[17]);
        case 16: c.m_quorum_address = s[16];
        case 15: c.m_magnitude = RoundFromString(s[15], 0);
        case 14: //c.cpidv2 = s[14];
        case 13: //c.m_rsa_weight = RoundFromString(s[13], 0);
        case 12: //c.m_last_payment_time = RoundFromString(s[12], 0);
        case 11: c.m_research_subsidy = RoundFromString(s[11], 2) * COIN;
        case 10: c.m_client_version = std::move(s[10]);
        case  9: //c.NetworkRAC = RoundFromString(s[9], 0);
        case  8:
            c.m_mining_id = MiningId::Parse(s[0]);
            //c.projectname = s[1];
            //boost::to_lower(c.projectname);
            //c.aesskein = s[2];
            //c.rac = RoundFromString(s[3],0);
            //c.pobdifficulty = RoundFromString(s[4],6);
            //c.diffbytes = (unsigned int)RoundFromString(s[5],0);
            //c.enccpid = s[6];
            //c.encboincpublickey = s[6];
            //c.encaes = s[7];
            //c.nonce = RoundFromString(s[8],0);
            break;
    }

    return c;
}

bool Claim::WellFormed() const
{
    if (m_version <= 0 || m_version > Claim::CURRENT_VERSION) {
        return false;
    }

    if (m_version == 1) {
        return true;
    }

    if (!m_mining_id.Valid()) {
        return false;
    }

    if (m_client_version.empty()) {
        return false;
    }

    if (m_block_subsidy <= 0) {
        return false;
    }

    if (m_mining_id.Which() == MiningId::Kind::CPID) {
        if (m_research_subsidy <= 0 || m_signature.empty()) {
            return false;
        }
    }

    if (m_quorum_hash.Valid() && !m_superblock->WellFormed()) {
        return false;
    }

    return true;
}

bool Claim::HasResearchReward() const
{
    return m_mining_id.Which() == MiningId::Kind::CPID;
}

bool Claim::ContainsSuperblock() const
{
    return m_superblock->WellFormed();
}

CAmount Claim::TotalSubsidy() const
{
    return m_block_subsidy + m_research_subsidy;
}

bool Claim::Sign(
    CKey& private_key,
    const uint256& last_block_hash,
    const CTransaction& coinstake_tx)
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return false;
    }

    const uint256 hash = GetClaimHash(*this, last_block_hash, coinstake_tx);

    if (!private_key.Sign(hash, m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool Claim::VerifySignature(
    const CPubKey& public_key,
    const uint256& last_block_hash,
    const CTransaction& coinstake_tx) const
{
    CKey key;

    if (!key.SetPubKey(public_key)) {
        return false;
    }

    const uint256 hash = GetClaimHash(*this, last_block_hash, coinstake_tx);

    return key.Verify(hash, m_signature);
}

uint256 Claim::GetHash() const
{
    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

    // Claim contracts do not use the contract action specifier:
    Serialize(hasher, ContractAction::UNKNOWN);

    return hasher.GetHash();
}
