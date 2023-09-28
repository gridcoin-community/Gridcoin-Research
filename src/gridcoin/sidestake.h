// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SIDESTAKE_H
#define GRIDCOIN_SIDESTAKE_H

#include "base58.h"
#include "gridcoin/support/enumbytes.h"
#include "serialize.h"

namespace GRC {

enum class SideStakeStatus
{
    UNKNOWN,
    ACTIVE,         //!< A user specified sidestake that is active
    INACTIVE,       //!< A user specified sidestake that is inactive
    DELETED,        //!< A mandatory sidestake that has been deleted by contract
    MANDATORY,      //!< An active mandatory sidetake by contract
    OUT_OF_BOUND
};

class SideStake
{
    using Status = EnumByte<SideStakeStatus>;

    CBitcoinAddress m_address;

    double m_allocation;

    int64_t m_timestamp;        //!< Time of the sidestake contract transaction.

    uint256 m_hash;             //!< The hash of the transaction that contains a mandatory sidestake.

    uint256 m_previous_hash;    //!< The m_hash of the previous mandatory sidestake allocation with the same address.

    Status m_status;            //!< The status of the sidestake. It is of type int instead of enum for serialization.

    //!
    //! \brief Initialize an empty, invalid sidestake instance.
    //!
    SideStake();

    //!
    //! \brief Initialize a sidestake instance with the provided address and allocation. This is used to construct a user
    //! specified sidestake.
    //!
    //! \param address
    //! \param allocation
    //!
    SideStake(CBitcoinAddress address, double allocation);

    //!
    //! \brief Initial a sidestake instance with the provided parameters. This form is normally used to construct a
    //! mandatory sidestake from a contract.
    //!
    //! \param address
    //! \param allocation
    //! \param timestamp
    //! \param hash
    //!
    SideStake(CBitcoinAddress address, double allocation, int64_t timestamp, uint256 hash);

    //!
    //! \brief Determine whether a sidestake contains each of the required elements.
    //! \return true if the sidestake is well-formed.
    //!
    bool WellFormed() const;

    //!
    //! \brief Provides the sidestake address and status (value) as a pair of strings.
    //! \return std::pair of strings
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

           //!
           //! \brief Returns the string representation of the current sidestake status
           //!
           //! \return Translated string representation of sidestake status
           //!
    std::string StatusToString() const;

           //!
           //! \brief Returns the translated or untranslated string of the input sidestake status
           //!
           //! \param status. SideStake status
           //! \param translated. True for translated, false for not translated. Defaults to true.
           //!
           //! \return SideStake status string.
           //!
    std::string StatusToString(const SideStakeStatus& status, const bool& translated = true) const;

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!

    bool operator==(SideStake b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!

    bool operator!=(SideStake b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_address);
        READWRITE(m_allocation);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a sidestake
//!
typedef std::shared_ptr<SideStake> SideStake_ptr;

//!
//! \brief A type that either points to some sidestake or does not.
//!
typedef const SideStake_ptr SideStakeOption;


} // namespace GRC

#endif // GRIDCOIN_SIDESTAKE_H
