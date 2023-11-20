// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_RESULT_H
#define GRIDCOIN_VOTING_RESULT_H

#include "wallet/ismine.h"
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
        isminetype m_ismine;         //!< True if the vote is from the wallet holder.

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
        //! \brief User copy constructor.
        //!
        //! \param original_votedetail
        //!
        VoteDetail(const VoteDetail& original_votedetail);

        //!
        //! \brief Determine whether a vote contributes no weight.
        //!
        //! \return \c true if the vote claims no balance or magnitude weight.
        //!
        bool Empty() const;

        VoteDetail& operator=(const VoteDetail& b);
    };

    const Poll m_poll;                            //!< The poll associated with the result.
    Weight m_total_weight;                        //!< Aggregate weight of all the votes submitted.
    size_t m_invalid_votes;                       //!< Number of votes that failed validation.
    std::vector<Cpid> m_pools_voted;              //!< Cpids of pools that actually voted.
    std::optional<CAmount> m_active_vote_weight;  //!< Active vote weight of poll.
    std::optional<double> m_vote_percent_avw;     //!< Vote weight percent of AVW.
    std::optional<bool> m_poll_results_validated; //!< Whether the poll's AVW is >= the minimum AVW for the poll.
    bool m_finished;                              //!< Whether the poll finished as of this result.
    bool m_self_voted;                            //!< Whether the wallet holder voted.
    VoteDetail m_self_vote_detail;                //!< The vote detail from the wallet holder's (last) vote

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

#endif // GRIDCOIN_VOTING_RESULT_H
