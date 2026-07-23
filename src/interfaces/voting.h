// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_VOTING_H
#define GRIDCOIN_INTERFACES_VOTING_H

#include "interfaces/marshal.h"

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

//! GUI-facing mirror of GRC::PollFilterFlag (gridcoin/voting/filter.h). A plain
//! (unscoped) enum like the core one, because it is used as a bitmask
//! (flags & ACTIVE). The values match; the VotingManager impl static_asserts
//! them. Lets the voting widgets/models express a filter without the core header.
enum PollFilterFlag {
    NO_FILTER = 0,
    ACTIVE = 1,
    FINISHED = 2,
};

//! GUI-facing mirror of GRC::PollType (gridcoin/voting/fwd.h). Values match; the
//! implementation static_asserts them.
enum class PollType {
    UNKNOWN = 0,
    SURVEY,
    PROJECT,
    DEVELOPMENT,
    GOVERNANCE,
    MARKETING,
    OUTREACH,
    COMMUNITY,
    OUT_OF_BOUND,
};

//! GUI-facing mirror of GRC::PollWeightType (gridcoin/voting/fwd.h), so the
//! pollcard can classify PollTableItem::weight_type (the raw enum value) without
//! the core header. Values match; the implementation static_asserts them.
enum class PollWeightType {
    UNKNOWN = 0,
    MAGNITUDE,
    BALANCE,
    BALANCE_AND_MAGNITUDE,
    CPID_COUNT,
    PARTICIPANT_COUNT,
    OUT_OF_BOUND,
};

//! Poll field-size limits, mirrored from GRC::Poll::MAX_* / MIN_DURATION_DAYS so
//! the GUI's (static) input-validation helpers can read them without
//! gridcoin/voting/poll.h. The VotingManager implementation static_asserts each
//! value against the core constant, so drift is a compile error.
namespace poll_limits {
inline constexpr int MIN_DURATION_DAYS = 7;
inline constexpr int MAX_TITLE_LENGTH = 80;
inline constexpr int MAX_URL_LENGTH = 100;
inline constexpr int MAX_QUESTION_LENGTH = 100;
inline constexpr int MAX_CHOICE_LABEL_LENGTH = 100;
inline constexpr int MAX_ADDITIONAL_FIELD_NAME_LENGTH = 100;
inline constexpr int MAX_ADDITIONAL_FIELD_VALUE_LENGTH = 500;
inline constexpr int MAX_CHOICES = 20;
} // namespace poll_limits

//! Metadata for one poll type (from the core POLL_TYPES / POLL_TYPE_RULES
//! tables), as a value row for the GUI's poll-type picker.
struct PollTypeMeta {
    int type = 0;                       //!< PollType raw value.
    std::string name;                   //!< Translated type name.
    std::string description;            //!< Translated type description.
    int min_duration_days = 0;
    std::vector<std::string> required_fields;
};

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

//! Categorized outcome of a submitPoll/submitVote command. The node returns a
//! status the GUI maps to translated text, so fixed user-facing wording is not
//! baked into the cross-process boundary (mirrors SideStakeEditStatus in 1d-ii).
//! FAILED carries a dynamic message in \p error — e.g. a VotingError raised by
//! the builder — that the GUI shows as-is (these were never translated).
enum class VotingSubmitStatus
{
    OK,               //!< Submitted; \p txid holds the hex transaction id.
    POLL_NOT_FOUND,   //!< No poll matches the supplied txid.
    POLL_LOAD_FAILED, //!< The poll could not be read from disk.
    FAILED,           //!< Other failure; \p error carries the message.
};

//! Outcome of a submitPoll/submitVote command. Kept as a value so the whole
//! command is a single request/response across the boundary.
struct VotingSubmitResult
{
    VotingSubmitStatus status = VotingSubmitStatus::FAILED;
    std::string txid;  //!< Hex transaction id when status == OK.
    std::string error; //!< Dynamic message when status == FAILED (else empty).
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

    //! Poll-type metadata (name/description/min-duration/required fields) for
    //! each valid poll type, in enum order, excluding OUT_OF_BOUND.
    virtual std::vector<PollTypeMeta> getPollTypes() = 0;

    //! Whether poll v3 is active at the current tip (replaces a GUI-side
    //! IsPollV3Enabled(nBestHeight) read); gates the extra project poll fields.
    virtual bool pollV3Enabled() = 0;

    //! Estimated fee to create a poll, in 1/COIN units.
    virtual CAmount estimatePollFee() = 0;

    //! The title of the most recent poll, or "" if none. Used for the overview
    //! "current poll" line.
    virtual std::string currentPollTitle() = 0;

    //! The timestamp (Unix seconds) of the most recent active poll, or 0 if
    //! none. The GUI reads this once at construction so its new-poll
    //! notifications skip polls that already existed at start-up.
    virtual int64_t latestActivePollTime() = 0;

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


// Marshalability pins (interfaces/marshal.h): each DTO above must stay a
// copyable value type the node side can fill and ship across the boundary.
INTERFACES_ASSERT_MARSHALABLE(PollTypeMeta);
INTERFACES_ASSERT_MARSHALABLE(PollChoiceResult);
INTERFACES_ASSERT_MARSHALABLE(PollAdditionalField);
INTERFACES_ASSERT_MARSHALABLE(PollSelfVoteResponse);
INTERFACES_ASSERT_MARSHALABLE(PollTableItem);
INTERFACES_ASSERT_MARSHALABLE(PollSubmission);
INTERFACES_ASSERT_MARSHALABLE(VotingSubmitResult);
} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_VOTING_H
