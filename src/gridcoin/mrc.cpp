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
//! \brief Get the hash of a subset of the data in the mrc object used as
//! input to sign or verify a research reward claim.
//!
//! \param mrc           MRC to generate a hash for.
//! \param last_block_hash Hash of the block at the head of the chain.
//! \param mrc    mrc transaction of the block that contains
//! the claim.
//!
//! \return Hash of the CPID and last block hash contained in the claim.
//!
uint256 GetMRCHash(
    const MRC& mrc,
    const uint256& last_block_hash)
{
    const CpidOption cpid = mrc.m_mining_id.TryCpid();

    if (!cpid) {
        return uint256();
    }

        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher << *cpid << last_block_hash;

        return hasher.GetHash();
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


CAmount MRC::ComputeMRCFee()
{
    CAmount fee = 0;

    //stub

    return fee;
}

bool MRC::Sign(
    CKey& private_key,
    const uint256& last_block_hash)
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return false;
    }

    const uint256 hash = GetMRCHash(*this, last_block_hash);

    if (!private_key.Sign(hash, m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool MRC::VerifySignature(
    const CPubKey& public_key,
    const uint256& last_block_hash) const
{
    CKey key;

    if (!key.SetPubKey(public_key)) {
        return false;
    }

    const uint256 hash = GetMRCHash(*this, last_block_hash);

    return key.Verify(hash, m_signature);
}

uint256 MRC::GetHash() const
{
    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

    // MRC contracts do not use the contract action specifier:
    Serialize(hasher, ContractAction::UNKNOWN);

    return hasher.GetHash();
}
