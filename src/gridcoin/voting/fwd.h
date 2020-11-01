// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"

#include <boost/optional/optional_fwd.hpp>

namespace GRC {

class Poll;
class PollReference;
class PollResult;
class Vote;

using PollOption = boost::optional<Poll>;
using PollResultOption = boost::optional<PollResult>;

//!
//! \brief The unspent amount that a poll creator must hold in an address.
//!
constexpr CAmount POLL_REQUIRED_BALANCE = 100000 * COIN;

//!
//! \brief The maximum number of choices that a poll can contain.
//!
constexpr size_t POLL_MAX_CHOICES_SIZE = 20;

//!
//! \brief Describes the poll types.
//!
//! CONSENSUS: Do not remove or reorder items in this enumeration except for
//! OUT_OF_BOUND which must remain at the end.
//!
//! TODO: we may add additional poll types with specialized requirements, data,
//! or behavior such as project whitelist polls.
//!
enum class PollType
{
    UNKNOWN,       //!< An invalid, non-standard, or empty poll type.
    SURVEY,        //!< For casual, opinion, and legacy polls.
    OUT_OF_BOUND,  //!< Marker value for the end of the valid range.
};

//!
//! \brief Describes the methods used to weigh votes for a poll.
//!
//! CONSENSUS: Do not remove or reorder items in this enumeration except for
//! OUT_OF_BOUND which must remain at the end.
//!
enum class PollWeightType
{
    UNKNOWN,               //!< An invalid, non-standard, or empty weight type.
    MAGNITUDE,             //!< Deprecated: Weight based on magnitude of a CPID.
    BALANCE,               //!< Weight based on unspent GRC of a voting wallet.
    BALANCE_AND_MAGNITUDE, //!< Weight based on both magnitude and balance.
    CPID_COUNT,            //!< Deprecated: each CPID receives the same weight.
    PARTICIPANT_COUNT,     //!< Deprecated: each vote receives the same weight.
    OUT_OF_BOUND,          //!< Marker value for the end of the valid range.
};

//!
//! \brief Describes the methods for choosing poll answers.
//!
//! CONSENSUS: Do not remove or reorder items in this enumeration except for
//! OUT_OF_BOUND which must remain at the end.
//!
enum class PollResponseType
{
    UNKNOWN,         //!< An invalid, non-standard, or empty response type.
    YES_NO_ABSTAIN,  //!< Standard yes/no/abstain single-choice answers only.
    SINGLE_CHOICE,   //!< All vote weight applied to one answer.
    MULTIPLE_CHOICE, //!< Vote weight split equally between each chosen answer.
    OUT_OF_BOUND,    //!< Marker value for the end of the valid range.
};

//!
//! \brief Thrown when constructing a poll or vote with invalid data.
//!
class VotingError : public std::runtime_error
{
public:
    explicit VotingError(const std::string& what) : std::runtime_error(what)
    {
    }
};

//!
//! \brief Get the title of the most recent poll.
//!
//! \return The first 80 characters of the most recent poll title.
//!
std::string GetCurrentPollTitle();
}
