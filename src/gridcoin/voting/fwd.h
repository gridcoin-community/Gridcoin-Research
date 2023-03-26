// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_FWD_H
#define GRIDCOIN_VOTING_FWD_H

#include "amount.h"
#include <optional>
#include <stdexcept>
#include <string>

namespace GRC {

class Poll;
class PollReference;
class PollResult;
class Vote;

using PollOption = std::optional<Poll>;
using PollResultOption = std::optional<PollResult>;

//!
//! \brief The unspent amount that a poll creator must hold in an address.
//!
constexpr CAmount POLL_REQUIRED_BALANCE = 100000 * COIN;

//!
//! \brief The maximum number of choices that a poll can contain.
//!
constexpr size_t POLL_MAX_CHOICES_SIZE = 20;

//!
//! \brief The maximum number of additional fields that a poll can contain.
//!
constexpr size_t POLL_MAX_ADDITIONAL_FIELDS_SIZE = 16;

//!
//! \brief Describes the poll types.
//!
//! CONSENSUS: Do not remove or reorder items in this enumeration except for
//! OUT_OF_BOUND which must remain at the end.
//!
enum class PollType
{
    UNKNOWN,       //!< An invalid, non-standard, or empty poll type.
    SURVEY,        //!< For casual, opinion, and legacy polls.
    PROJECT,       //!< Propose additions or removals of projects for research rewards eligibility.
    DEVELOPMENT,   //!< Propose a change to Gridcoin at the protocol level.
    GOVERNANCE,    //!< Proposals related to Gridcoin management like poll requirements or funding.
    MARKETING,     //!< Propose marketing initiatives like ad campaigns.
    OUTREACH,      //!< For polls about community representation, public relations, and communications.
    COMMUNITY,     //!< For other initiatives related to the Gridcoin community not included in the above.
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

#endif // GRIDCOIN_VOTING_FWD_H
