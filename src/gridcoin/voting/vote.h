// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/voting/claims.h"
#include "serialize.h"
#include "uint256.h"

namespace GRC {
//!
//! \brief A contract that contains a response to a poll.
//!
class Vote : public IContractPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized vote.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 1;

    //!
    //! \brief Version number of the serialized vote format.
    //!
    //! Defaults to the most recent version for a new vote instance.
    //!
    //! Version 1: Poll data serializable in binary format. Unlike the other
    //! contract types, legacy vote contracts cannot be transformed into the
    //! binary format. A separate class represents legacy votes.
    //!
    uint32_t m_version;

    //!
    //! \brief Hash of the transaction that contains the poll that the vote
    //! expresses a response for.
    //!
    //! TODO: If transactions allow more than one contract in the future, this
    //! class will need the offsets for the poll contracts in the transactions
    //! as well to identify the associated poll for the vote.
    //!
    uint256 m_poll_txid;

    //!
    //! \brief The set of responses to the poll.
    //!
    //! This collection contains offset values that correspond to the offsets
    //! of the choices in the associated poll.
    //!
    std::vector<uint8_t> m_responses;

    //!
    //! \brief Attests to the weight of the vote.
    //!
    //! Nodes inspect the data in the claim to determine the weight of a vote
    //! and to validate ownership of the assets that supply that weight.
    //!
    VoteWeightClaim m_claim;

    //!
    //! \brief Initialize an empty, invalid vote contract.
    //!
    Vote() : m_version(CURRENT_VERSION)
    {
    }

    //!
    //! \brief Initialize a vote with data for a contract in a transaction.
    //!
    //! \param version   Version of the serialized vote format.
    //! \param poll_txid Hash of the transaction that contains the poll.
    //! \param responses The set of responses to the poll.
    //! \param claim     Attests to the weight of the vote.
    //!
    Vote(
        const uint32_t version,
        const uint256 poll_txid,
        std::vector<uint8_t> responses,
        VoteWeightClaim claim);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::VOTE;
    }

    //!
    //! \brief Determine whether the instance represents a complete vote.
    //!
    //! \return \c true if the vote contains each of the required elements.
    //!
    bool WellFormed() const
    {
        return WellFormed(ContractAction::UNKNOWN);
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        return m_version > 0 && m_version <= CURRENT_VERSION
            && !m_responses.empty()
            && m_claim.WellFormed();
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        // Binary format votes cannot be transformed into the legacy, XML-like
        // string contracts.
        //
        return std::string();
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        // Binary format votes cannot be transformed into the legacy, XML-like
        // string contracts.
        //
        return std::string();
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const override
    {
        // 0.01 GRC for the vote contract + the scaled claim fee:
        return (COIN / 100) + m_claim.RequiredBurnAmount();
    }

    //!
    //! \brief Determine whether the specified poll choice already exists.
    //!
    //! \return \c true if another response contains a matching offset.
    //!
    bool ResponseExists(const uint8_t offset) const;

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_poll_txid);
        READWRITE(m_responses);
        READWRITE(m_claim);
    }
}; // Vote

//!
//! \brief A contract that contains a legacy response to a poll.
//!
//! Unlike the other contract types, legacy vote contracts cannot be transformed
//! into the binary format. This class contains vote data parsed from the legacy
//! XML-like string contracts.
//!
class LegacyVote : public IContractPayload
{
public:
    std::string m_key;       //!< Legacy vote contract key. Serves as a vote ID.
    MiningId m_mining_id;    //!< CPID parsed from the contract body.
    double m_amount;         //!< Balance claim parsed from the contract body.
    double m_magnitude;      //!< Magnitude claim parsed from the contract body.
    std::string m_responses; //!< Semicolon-delimited poll choice labels.

    //!
    //! \brief Initialize a vote from data in a legacy contract.
    //!
    //! \param key        Legacy vote contract key. Serves as a vote ID.
    //! \param mining_id  CPID parsed from the contract body.
    //! \param amount     Balance claim parsed from the contract body.
    //! \param magnitude  Magnitude claim parsed from the contract body.
    //! \param responses  Semicolon-delimited poll choice labels.
    //!
    LegacyVote(
        std::string key,
        MiningId mining_id,
        double amount,
        double magnitude,
        std::string responses);

    //!
    //! \brief Parses a vote object from a legacy, XML-like string contract.
    //!
    //! \param key   Contract key string that represents a vote ID.
    //! \param value Vote data in the legacy, XML-like string contract format.
    //!
    //! \return A legacy vote object parsed from the supplied strings.
    //!
    static LegacyVote Parse(const std::string& key, const std::string& value);

    //!
    //! \brief Parse the selected responses from the legacy contract answers.
    //!
    //! This method is used to transform responses in legacy vote to build a
    //! poll result.
    //!
    //! \param choice_map Associates the poll choice labels with the offsets
    //! of the choices in the poll.
    //!
    //! \return A set of offsets that match the choices in the poll.
    //!
    std::vector<std::pair<uint8_t, uint64_t>>
    ParseResponses(const std::map<std::string, uint8_t>& choice_map) const;

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::VOTE;
    }

    //!
    //! \brief Determine whether the instance represents a complete vote.
    //!
    //! \return \c true if the vote contains each of the required elements.
    //!
    bool WellFormed() const
    {
        return !m_key.empty()
            && m_amount >= 0
            && m_magnitude >= 0
            && !m_responses.empty();
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        return true; // Legacy votes have no impact on consensus.
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return m_key;
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return std::string(); // Legacy serialization removed
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const override
    {
        // Prevent users from sending this contract manually:
        return MAX_MONEY;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        // No binary serialization format exists for legacy votes.
    }
}; // LegacyVote
}
