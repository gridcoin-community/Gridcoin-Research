// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/cpid.h"
#include "gridcoin/magnitude.h"
#include "gridcoin/voting/fwd.h"
#include "gridcoin/voting/poll.h"

#include <vector>

namespace GRC {
//!
//! \brief Contains the results of a poll.
//!
class PollResult
{
public:
    //!
    //! \brief Type used to represent voting weight.
    //!
    using Weight = uint64_t;

    //!
    //! \brief Aggregate voting weight for each poll choice.
    //!
    class ResponseDetail
    {
    public:
        Weight m_weight; //!< Total vote weight for the response.
        double m_votes;  //!< Number of votes for the response.

        //!
        //! \brief Initialize an empty response detail object.
        //!
        ResponseDetail();

        //!
        //! \brief Determine whether a response compares less than another by
        //! total voting weight.
        //!
        bool operator<(const ResponseDetail& other) const
        {
            return m_weight < other.m_weight;
        }
    };

    //!
    //! \brief Vote context resolved from a vote contract.
    //!
    class VoteDetail
    {
    public:
        Weight m_amount;       //!< Total balance resolved for the vote.
        MiningId m_mining_id;  //!< CPID for the vote, if any.
        Magnitude m_magnitude; //!< Magnitude resolved for the vote.

        //!
        //! \brief The selected poll choice offsets and the associated voting
        //! weight resolved for each choice.
        //!
        std::vector<std::pair<uint8_t, Weight>> m_responses;

        //!
        //! \brief Initialize an empty vote detail object.
        //!
        VoteDetail();

        //!
        //! \brief Determine whether a vote contributes no weight.
        //!
        //! \return \c true if the vote claims no balance or magnitude weight.
        //!
        bool Empty() const;
    };

    const Poll m_poll;      //!< The poll associated with the result.
    Weight m_total_weight;  //!< Aggregate weight of all the votes submitted.
    size_t m_invalid_votes; //!< Number of votes that failed validation.

    //!
    //! \brief The aggregated voting weight tallied for each poll choice.
    //!
    //! The offset of each response entry matches the offset of the choice in
    //! the poll.
    //!
    std::vector<ResponseDetail> m_responses;

    //!
    //! \brief Vote context resolved for each vote contract submitted for the
    //! poll.
    //!
    std::vector<VoteDetail> m_votes;

    //!
    //! \brief Initialize a result container for the supplied poll.
    //!
    //! \param The poll to associate with the result.
    //!
    PollResult(Poll poll);

    //!
    //! \brief Generate the result for the specified poll.
    //!
    //! \param poll_ref Refers to the poll to generate the result for.
    //!
    //! \return An object that contains the calculated result for the poll or
    //! no result if an error occurred.
    //!
    static PollResultOption BuildFor(const PollReference& poll_ref);

    //!
    //! \brief Get the offset of the poll choice with the most votes.
    //!
    //! \return Offset of the choice as it exists in the original poll.
    //!
    size_t Winner() const;

    //!
    //! \brief Get the label of the poll choice with the most votes.
    //!
    //! \return Label of the choice as it exists in the original poll.
    //!
    const std::string& WinnerLabel() const;

    //!
    //! \brief Add a resolved vote to the poll result.
    //!
    //! \param detail Vote context resolved from a vote contract.
    //!
    void TallyVote(VoteDetail detail);
}; // PollResult
}
