// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "key.h"
#include "gridcoin/cpid.h"
#include "gridcoin/magnitude.h"
#include "serialize.h"

#include <vector>

class COutPoint;
class CTransaction;

namespace GRC {

class Poll;
class Vote;

//!
//! \brief A serialized message produced to sign and verify voting claims.
//!
using ClaimMessage = std::vector<uint8_t>;

//!
//! \brief Testifies to the ownership of the unspent amount of an address.
//!
class AddressClaim
{
public:
    std::vector<COutPoint> m_outpoints; //!< Unspent outputs to claim.
    std::vector<uint8_t> m_signature;   //!< Proves ownership of the outputs.
    CPubKey m_public_key;               //!< Public key of the outputs.

    //!
    //! \brief Initialize an empty, invalid address claim.
    //!
    AddressClaim()
    {
    }

    //!
    //! \brief Initialize an incomplete address claim from a set of outputs.
    //!
    AddressClaim(std::vector<COutPoint> outpoints)
        : m_outpoints(std::move(outpoints))
    {
    }

    //!
    //! \brief Determine whether an address claim compares less than another.
    //!
    bool operator<(const AddressClaim& other) const
    {
        return m_public_key < other.m_public_key;
    }

    //!
    //! \brief Determine whether an instance represents a complete address
    //! claim.
    //!
    //! The result of this method call does NOT guarantee that the claim is
    //! valid. The return value of \c true only indicates that the instance
    //! received each of the pieces of data needed for a well-formed claim.
    //!
    //! \return \c true if the claim contains each of the required elements.
    //!
    bool WellFormed() const
    {
        if (m_outpoints.empty() || m_signature.empty() || !m_public_key.IsValid()) {
            return false;
        }

        // Output set must be sorted and contain no duplicates:
        for (size_t i = 1; i < m_outpoints.size(); ++i) {
            if (m_outpoints[i] < m_outpoints[i - 1]) {
                return false;
            }
        }

        return true;
    }

    //!
    //! \brief Get the burn fee amount required for the claim.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const
    {
        // 0.001 GRC per claimed UTXO:
        return m_outpoints.size() * COIN / 1000;
    }

    //!
    //! \brief Sign the claim.
    //!
    //! \param private_key Private key that can spend the outputs in the claim.
    //! \param message     Message to sign the claim for.
    //!
    //! \return \c false if signing fails.
    //!
    bool Sign(CKey& private_key, const ClaimMessage& message);

    //!
    //! \brief Validate authenticity of the claim by verifying the signature.
    //!
    //! \param message Message to verify the claim for.
    //!
    //! \return \c true if the signature check passes for the supplied message.
    //!
    bool VerifySignature(const ClaimMessage& message) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_public_key);
        READWRITE(m_signature);
        READWRITE(m_outpoints);
    }
}; // AddressClaim

//!
//! \brief Testifies to the ownership of an unspent balance.
//!
class BalanceClaim
{
public:
    //!
    //! \brief Testifies to the ownership of the unspent amount of each of the
    //! addresses that comprise the balance.
    //!
    std::vector<AddressClaim> m_address_claims;

    //!
    //! \brief Initialize an empty balance claim.
    //!
    BalanceClaim()
    {
    }

    //!
    //! \brief Determine whether an instance represents a complete balance
    //! claim.
    //!
    //! The result of this method call does NOT guarantee that the claim is
    //! valid. The return value of \c true only indicates that the instance
    //! received each of the pieces of data needed for a well-formed claim.
    //!
    //! \return \c true if the claim contains each of the required elements.
    //!
    bool WellFormed() const
    {
        if (m_address_claims.empty()) {
            return true;
        }

        if (!m_address_claims.front().WellFormed()) {
            return false;
        }

        // Address set must be sorted and contain no duplicates:
        for (size_t i = 1; i < m_address_claims.size(); ++i) {
            if (m_address_claims[i] < m_address_claims[i - 1]) {
                return false;
            }

            if (!m_address_claims[i].WellFormed()) {
                return false;
            }
        }

        return true;
    }

    //!
    //! \brief Get the burn fee amount required for the claim.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const
    {
        CAmount amount = 0;

        for (const auto& claim : m_address_claims) {
            // 0.01 per address + a scaled fee based on the number of outputs:
            amount += (COIN / 100) + claim.RequiredBurnAmount();
        }

        return amount;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_address_claims);
    }
}; // BalanceClaim

//!
//! \brief Testifies to the ownership of a CPID.
//!
class MagnitudeClaim
{
public:
    MiningId m_mining_id;             //!< CPID to associate with the claim.
    uint256 m_beacon_txid;            //!< Locates the beacon contract on disk.
    std::vector<uint8_t> m_signature; //!< Proves ownership of the beacon.

    //!
    //! \brief Initialize an empty magnitude claim.
    //!
    MagnitudeClaim() : m_mining_id(MiningId::ForInvestor())
    {
    }

    //!
    //! \brief Determine whether an instance represents a complete magnitude
    //! claim.
    //!
    //! The result of this method call does NOT guarantee that the claim is
    //! valid. The return value of \c true only indicates that the instance
    //! received each of the pieces of data needed for a well-formed claim.
    //!
    //! \return \c true if the claim contains each of the required elements.
    //!
    bool WellFormed() const
    {
        if (m_mining_id.Which() == MiningId::Kind::CPID) {
            return !m_signature.empty() && !m_beacon_txid.IsNull();
        }

        return m_mining_id.Valid()
            && m_signature.empty()
            && m_beacon_txid.IsNull();
    }

    //!
    //! \brief Get the burn fee amount required for the claim.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const
    {
        if (m_mining_id.Which() == MiningId::Kind::CPID) {
            // Flat 0.01 GRC:
            return COIN / 100;
        }

        return 0;
    }

    //!
    //! \brief Sign the claim.
    //!
    //! \param private_key Beacon private key for the CPID in the claim.
    //! \param message     Message to sign the claim for.
    //!
    //! \return \c false if signing fails.
    //!
    bool Sign(CKey& private_key, const ClaimMessage& message);

    //!
    //! \brief Validate authenticity of the claim by verifying the signature.
    //!
    //! \param public_key Beacon public key to verify the signature with.
    //! \param message    Message to verify the claim for.
    //!
    //! \return \c true if the signature check passes for the supplied message.
    //!
    bool VerifySignature(const CPubKey& public_key, const ClaimMessage& message) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_mining_id);

        if (m_mining_id.Which() == MiningId::Kind::CPID) {
            READWRITE(m_beacon_txid);
            READWRITE(m_signature);
        }
    }
}; // MagnitudeClaim

//!
//! \brief Testifies to the eligibility of a participant to create a poll.
//!
class PollEligibilityClaim
{
public:
    //!
    //! \brief Version number of the current format for a serialized claim.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint16_t CURRENT_VERSION = 1;

    //!
    //! \brief The maximum number of unspent outputs that a poll can claim.
    //!
    //! Because nodes verify poll claims upon receipt (to the mempool or in a
    //! block), we limit the number of possible outputs that a poll can claim
    //! to establish ownership of the balance of an address. This reduces the
    //! potential cost to process spam poll transactions.
    //!
    static constexpr size_t MAX_OUTPOINTS = 250;

    uint16_t m_version; //!< Version of the serialized claim format.

    //!
    //! \brief Testifies that a participant owns an address with an unspent
    //! balance great enough to create a poll.
    //!
    AddressClaim m_address_claim;

    //!
    //! \brief Initialize an empty, invalid poll eligibility claim.
    //!
    PollEligibilityClaim() : m_version(CURRENT_VERSION)
    {
    }

    //!
    //! \brief Determine whether an instance represents a complete claim.
    //!
    //! The result of this method call does NOT guarantee that the claim is
    //! valid. The return value of \c true only indicates that the instance
    //! received each of the pieces of data needed for a well-formed claim.
    //!
    //! \return \c true if the claim contains each of the required elements.
    //!
    bool WellFormed() const
    {
        return m_version > 0 && m_version <= CURRENT_VERSION
            && m_address_claim.WellFormed()
            && m_address_claim.m_outpoints.size() <= MAX_OUTPOINTS;
    }

    //!
    //! \brief Get the burn fee amount required for the claim.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const
    {
        // A scaled fee based on the number of claimed outputs:
        return m_address_claim.RequiredBurnAmount();
    }

    //!
    //! \brief Resize the signature fields in the claim so that the wallet can
    //! calculate an accurate fee for the transaction that contains the claim.
    //!
    void ExpandDummySignatures();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_version);
        READWRITE(m_address_claim);
    }
}; // PollEligibilityClaim

//!
//! \brief Testifies to the ownership of a participant's voting weight.
//!
class VoteWeightClaim
{
public:
    //!
    //! \brief Version number of the current format for a serialized claim.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint16_t CURRENT_VERSION = 1;

    uint16_t m_version;               //!< Version of the serialized format.
    MagnitudeClaim m_magnitude_claim; //!< Testifies to CPID ownership.
    BalanceClaim m_balance_claim;     //!< Testifies to balance ownership.

    //!
    //! \brief Initialize an empty, invalid vote weight claim.
    //!
    VoteWeightClaim() : m_version(CURRENT_VERSION)
    {
    }

    //!
    //! \brief Determine whether an instance represents a complete claim.
    //!
    //! The result of this method call does NOT guarantee that the claim is
    //! valid. The return value of \c true only indicates that the instance
    //! received each of the pieces of data needed for a well-formed claim.
    //!
    //! \return \c true if the claim contains each of the required elements.
    //!
    bool WellFormed() const
    {
        return m_version > 0 && m_version <= CURRENT_VERSION
            && m_magnitude_claim.WellFormed()
            && m_balance_claim.WellFormed();
    }

    //!
    //! \brief Get the burn fee amount required for the claim.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const
    {
        return m_magnitude_claim.RequiredBurnAmount()
            + m_balance_claim.RequiredBurnAmount();
    }

    //!
    //! \brief Resize the signature fields in the claim so that the wallet can
    //! calculate an accurate fee for the transaction that contains the claim.
    //!
    void ExpandDummySignatures();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_version);
        READWRITE(m_magnitude_claim);
        READWRITE(m_balance_claim);
    }
}; // VoteWeightClaim

//!
//! \brief Serialize a poll for claim signing and verification.
//!
//! \param poll The poll contract to create the message from.
//! \param tx   The transaction that contains the contract.
//!
//! \return The critical contract data serialized as bytes.
//!
ClaimMessage PackPollMessage(const Poll& poll, const CTransaction& tx);

//!
//! \brief Serialize a vote for claim signing and verification.
//!
//! \param poll The vote contract to create the message from.
//! \param tx   The transaction that contains the contract.
//!
//! \return The critical contract data serialized as bytes.
//!
ClaimMessage PackVoteMessage(const Vote& vote, const CTransaction& tx);
}
