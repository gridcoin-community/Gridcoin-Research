// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_MRC_H
#define GRIDCOIN_MRC_H

#include "amount.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/cpid.h"
#include "gridcoin/superblock.h"
#include "serialize.h"
#include "uint256.h"

#include <optional>

class CPubKey;

namespace GRC {
//!
//! \brief Contains the reward claim context embedded in each generated block.
//!
//! Gridcoin blocks require some auxiliary information about claimed rewards to
//! facilitate and secure the reward protocol. Nodes embed the data represented
//! by a \c Claim instance in generated blocks to provide this context.
//!
class MRC : public IContractPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized reward
    //! claim block.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 1;

    //!
    //! \brief The maximum length of a serialized client version in a claim.
    //!
    static constexpr size_t MAX_VERSION_SIZE = 30;

    //!
    //! \brief The maximum length of a serialized organization value in a claim.
    //!
    static constexpr size_t MAX_ORGANIZATION_SIZE = 50;

    //!
    //! \brief Number of places after the decimal point of serialized magnitude
    //! unit values.
    //!
    static constexpr size_t MAG_UNIT_PLACES = 6;

    //!
    //! \brief Version number of the serialized reward claim format.
    //!
    //! Defaults to the most recent version for a new claim instance.
    //!
    //! Version 1: Parsed from legacy "BoincBlock"-formatted string data stored
    //! in the \c hashBoinc field of a coinbase transaction.
    //!
    //! Version 2: Claim data serializable in binary format. Stored in a block
    //! as the first contract in the coinbase transaction.
    //!
    //! Version 3: Includes the coinstake transaction in the signature.
    //!
    uint32_t m_version = CURRENT_VERSION;

    //!
    //! \brief Indicates whether the claim is for a researcher or investor.
    //!
    //! If the claim declares research rewards, this field shall contain the
    //! external CPID of the researcher. For this case, it must match a CPID
    //! advertised in a verified beacon.
    //!
    MiningId m_mining_id;

    //!
    //! \brief The version string of the wallet software running on the node
    //! that submits the claim.
    //!
    //! Informational. This provides a rough metric of the distribution of
    //! the software versions installed on staking nodes. No protocol rule
    //! exists that depends on this value.
    //!
    //! Max length: 30 bytes. Blocks that contain a claim structure with a
    //! version field longer than 30 characters are rejected.
    //!
    std::string m_client_version;

    //!
    //! \brief A user-customizable field that may contain any arbitrary data.
    //!
    //! Informational. This field is intended to demonstrate an affiliation of
    //! a staked block with the staking party. For example, testnet guidelines
    //! recommend that participants set the organization value to recognizable
    //! identities to facilitate communication when needed.
    //!
    //! Max length: 50 bytes. Blocks that contain a claim structure with an
    //! organization field longer than 50 characters are rejected.
    //!
    std::string m_organization;

    //!
    //! \brief The value of the research rewards claimed by the node in units
    //! of 1/100000000 GRC.
    //!
    //! Contains a value of zero for investor claims.
    //!
    //! Claims do not strictly need to contain the block subsidy or research
    //! subsidy values. Nodes will calculate these values anyway to validate
    //! incoming reward claims and can index those calculated values without
    //! this field. It can be considered informational.
    //!
    CAmount m_research_subsidy;


    //!
    //! \brief The value of the fees charged to the MRC claimant. These will be
    //! subtracted from the research subsidy and distributed to the staker and
    //! the foundation according to protocol rules encapsulated in ComputeMRCFee().
    //!
    CAmount m_fee;

    //!
    //! \brief The researcher magnitude value from the superblock at the time
    //! of the claim.
    //!
    //! Informational. The magnitude value may better enable off-chain services
    //! like explorers to more easily produce historical logs of the researcher
    //! magnitudes over time. The protocol only checks that this magnitude does
    //! not exceed the magnitude in a superblock for the same CPID.
    //!
    //! Previous protocol versions used the magnitude in reward calculations.
    //!
    uint16_t m_magnitude;

    //!
    //! \brief The magnitude ratio of the network at the time of the claim.
    //!
    //! Informational.
    //!
    double m_magnitude_unit;

    //!
    //! \brief Produced by signing the CPID and last block hash with a beacon
    //! public key.
    //!
    //! Nodes verify this signature with the CPID's stored beacon key to prevent
    //! unauthorized claim or replay of the research reward subsidy.
    //!
    std::vector<unsigned char> m_signature;

    //!
    //! \brief Initialize an empty, invalid reward claim object.
    //!
    MRC();

    //!
    //! \brief Initialize an empty, invalid reward claim object of the specified
    //! version.
    //!
    //! \param version Version number of the serialized reward claim format.
    //!
    MRC(uint32_t version);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::MRC;
    }

    //!
    //! \brief Determine whether the object contains a well-formed payload.
    //!
    //! \param action The action declared for the contract that contains the
    //! payload. It may determine how to validate the payload.
    //!
    //! \return \c true if the payload is complete.
    //!
    bool WellFormed(const GRC::ContractAction action) const override
    {
        return WellFormed(); // MRCs like claims do not have contract actions.
    }

    //!
    //! \brief Determine whether the instance represents a complete claim.
    //!
    //! The result of this method call does NOT guarantee that the claim is
    //! valid. The return value of \c true only indicates that the instance
    //! received each of the pieces of data needed for a well-formed claim.
    //!
    //! \return \c true if the claim contains each of the required elements.
    //!
    bool WellFormed() const;

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return ""; // No legacy contract key representation exists.
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return ""; // No legacy contract value representation exists.
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const override
    {
        // Fee for the contract itself is 0.001 GRC.
        return COIN / 100;
    }

    //!
    //! \brief Determine whether the instance represents a claim that includes
    //! accrued research rewards.
    //!
    //! \return \c true if the claim contains a valid CPID.
    //!
    bool HasResearchReward() const;

    //!
    //! \brief ComputeMRCFee
    //! \return Amount of fee in Halfords
    //!
    CAmount ComputeMRCFee();

    //!
    //! \brief Sign an instance that claims research rewards.
    //!
    //! \param private_key     The private key of the beacon to sign the claim
    //! with.
    //! \param last_block_hash Hash of the block that precedes the block that
    //! contains the claim.
    //!
    //! \return \c false if the claim does not contain a valid CPID or if the
    //! signing fails.
    //!
    bool Sign(
        CKey& private_key,
        const uint256& last_block_hash);

    //!
    //! \brief Validate the authenticity of a research reward claim by verifying
    //! the digital signature.
    //!
    //! \param public_key      The public key of the beacon that signed the
    //! claim.
    //! \param last_block_hash Hash of the block that precedes the block that
    //! contains the claim.
    //!
    //! \return \c true if the signature check passes using the supplied key.
    //!
    bool VerifySignature(
        const CPubKey& public_key,
        const uint256& last_block_hash) const;

    //!
    //! \brief Compute a hash of the claim data.
    //!
    //! \return Hash of the data in the claim.
    //!
    uint256 GetHash() const;

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        // Claim contracts do not use the contract action specifier:
        SerializationOp(s, ser_action);
    }

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_version);
        READWRITE(m_mining_id);
        READWRITE(LIMITED_STRING(m_client_version, MAX_VERSION_SIZE));
        READWRITE(LIMITED_STRING(m_organization, MAX_ORGANIZATION_SIZE));

        // Serialize research-related fields only for researcher claims:
        //
        if (m_mining_id.Which() == MiningId::Kind::CPID) {
            READWRITE(m_research_subsidy);
            READWRITE(m_fee);
            READWRITE(m_signature);
        }
    }
}; // MRC
}

#endif // GRIDCOIN_MRC_H
