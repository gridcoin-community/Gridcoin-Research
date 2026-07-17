// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_VOTING_H
#define GRIDCOIN_INTERFACES_VOTING_H

#include "amount.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace interfaces {

class Handler;

//! One choice of a poll together with its tallied result, as pointer-free value
//! data (Phase 1d-iii). The label is resolved on the node side; weight is
//! already scaled to whole GRC (divided by COIN), matching what the GUI renders.
struct PollChoiceResult
{
    std::string label;
    double votes = 0.0;
    uint64_t weight = 0;
};

//! One additional (custom) field of a poll as value data.
struct PollAdditionalField
{
    std::string name;
    std::string value;
    bool required = false;
};

//! The wallet holder's own vote, flattened to the offsets and weights the GUI
//! displays (the only part of the core VoteDetail the GUI reads). Weight is in
//! the raw 1/COIN units the core resolved; the GUI scales it.
struct PollSelfVoteResponse
{
    uint8_t choice_offset = 0;
    uint64_t weight = 0;
};

//! A single poll and its tallied result as a self-contained value row for the
//! GUI table (Phase 1d-iii). Every field the GUI renders or sorts on is
//! precomputed on the node side — no core types (Poll, PollResult, PollReference,
//! CBlockIndex) cross into the GUI. String forms (type, weight type, response
//! type, winning answer) are resolved here; times are Unix seconds; weights that
//! the GUI shows in whole GRC are already divided by COIN.
struct PollTableItem
{
    std::string txid;                //!< Poll transaction id (hex).
    uint32_t payload_version = 0;    //!< Contract payload version.
    std::string type_str;            //!< PollTypeToString().
    std::string title;
    std::string question;
    std::string url;
    int64_t start_time = 0;          //!< Poll start, Unix seconds.
    int64_t expiration = 0;          //!< Poll expiration, Unix seconds.
    uint32_t duration_days = 0;
    int weight_type = 0;             //!< PollWeightType raw value.
    std::string weight_type_str;
    std::string response_type;
    std::string top_answer;          //!< Winning choice label ("" if no votes).
    uint32_t total_votes = 0;
    uint64_t total_weight = 0;       //!< Total vote weight, whole GRC (/COIN).
    uint64_t active_weight = 0;      //!< Active vote weight, whole GRC (/COIN).
    double vote_percent_avw = 0.0;
    std::optional<bool> validated;   //!< AVW >= minimum (v3+ only), else nullopt.
    bool finished = false;
    bool multiple_choice = false;
    std::vector<PollAdditionalField> additional_fields;
    std::vector<PollChoiceResult> choices;
    bool self_voted = false;
    std::vector<PollSelfVoteResponse> self_vote_responses;
};

//! Input for submitPoll: the poll's user-entered fields as value data. The node
//! side chooses the payload version and constrains the type by it (pre-v3 polls
//! are all SURVEY), so the GUI does not read chain state to build a poll.
struct PollSubmission
{
    int type = 0;                    //!< PollType raw value (constrained node-side).
    std::string title;
    int duration_days = 0;
    std::string question;
    std::string url;
    int weight_type = 0;
    int response_type = 0;
    std::vector<std::string> choices;
    std::vector<PollAdditionalField> additional_fields;
};

//! Outcome of a submitPoll/submitVote command. On success \p txid is the hex
//! transaction id; on failure \p error carries a human-readable message (the
//! same text the former VotingModel returned). Kept as a value so the whole
//! command is a single request/response across the boundary.
struct VotingSubmitResult
{
    bool ok = false;
    std::string txid;
    std::string error;
};

//! Called when the node connects a new poll (uiInterface.NewPollReceived). The
//! payload is the poll's timestamp, matching the former core signal; the GUI
//! debounces against its last-seen poll time.
using NewPollReceivedFn = std::function<void(int64_t poll_time)>;

//! Called when the node connects (or, during a reorg, disconnects) a vote
//! (uiInterface.NewVoteReceived). The payload is the affected poll's txid (hex).
using NewVoteReceivedFn = std::function<void(std::string poll_txid)>;

//! The voting boundary (Phase 1d-iii). Hands the GUI value-type poll rows and
//! command-style submissions, so no GUI code holds a GRC::PollResult, walks the
//! poll registry, or calls the vote/poll builders directly. The table build runs
//! the core PollResultCache (tally memoization, pinned-tip consistency and the
//! reorg-retry all live in the node); the GUI keeps calling it off its own worker
//! thread and just renders the returned snapshot.
class VotingManager
{
public:
    virtual ~VotingManager() = default;

    //! The poll table for the given filter (GRC::PollFilterFlag bits), tallied
    //! against a single pinned tip. Potentially slow (a large poll's tally), so
    //! callers must invoke it off the UI thread. Thread-safe.
    virtual std::vector<PollTableItem> buildPollTable(int filter_flags) = 0;

    //! Estimated fee to create a poll, in 1/COIN units.
    virtual CAmount estimatePollFee() = 0;

    //! The title of the most recent poll, or "" if none. Used for the overview
    //! "current poll" line.
    virtual std::string currentPollTitle() = 0;

    //! Build, sign and broadcast a poll contract. The wallet must already be
    //! unlocked by the caller (the GUI arranges the modal unlock through the
    //! wallet interface); a locked wallet yields a failure result rather than a
    //! prompt from the node.
    virtual VotingSubmitResult submitPoll(const PollSubmission& poll) = 0;

    //! Build, sign and broadcast a vote for \p poll_txid (hex) selecting the
    //! given choice offsets. Same wallet-unlock contract as submitPoll.
    virtual VotingSubmitResult submitVote(const std::string& poll_txid,
                                          const std::vector<uint8_t>& choice_offsets) = 0;

    //! Subscribe to new-poll notifications (uiInterface.NewPollReceived).
    virtual std::unique_ptr<Handler> handleNewPollReceived(NewPollReceivedFn fn) = 0;

    //! Subscribe to new-vote notifications (uiInterface.NewVoteReceived).
    virtual std::unique_ptr<Handler> handleNewVoteReceived(NewVoteReceivedFn fn) = 0;
};

//! Return an in-process VotingManager over the global poll registry, the poll
//! result cache and the node's single wallet.
std::unique_ptr<VotingManager> MakeVotingManager();

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_VOTING_H
