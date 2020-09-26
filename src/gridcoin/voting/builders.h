// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/voting/fwd.h"

#include <memory>

class CWallet;
class CWalletTx;
class uint256;

namespace GRC {
//!
//! \brief Constructs poll contract objects from user input.
//!
//! This is a move-only type. It provides a fluent interface that applies
//! validation for each of the poll component builder methods.
//!
class PollBuilder
{
public:
    //!
    //! \brief Initialize a new poll builder.
    //!
    PollBuilder();

    //!
    //! \brief Move constructor.
    //!
    PollBuilder(PollBuilder&& builder);

    //!
    //! \brief Destructor.
    //!
    ~PollBuilder();

    //!
    //! \brief Move assignment operator.
    //!
    PollBuilder& operator=(PollBuilder&& builder);

    //!
    //! \brief Set the type of the poll.
    //!
    //! \throws VotingError If the supplied poll type is not valid.
    //!
    PollBuilder SetType(const PollType type);

    //!
    //! \brief Set the type of poll from the integer representation of the type.
    //!
    //! \throws VotingError If the supplied poll type is not valid.
    //!
    PollBuilder SetType(const int64_t type);

    //!
    //! \brief Set the vote weighing method for the poll.
    //!
    //! \throws VotingError If the supplied weight type is not valid.
    //!
    PollBuilder SetWeightType(const PollWeightType type);

    //!
    //! \brief Set the vote weighing method for the poll from the integer
    //! representation of the weight type.
    //!
    //! \throws VotingError If the supplied weight type is not valid.
    //!
    PollBuilder SetWeightType(const int64_t type);

    //!
    //! \brief Set the method for choosing poll answers.
    //!
    //! \throws VotingError If the supplied response type is not valid.
    //!
    PollBuilder SetResponseType(const PollResponseType type);

    //!
    //! \brief Set the method for choosing poll answers from the integer
    //! representation of the response type.
    //!
    //! \throws VotingError If the supplied response type is not valid.
    //!
    PollBuilder SetResponseType(const int64_t type);

    //!
    //! \brief Set the number of days that the poll remains active.
    //!
    //! \throws VotingError If the supplied duration is outside of the range
    //! of days for a valid poll.
    //!
    PollBuilder SetDuration(const uint32_t days);

    //!
    //! \brief Set the title of the poll.
    //!
    //! \param title A non-empty UTF-8 encoded string.
    //!
    //! \throws VotingError If the supplied title is empty or longer than the
    //! allowed size for a poll title.
    //!
    PollBuilder SetTitle(std::string title);

    //!
    //! \brief Set the URL of the poll discussion webpage.
    //!
    //! \param url A non-empty UTF-8 encoded string.
    //!
    //! \throws VotingError If the supplied URL is empty or longer than the
    //! allowed size for a poll URL.
    //!
    PollBuilder SetUrl(std::string url);

    //!
    //! \brief Set the prompt that voters shall answer.
    //!
    //! \param question A non-empty UTF-8 encoded string.
    //!
    //! \throws VotingError If the supplied question is empty or longer than
    //! the allowed size for a poll question.
    //!
    PollBuilder SetQuestion(std::string question);

    //!
    //! \brief Set the set of possible answer choices for the poll.
    //!
    //! \param labels A set of non-empty UTF-8 encoded choice display labels.
    //!
    //! \throws VotingError If a label is empty or longer than the allowed size
    //! for a poll choice, or if the set of choices exceeds the maximum allowed
    //! number for a poll, or if the set of choices contains a duplicate label.
    //!
    PollBuilder SetChoices(std::vector<std::string> labels);

    //!
    //! \brief Add a set of possible answer choices for the poll.
    //!
    //! \param labels A set of non-empty UTF-8 encoded choice display labels.
    //!
    //! \throws VotingError If a label is empty or longer than the allowed size
    //! for a poll choice, or if the set of choices exceeds the maximum allowed
    //! number for a poll, or if the set of choices contains a duplicate label.
    //!
    PollBuilder AddChoices(std::vector<std::string> labels);

    //!
    //! \brief Add a possible answer choice for the poll.
    //!
    //! \param label A non-empty UTF-8 encoded string.
    //!
    //! \throws VotingError If a label is empty or longer than the allowed size
    //! for a poll choice, or if the set of choices exceeds the maximum allowed
    //! number for a poll, or if the set of choices contains a duplicate label.
    //!
    PollBuilder AddChoice(std::string label);

    //!
    //! \brief Generate a poll contract transaction with the constructed poll.
    //!
    //! \param pwallet Points to a wallet instance to generate the claim from.
    //!
    //! \return A new transaction that contains the poll contract.
    //!
    //! \throws VotingError If the constructed poll is malformed.
    //!
    CWalletTx BuildContractTx(CWallet* const pwallet);

private:
    std::unique_ptr<Poll> m_poll; //!< The poll under construction.
}; // PollBuilder

//!
//! \brief Constructs vote contract objects from user input.
//!
//! This is a move-only type. It provides a fluent interface that applies
//! validation for each of the vote component builder methods.
//!
class VoteBuilder
{
public:
    //!
    //! \brief Move constructor.
    //!
    VoteBuilder(VoteBuilder&& builder);

    //!
    //! \brief Destructor.
    //!
    ~VoteBuilder();

    //!
    //! \brief Initialize a vote builder for the specified poll.
    //!
    //! \param poll      Poll to cast a vote for.
    //! \param poll_txid Transaction hash of the associated poll.
    //!
    static VoteBuilder ForPoll(const Poll& poll, const uint256 poll_txid);

    //!
    //! \brief Move assignment operator.
    //!
    VoteBuilder& operator=(VoteBuilder&& builder);

    //!
    //! \brief Set the vote responses from poll choice offsets.
    //!
    //! \param offsets The offsets of the poll choices to vote for.
    //!
    //! \throws VotingError If an offset exceeds the bounds of the choices in
    //! the poll, or if the set of responses contains a duplicate choice.
    //!
    VoteBuilder SetResponses(const std::vector<uint8_t>& offsets);

    //!
    //! \brief Set the vote responses from poll choice labels.
    //!
    //! \param labels The UTF-8 encoded labels of the poll choices to vote for.
    //!
    //! \throws VotingError If a label does not match the label of a choice in
    //! the poll, or if the set of responses contains a duplicate choice.
    //!
    //! \deprecated Referencing voting items by strings is deprecated. Instead,
    //! use poll choice offsets to select responses for a vote.
    //!
    VoteBuilder SetResponses(const std::vector<std::string>& labels);

    //!
    //! \brief Add vote responses from poll choice offsets.
    //!
    //! \param offsets The offsets of the poll choices to vote for.
    //!
    //! \throws VotingError If an offset exceeds the bounds of the choices in
    //! the poll, or if the set of responses contains a duplicate choice.
    //!
    VoteBuilder AddResponses(const std::vector<uint8_t>& offsets);

    //!
    //! \brief Add vote responses from poll choice labels.
    //!
    //! \param labels The UTF-8 encoded labels of the poll choices to vote for.
    //!
    //! \throws VotingError If a label does not match the label of a choice in
    //! the poll, or if the set of responses contains a duplicate choice.
    //!
    //! \deprecated Referencing voting items by strings is deprecated. Instead,
    //! use poll choice offsets to select responses for a vote.
    //!
    VoteBuilder AddResponses(const std::vector<std::string>& labels);

    //!
    //! \brief Add a vote response from a poll choice offset.
    //!
    //! \param offset The offset of the poll choice to vote for.
    //!
    //! \throws VotingError If an offset exceeds the bounds of the choices in
    //! the poll, or if the set of responses contains a duplicate choice.
    //!
    VoteBuilder AddResponse(const uint8_t offset);

    //!
    //! \brief Add a vote response from a poll choice label.
    //!
    //! \param label The UTF-8 encoded label of the poll choice to vote for.
    //!
    //! \throws VotingError If a label does not match the label of a choice in
    //! the poll, or if the set of responses contains a duplicate choice.
    //!
    //! \deprecated Referencing voting items by strings is deprecated. Instead,
    //! use poll choice offsets to select responses for a vote.
    //!
    VoteBuilder AddResponse(const std::string& label);

    //!
    //! \brief Generate a vote contract transaction with the constructed vote.
    //!
    //! \param pwallet Points to a wallet instance to generate the claim from.
    //!
    //! \return A new transaction that contains the vote contract.
    //!
    //! \throws VotingError If the constructed vote is malformed.
    //!
    CWalletTx BuildContractTx(CWallet* const pwallet);

private:
    const Poll* m_poll;           //!< The poll to create a vote contract for.
    std::unique_ptr<Vote> m_vote; //!< The vote under construction.

    //!
    //! \brief Initialize a vote builder for the specified poll.
    //!
    //! \param poll      Poll to cast a vote for.
    //! \param poll_txid Transaction hash of the associated poll.
    //!
    VoteBuilder(const Poll& poll, const uint256 poll_txid);
}; // VoteBuilder

//!
//! \brief Send a transaction that contains a poll contract.
//!
//! This helper abstracts the transaction-sending code from the GUI layer. We
//! may want to replace it with a sub-routine in a view model when we rewrite
//! the voting GUI components.
//!
//! \param builder An initialized poll builder instance to create the poll
//! contract from.
//!
//! \throws VotingError If the constructed vote is malformed or the transaction
//! fails to send.
//!
void SendPollContract(PollBuilder builder);

//!
//! \brief Send a transaction that contains a vote contract.
//!
//! This helper abstracts the transaction-sending code from the GUI layer. We
//! may want to replace it with a sub-routine in a view model when we rewrite
//! the voting GUI components.
//!
//! \param builder An initialized vote builder instance to create the vote
//! contract from.
//!
//! \throws VotingError If the constructed vote is malformed or the transaction
//! fails to send.
//!
void SendVoteContract(VoteBuilder builder);
}
