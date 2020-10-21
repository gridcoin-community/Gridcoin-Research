// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/contract/payload.h"
#include "gridcoin/voting/claims.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/vote.h"
#include "serialize.h"

namespace GRC {
//!
//! \brief The body of a poll contract submitted in a transaction.
//!
class PollPayload : public IContractPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized poll.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

    //!
    //! \brief Version number of the serialized poll format.
    //!
    //! Defaults to the most recent version for a new poll instance.
    //!
    //! Version 1: Legacy XML-like string of fields parsed from a contract.
    //!
    //! Version 2: Poll data serializable in binary format.
    //!
    uint32_t m_version;

    Poll m_poll;                  //!< The body of the poll.
    PollEligibilityClaim m_claim; //!< Used to verify the poll author's balance.

    //!
    //! \brief Initialize an empty, invalid poll payload.
    //!
    PollPayload()
        : m_version(CURRENT_VERSION)
    {
    }

    //!
    //! \brief Initialize a poll payload for submission in a transaction.
    //!
    //! \param poll  The body of the poll.
    //! \param claim Testifies that the poll author owns the required balance.
    //!
    PollPayload(Poll poll, PollEligibilityClaim claim)
        : PollPayload(CURRENT_VERSION, std::move(poll), std::move(claim))
    {
    }

    //!
    //! \brief Initialize a poll from data in a legacy contract.
    //!
    //! \param poll The body of the poll.
    //!
    PollPayload(Poll poll)
        : PollPayload(1, std::move(poll), {})
    {
    }

    //!
    //! \brief Initialize a poll from data in a contract.
    //!
    //! \param version Version number of the serialized poll format.
    //! \param poll    The body of the poll.
    //! \param claim   Testifies that the poll author owns the required balance.
    //!
    PollPayload(const uint32_t version, Poll poll, PollEligibilityClaim claim)
        : m_version(version)
        , m_poll(std::move(poll))
        , m_claim(std::move(claim))
    {
    }

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::POLL;
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
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
            && m_poll.WellFormed(m_version)
            && m_claim.WellFormed();
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return m_poll.m_title;
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
        // 50 GRC + a scaled fee based on the number of claimed outputs:
        return (50 * COIN) + m_claim.RequiredBurnAmount();
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_poll);
        READWRITE(m_claim);
    }
}; // PollPayload
}
