// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/contract/payload.h"

#include <string>

namespace GRC {
//!
//! \brief An arbitrary, user-supplied message attached to a transaction.
//!
//! This contract type does not have any associated protocol behavior. It
//! replaces the message feature that stored a transaction message in the
//! legacy \c hashBoinc field of a transaction object. It allows users to
//! affix arbitrary data to a transaction.
//!
class TxMessage : public IContractPayload
{
public:
    std::string m_message; //!< The content of the transaction message.

    //!
    //! \brief Initialize an empty, invalid transaction message.
    //!
    TxMessage()
    {
    }

    //!
    //! \brief Initialize a transaction message from the supplied string.
    //!
    //! \param message An arbitrary, user-supplied string.
    //!
    TxMessage(std::string message) : m_message(std::move(message))
    {
    }

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::MESSAGE;
    }

    //!
    //! \brief Determine whether the object contains a well-formed payload.
    //!
    //! \param action The action declared for the contract that contains the
    //! payload. It may determine how to validate the payload.
    //!
    //! \return \c true if the payload is complete.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        return !m_message.empty();
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return std::string();
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return m_message;
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    int64_t RequiredBurnAmount() const override
    {
        // Flat rate up to first KB:
        if (m_message.size() <= 1000) {
            return 0.001 * COIN;
        }

        // 0.001 GRC per KB:
        return m_message.size() * 100;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const GRC::ContractAction action)
    {
        READWRITE(m_message);
    }
}; // TxMessage
}
