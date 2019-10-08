#include "beacon.h"
#include "key.h"
#include "neuralnet/claim.h"
#include "util.h"

using namespace NN;

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
    if (block_hash == 0) {
        return "0";
    }

    return block_hash.ToString();
}

//!
//! \brief Get the hash of a subset of the data in the claim object used as
//! input to sign or verify a research reward claim.
//!
//! \param claim           Claim to generate a hash for.
//! \param last_block_hash Hash of the block that preceeds the block that
//! contains the claim.
//!
//! \return Hash of the CPID and last block hash contained in the claim.
//!
uint256 GetClaimHash(const Claim& claim, const uint256& last_block_hash)
{
    const CpidOption cpid = claim.m_mining_id.TryCpid();

    if (!cpid) {
        return uint256(0);
    }

    if (claim.m_version > 1) {
        return Hash(
            cpid->Raw().begin(),
            cpid->Raw().end(),
            last_block_hash.begin(),
            last_block_hash.end());
    }

    const std::string cpid_hex = cpid->ToString();
    const std::string hash_hex = BlockHashToString(last_block_hash);

    return Hash(cpid_hex.begin(), cpid_hex.end(), hash_hex.begin(), hash_hex.end());
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

bool NN::VerifyClaim(const Claim& claim, const uint256& last_block_hash)
{
    if (!claim.m_mining_id.Valid()) {
        return error("VerifyClaim(): Invalid mining ID.");
    }

    const CpidOption cpid = claim.m_mining_id.TryCpid();

    if (!cpid) {
        // Investor claims are not signed by a beacon key.
        return true;
    }

    const std::string cpid_str = cpid->ToString();
    const std::string beacon_key = GetBeaconPublicKey(cpid_str, false);

    if (claim.VerifySignature(ParseHex(beacon_key), last_block_hash)) {
        return true;
    }

    for (const auto& beacon_alt_key : GetAlternativeBeaconKeys(cpid_str)) {
        if (claim.VerifySignature(ParseHex(beacon_alt_key), last_block_hash)) {
            LogPrintf("WARNING: VerifyClaim(): Good signature with alternative key.");
            return true;
        }
    }

    LogPrintf("WARNING: VerifyClaim(): Block key mismatch.");

    return false;
}

// -----------------------------------------------------------------------------
// Class: Claim
// -----------------------------------------------------------------------------

Claim::Claim() : Claim(CURRENT_VERSION)
{
}

Claim::Claim(uint32_t version)
    : m_version(version)
    , m_block_subsidy(0)
    , m_last_block_hash(0)
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
        case 22: c.m_superblock = Superblock::UnpackLegacy(s[22]);
        case 21: if (!c.m_quorum_hash.Valid())
                    c.m_quorum_hash = QuorumHash::Parse(s[21]);
        case 20: //c.OrganizationKey = s[20];
        case 19: c.m_organization = std::move(s[19]);
        case 18: c.m_block_subsidy = RoundFromString(s[18], subsidy_places);
        case 17: //c.m_last_block_hash = uint256(s[17]);
        case 16: c.m_quorum_address = s[16];
        case 15: c.m_magnitude = RoundFromString(s[15], 0);
        case 14: //c.cpidv2 = s[14];
        case 13: //c.m_rsa_weight = RoundFromString(s[13], 0);
        case 12: //c.m_last_payment_time = RoundFromString(s[12], 0);
        case 11: c.m_research_subsidy = RoundFromString(s[11], 2);
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
    return m_version > 0 && m_version <= Claim::CURRENT_VERSION
        && (m_version == 1
            || (m_mining_id.Valid()
                && !m_client_version.empty()
                && m_client_version.size() <= 30
                && m_organization.size() <= 50
                && m_block_subsidy > 0
                && (m_mining_id.Which() == MiningId::Kind::INVESTOR
                    || (m_research_subsidy > 0 && m_signature.size() > 0))
                && (!m_quorum_hash.Valid() || m_quorum_address.size() > 0)
            )
        );
}

bool Claim::HasResearchReward() const
{
    return m_mining_id.Which() == MiningId::Kind::CPID;
}

bool Claim::ContainsSuperblock() const
{
    return m_superblock.WellFormed();
}

double Claim::TotalSubsidy() const
{
    return m_block_subsidy + m_research_subsidy;
}

bool Claim::Sign(CKey& private_key, const uint256& last_block_hash)
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return false;
    }

    if (!private_key.Sign(GetClaimHash(*this, last_block_hash), m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool Claim::VerifySignature(
    const CPubKey& public_key,
    const uint256& last_block_hash) const
{
    CKey key;

    if (!key.SetPubKey(public_key)) {
        return false;
    }

    return key.Verify(GetClaimHash(*this, last_block_hash), m_signature);
}

uint256 Claim::GetHash() const
{
    return SerializeHash(*this);
}

std::string Claim::ToString(const int block_version) const
{
    constexpr char delim[] = "<|>";

    const int subsidy_places = block_version < 8 ? 2 : 8;

    // Note: Commented-out items recorded to document removed fields:
    //
    return m_mining_id.ToString()
        + delim // + mcpid.projectname
        + delim // + mcpid.aesskein
        + delim // + RoundToString(mcpid.rac, 0)
        + delim // + RoundToString(mcpid.pobdifficulty, 5)
        + delim // + RoundToString((double)mcpid.diffbytes, 0)
        + delim // + mcpid.enccpid
        + delim // + mcpid.encaes
        + delim // + RoundToString(mcpid.nonce, 0)
        + delim // + RoundToString(mcpid.NetworkRAC, 0)
        + delim + m_client_version
        + delim + RoundToString(m_research_subsidy, subsidy_places)
        + delim // + std::to_string(m_last_payment_time)
        + delim // + std::to_string(m_rsa_weight)
        + delim // + mcpid.cpidv2
        + delim + std::to_string(m_magnitude)
        + delim + m_quorum_address
        + delim + BlockHashToString(m_last_block_hash)
        + delim + RoundToString(m_block_subsidy, subsidy_places)
        + delim + m_organization
        + delim // + mcpid.OrganizationKey
        + delim + m_quorum_hash.ToString()
        + delim + (m_superblock.WellFormed() ? m_superblock.PackLegacy() : "")
        + delim // + RoundToString(mcpid.ResearchSubsidy2,2)
        + delim // + RoundToString(m_research_age, 6)
        + delim + RoundToString(m_magnitude_unit, MAG_UNIT_PLACES)
        + delim // + RoundToString(m_average_magnitude, 2)
        + delim // + BlockHashToString(m_last_por_block_hash)
        + delim // + mcpid.CurrentNeuralHash
        + delim // + m_public_key.ToString()
        + delim + EncodeBase64(m_signature.data(), m_signature.size());
}
