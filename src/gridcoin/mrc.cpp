// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "key.h"
#include "main.h"
#include "gridcoin/mrc.h"
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
    const MRC& mrc,
    const uint256& last_block_hash,
    const CTransaction& coinstake_tx)
{
    const CpidOption cpid = mrc.m_mining_id.TryCpid();

    if (!cpid) {
        return uint256();
    }

    if (mrc.m_version >= 2) {
        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher << *cpid << last_block_hash;

        if (mrc.m_version >= 3) {
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
// Class: MRC
// -----------------------------------------------------------------------------

MRC::MRC() : MRC(CURRENT_VERSION)
{
}

MRC::MRC(uint32_t version)
    : m_version(version)
    , m_research_subsidy(0)
    , m_magnitude(0)
    , m_magnitude_unit(0)
{
}

bool MRC::WellFormed() const
{
    if (m_version <= 0 || m_version > MRC::CURRENT_VERSION) {
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

    if (m_mining_id.Which() == MiningId::Kind::CPID) {
        if (m_research_subsidy <= 0 || m_signature.empty()) {
            return false;
        }
    }

    return true;
}

bool MRC::HasResearchReward() const
{
    return m_mining_id.Which() == MiningId::Kind::CPID;
}

bool MRC::Sign(
    CKey& private_key,
    const uint256& last_block_hash,
    const CTransaction& mrc_tx)
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return false;
    }

    const uint256 hash = GetClaimHash(*this, last_block_hash, mrc_tx);

    if (!private_key.Sign(hash, m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool MRC::VerifySignature(
    const CPubKey& public_key,
    const uint256& last_block_hash,
    const CTransaction& mrc_tx) const
{
    CKey key;

    if (!key.SetPubKey(public_key)) {
        return false;
    }

    const uint256 hash = GetClaimHash(*this, last_block_hash, mrc_tx);

    return key.Verify(hash, m_signature);
}

uint256 MRC::GetHash() const
{
    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

    // MRC contracts do not use the contract action specifier:
    Serialize(hasher, ContractAction::UNKNOWN);

    return hasher.GetHash();
}
