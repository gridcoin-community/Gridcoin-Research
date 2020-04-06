#pragma once

#include "neuralnet/cpid.h"
#include "neuralnet/superblock.h"
#include "serialize.h"
#include "uint256.h"

#include <boost/optional.hpp>

class CPubKey;

namespace NN {
//!
//! \brief Contains the reward claim context embedded in each generated block.
//!
//! Gridcoin blocks require some auxiliary information about claimed rewards to
//! facilitate and secure the reward protocol. Nodes embed the data represented
//! by a \c Claim instance in generated blocks to provide this context.
//!
struct Claim
{
    //!
    //! \brief Version number of the current format for a serialized reward
    //! claim block.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

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
    //! Version 2: Claim data serializable in binary format. Stored in a block's
    //! \c m_claim field to enable submission of larger superblocks.
    //!
    uint32_t m_version = CURRENT_VERSION;

    //!
    //! \brief Indicates whether the claim is for a researcher or investor.
    //!
    //! If the claim declares research rewards, this field shall contain the
    //! external CPID of the researcher. For this case, it must match a CPID
    //! advertised in a verified beacon.
    //!
    MiningId m_mining_id; // MiningCPID::cpid

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
    std::string m_client_version; // MiningCPID::clientversion

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
    std::string m_organization; // MiningCPID::Organization

    //!
    //! \brief The value minted for generating the new block in units of
    //! 1/100000000 GRC.
    //!
    //! Below the switch to constant block rewards, this field contains the
    //! amount of accrued interest claimed by the staking node. It contains
    //! the constant block reward rate after the switch.
    //!
    //! Claims do not strictly need to contain the block subsidy or research
    //! subsidy values. Nodes will calculate these values anyway to validate
    //! incoming reward claims and can index those calculated values without
    //! this field. It can be considered informational.
    //!
    int64_t m_block_subsidy; // MiningCPID::InterestSubsidy

    //!
    //! \brief Hash of the block below the block containing this claim.
    //!
    //! Nodes check that this hash matches the hash of block that preceeds the
    //! block that contains the claim. This hash is signed along with the CPID
    //! to prevent replay of the research reward subsidy.
    //!
    //! The significance of the last block is embedded into the claim signature
    //! for researchers so we can consider this field informational.
    //!
    //! TODO: Remove this field after the switch to block version 11.
    //!
    uint256 m_last_block_hash; // MiningCPID::lastblockhash

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
    int64_t m_research_subsidy; // MiningCPID::ResearchSubsidy

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
    uint16_t m_magnitude; // MiningCPID::Magnitude

    //!
    //! \brief The magnitude ratio of the network at the time of the claim.
    //!
    //! Informational.
    //!
    double m_magnitude_unit; // MiningCPID::ResearchMagnitudeUnit

    //!
    //! \brief Produced by signing the CPID and last block hash with a beacon
    //! public key.
    //!
    //! Nodes verify this signature with the CPID's stored beacon key to prevent
    //! unauthorized claim or replay of the research reward subsidy.
    //!
    std::vector<unsigned char> m_signature; // MiningCPID::BoincSignature

    //!
    //! \brief Hash of the superblock to vote for.
    //!
    //! When a superblock is due for a day, nodes will vote for a particular
    //! superblock contract by submitting in this field the contract hash of
    //! the pending superblock that the node caches locally.
    //!
    QuorumHash m_quorum_hash; // MiningCPID::NeuralHash

    //!
    //! \brief The default wallet address of the node submitting the claim.
    //!
    //! This address matches the address advertised in the beacon for the CPID
    //! owned by the node. It will determine whether a node may participate in
    //! the superblock quorum for a particular day.
    //!
    std::string m_quorum_address; // MiningCPID::GRCAddress

    //!
    //! \brief The complete superblock data when the node submitting the claim
    //! also publishes the daily superblock in the generated block.
    //!
    //! Must be accompanied by a valid superblock hash in the \c m_quorum_hash
    //! field.
    //!
    Superblock m_superblock; // MiningCPID::superblock

    //!
    //! \brief Initialize an empty, invalid reward claim object.
    //!
    Claim();

    //!
    //! \brief Initialize an empty, invalid reward claim object of the specified
    //! version.
    //!
    //! \param version Version number of the serialized reward claim format.
    //!
    Claim(uint32_t version);

    //!
    //! \brief Parse a claim object from a legacy, delimited string (hashBoinc).
    //!
    //! \param claim         Legacy "BoincBlock"-formatted claim string data.
    //! \param block_version Format version of the block that contains the claim
    //! to parse.
    //!
    //! \return A new claim instance that contains the parsed reward context.
    //!
    static Claim Parse(const std::string& claim, int block_version);

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
    //! \brief Determine whether the instance represents a claim that includes
    //! accrued research rewards.
    //!
    //! \return \c true if the claim contains a valid CPID.
    //!
    bool HasResearchReward() const;

    //!
    //! \brief Determine whether the claim instance includes a superblock.
    //!
    //! \return \c true if the claim contains a valid superblock object.
    //!
    bool ContainsSuperblock() const;

    //!
    //! \brief Get the sum of the claimed block and research rewards.
    //!
    //! \return The sum of the block subsidy and research subsidy declared in
    //! the claim.
    //!
    int64_t TotalSubsidy() const;

    //!
    //! \brief Sign an instance that claims research rewards.
    //!
    //! \param private_key     The private key of the beacon to sign the claim
    //! with.
    //! \param last_block_hash Hash of the block that preceeds the block that
    //! contains the claim.
    //!
    //! \return \c false if the claim does not contain a valid CPID or if the
    //! signing fails.
    //!
    bool Sign(CKey& private_key, const uint256& last_block_hash);

    //!
    //! \brief Validate the authenticity of a research reward claim by verifying
    //! the digital signature.
    //!
    //! \param public_key      The public key of the beacon that signed the
    //! claim.
    //! \param last_block_hash Hash of the block that preceeds the block that
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

    //!
    //! \brief Get the legacy string representation of the claim.
    //!
    //! CONSENSUS: Although this method produces a legacy string compatible
    //! with older protocols, it does not guarantee that the string matches
    //! exactly to legacy input string versions imported by Claim::Parse().
    //! Use this method to produce a new claim context for generated legacy
    //! blocks or for informational output. Do not reproduce existing claim
    //! data with this routine if it will be retransmitted to other nodes.
    //!
    //! \param block_version Determines the number of subsidy places.
    //!
    //! \return A "BoincBlock"-formatted string that contains the claim data.
    //!
    std::string ToString(const int block_version) const;

    //
    // Serialize and deserialize the claim in binary format instead of parsing
    // and formatting the legacy delimited string representation.
    //
    // For Claim::m_version >= 2.
    //
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_version);
        READWRITE(m_mining_id);
        READWRITE(LIMITED_STRING(m_client_version, MAX_VERSION_SIZE));
        READWRITE(LIMITED_STRING(m_organization, MAX_ORGANIZATION_SIZE));

        READWRITE(m_block_subsidy);

        // Serialize research-related fields only for researcher claims:
        //
        if (m_mining_id.Which() == MiningId::Kind::CPID) {
            READWRITE(m_research_subsidy);
            READWRITE(m_signature);
        }

        READWRITE(m_quorum_hash);

        if (!(s.GetType() & (SER_GETHASH|SER_SKIPSUPERBLOCK))
            && m_quorum_hash.Valid())
        {
            READWRITE(m_superblock);
        }
    }
}; // Claim

//!
//! \brief An optional type that either contains some claim object or does not.
//!
typedef boost::optional<Claim> ClaimOption;

//!
//! \brief Check the authenticity of a research reward claim by verifying the
//! signature against a matching beacon public key.
//!
//! \brief claim           Contains the claim data to verify.
//! \param last_block_hash Hash of the block that preceeds the block that
//! contains the claim.
//!
//! \return \c true if the signature check passes.
//!
bool VerifyClaim(const Claim& claim, const uint256& last_block_hash);
}
