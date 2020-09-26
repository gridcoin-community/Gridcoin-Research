// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "main.h"
#include "gridcoin/voting/claims.h"

using namespace GRC;

namespace {
//!
//! \brief The expected size of each claim signature used to expand the fields
//! for voting transaction fee estimation.
//!
constexpr size_t SIGNATURE_BYTES_ESTIMATE = 71;

//!
//! \brief Hash the message for an address claim signature.
//!
//! \param claim   The claim to sign or verify.
//! \param message Message to verify the claim for.
//!
//! \return A SHA256 hash of the claim message for the signature.
//!
uint256 HashClaim(const AddressClaim& claim, const ClaimMessage& message)
{
    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);
    hasher << message << claim.m_outpoints;

    return hasher.GetHash();
}

//!
//! \brief Hash the message for a magnitude claim signature.
//!
//! \param claim   The claim to sign or verify.
//! \param message Message to verify the claim for.
//!
//! \return A SHA256 hash of the claim message for the signature.
//!
uint256 HashClaim(const MagnitudeClaim& claim, const ClaimMessage& message)
{
    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);
    hasher << message << claim.m_mining_id;

    return hasher.GetHash();
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: AddressClaim
// -----------------------------------------------------------------------------

bool AddressClaim::Sign(CKey& private_key, const ClaimMessage& message)
{
    if (!private_key.Sign(HashClaim(*this, message), m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool AddressClaim::VerifySignature(const ClaimMessage& message) const
{
    CKey key;

    if (!key.SetPubKey(m_public_key)) {
        return false;
    }

    return key.Verify(HashClaim(*this, message), m_signature);
}

// -----------------------------------------------------------------------------
// Class: MagnitudeClaim
// -----------------------------------------------------------------------------

bool MagnitudeClaim::Sign(CKey& private_key, const ClaimMessage& message)
{
    if (!private_key.Sign(HashClaim(*this, message), m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool MagnitudeClaim::VerifySignature(
    const CPubKey& public_key,
    const ClaimMessage& message) const
{
    CKey key;

    if (!key.SetPubKey(public_key)) {
        return false;
    }

    return key.Verify(HashClaim(*this, message), m_signature);
}

// -----------------------------------------------------------------------------
// Class: PollEligibilityClaim
// -----------------------------------------------------------------------------

void PollEligibilityClaim::ExpandDummySignatures()
{
    m_address_claim.m_signature.resize(SIGNATURE_BYTES_ESTIMATE);
}

// -----------------------------------------------------------------------------
// Class: VoteWeightClaim
// -----------------------------------------------------------------------------

void VoteWeightClaim::ExpandDummySignatures()
{
    if (m_magnitude_claim.m_mining_id.Which() == MiningId::Kind::CPID) {
        m_magnitude_claim.m_signature.resize(SIGNATURE_BYTES_ESTIMATE);
    }

    for (auto& claim : m_balance_claim.m_address_claims) {
        claim.m_signature.resize(SIGNATURE_BYTES_ESTIMATE);
    }
}
