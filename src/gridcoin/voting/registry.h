// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/contract/handler.h"
#include "gridcoin/voting/fwd.h"

class CTxDB;

namespace GRC {

class Contract;
class PollRegistry;

//!
//! \brief Stores an in-memory reference to a poll contract and its votes.
//!
//! The poll registry tracks recent poll and vote contracts submitted to the
//! network. The registry stores poll reference objects rather than complete
//! contracts to avoid consuming memory to maintain this state.
//!
//! This class associates votes with polls and contains the transaction hash
//! used to locate and load the poll contract from disk.
//!
class PollReference
{
    friend class PollRegistry;

public:
    //!
    //! \brief Initialize an empty, invalid poll reference object.
    //!
    PollReference();

    //!
    //! \brief Load the associated poll object from disk.
    //!
    //! \return An object that contains the associated poll if successful.
    //!
    PollOption TryReadFromDisk(CTxDB& txdb) const;

    //!
    //! \brief Load the associated poll object from disk.
    //!
    //! \return An object that contains the associated poll if successful.
    //!
    PollOption TryReadFromDisk() const;

    //!
    //! \brief Get the hash of the transaction that contains the associated poll.
    //!
    uint256 Txid() const;

    //!
    //! \brief Get the title of the associated poll.
    //!
    const std::string& Title() const;

    //!
    //! \brief Get the votes associated with the poll.
    //!
    //! \return The set of transaction hashes of the associated votes.
    //!
    const std::vector<uint256>& Votes() const;

    //!
    //! \brief Get the elapsed time since poll creation.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return Poll age in seconds.
    //!
    int64_t Age(const int64_t now) const;

    //!
    //! \brief Determine whether the poll age exceeds the duration of the poll.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return \c true if a poll's age exceeds the poll duration.
    //!
    bool Expired(const int64_t now) const;

    //!
    //! \brief Get the time when the poll expires.
    //!
    //! \return Expiration time as the number of seconds since the UNIX epoch.
    //!
    int64_t Expiration() const;

    //!
    //! \brief Record a transaction that contains a response to the poll.
    //!
    //! \param txid Hash of the transaction that contains the vote.
    //!
    void LinkVote(const uint256 txid);

    //!
    //! \brief Remove a record of a transaction that contains a response to
    //! the poll.
    //!
    //! \param txid Hash of the transaction that contains the vote.
    //!
    void UnlinkVote(const uint256 txid);

private:
    const uint256* m_ptxid;       //!< Hash of the poll transaction.
    const std::string* m_ptitle;  //!< Title of the poll.
    int64_t m_timestamp;          //!< Timestamp of the poll transaction.
    uint32_t m_duration_days;     //!< Number of days the poll remains active.
    std::vector<uint256> m_votes; //!< Hashes of the linked vote transactions.
}; // PollReference

//!
//! \brief Tracks recent polls and votes submitted to the network.
//!
//! THREAD SAFETY: This API uses the transaction database to read poll and vote
//! contracts from disk. Always lock \c cs_main for the poll registry, for poll
//! references, and for iterator lifetimes.
//!
class PollRegistry : public IContractHandler
{
public:
    using PollMapByTitle = std::map<std::string, PollReference>;
    using PollMapByTxid = std::map<uint256, PollReference*>;

    //!
    //! \brief A traversable, immutable sequence of the polls in the registry.
    //!
    class Sequence
    {
    public:
        //!
        //! \brief Behaves like a forward \c const iterator.
        //!
        class Iterator
        {
        public:
            using BaseIterator = PollMapByTitle::const_iterator;
            using difference_type = BaseIterator::difference_type;
            using iterator_category = std::forward_iterator_tag;

            using value_type = Iterator;
            using pointer = const value_type*;
            using reference = const value_type&;

            //!
            //! \brief Initialize an iterator for the beginning of the sequence.
            //!
            Iterator(
                BaseIterator iter,
                BaseIterator end,
                const bool active_only,
                const int64_t now);

            //!
            //! \brief Initialize an iterator for the end of the sequence.
            //!
            Iterator(BaseIterator end);

            //!
            //! \brief Get the poll reference at the current position.
            //!
            const PollReference& Ref() const;

            //!
            //! \brief Load the poll at the current position from disk.
            //!
            PollOption TryPollFromDisk() const;

            //!
            //! \brief Get a reference to the current position.
            //!
            //! \return A reference to itself.
            //!
            reference operator*() const;

            //!
            //! \brief Get a pointer to the current position.
            //!
            //! \return A pointer to itself.
            //!
            pointer operator->() const;

            //!
            //! \brief Advance the current position.
            //!
            Iterator& operator++();

            //!
            //! \brief Advance the current position.
            //!
            Iterator operator++(int);

            //!
            //! \brief Determine whether the item at the current position is
            //! equal to the specified position.
            //!
            bool operator==(const Iterator& other) const;

            //!
            //! \brief Determine whether the item at the current position is
            //! not equal to the specified position.
            //!
            bool operator!=(const Iterator& other) const;

        private:
            BaseIterator m_iter; //!< The current position.
            BaseIterator m_end;  //!< Element after the end of the sequence.
            bool m_active_only;  //!< Whether to skip finished polls.
            int64_t m_now;       //!< Current time in seconds.
        }; // Iterator

        //!
        //! \brief Initialize a poll sequence.
        //!
        //! \param polls       The set of poll references in the registry.
        //! \param active_only Whether the sequence skips finished polls.
        //!
        Sequence(const PollMapByTitle& polls, const bool active_only = false);

        //!
        //! \brief Set whether the sequence skips finished polls.
        //!
        //! \param active_only Whether the sequence skips finished polls.
        //!
        Sequence OnlyActive(const bool active_only = true) const;

        //!
        //! \brief Returns an iterator to the beginning.
        //!
        Iterator begin() const;

        //!
        //! \brief Returns an iterator to the end.
        //!
        Iterator end() const;

    private:
        const PollMapByTitle& m_polls; //!< Poll references in the registry.
        bool m_active_only;            //!< Whether to skip finished polls.
    }; // Sequence

    //!
    //! \brief Get a traversable sequence of the polls in the registry.
    //!
    //! \return A traversable type that iterates over the poll references in
    //! the registry and reads poll objects from disk.
    //!
    const Sequence Polls() const;

    //!
    //! \brief Get the most recent poll submitted to the network.
    //!
    //! \return Points to a poll object or \c nullptr when no recent polls
    //! exist.
    //!
    const PollReference* TryLatestActive() const;

    //!
    //! \brief Get the poll with the specified transaction ID.
    //!
    //! \param title Hash of the transaction that contains the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll exists for
    //! the supplied transaction hash.
    //!
    const PollReference* TryByTxid(const uint256 txid) const;

    //!
    //! \brief Get the poll with the specified title.
    //!
    //! \param title The title of the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll contains a
    //! matching title.
    //!
    const PollReference* TryByTitle(const std::string& title) const;

    //!
    //! \brief Destroy the contract handler state to prepare for historical
    //! contract replay.
    //!
    void Reset() override;

    //!
    //! \brief Perform contextual validation for the provided contract.
    //!
    //! \param contract Contract to validate.
    //! \param tx       Transaction that contains the contract.
    //!
    //! \return \c false If the contract fails validation.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx) const override;

    //!
    //! \brief Register a poll or vote from contract data.
    //!
    //! \param ctx References the poll or vote contract and associated context.
    //!
    void Add(const ContractContext& ctx) override;

    //!
    //! \brief Deregister the poll or vote specified by contract data.
    //!
    //! \param ctx References the poll or vote contract and associated context.
    //!
    void Delete(const ContractContext& ctx) override;

private:
    PollMapByTitle m_polls;             //!< Poll references keyed by title.
    PollMapByTxid m_polls_by_txid;      //!< Poll references keyed by TXID.
    const PollReference* m_latest_poll; //!< Cache for the most recent poll.

    //!
    //! \brief Get the poll with the specified title.
    //!
    //! \param title The title of the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll contains a
    //! matching title.
    //!
    PollReference* TryBy(const std::string& title);

    //!
    //! \brief Get the poll with the specified title.
    //!
    //! \param title The title of the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll exists for
    //! the supplied transaction hash.
    //!
    PollReference* TryBy(const uint256 txid);

    //!
    //! \brief Register a poll from contract data.
    //!
    //! \param ctx References the poll contract and associated context.
    //!
    void AddPoll(const ContractContext& ctx);

    //!
    //! \brief Register a vote from contract data.
    //!
    //! \param ctx References the vote contract and associated context.
    //!
    void AddVote(const ContractContext& ctx);

    //!
    //! \brief Deregister the poll specified by contract data.
    //!
    //! \param ctx References the poll contract and associated context.
    //!
    void DeletePoll(const ContractContext& ctx);

    //!
    //! \brief Deregister the vote specified by contract data.
    //!
    //! \param ctx References the vote contract and associated context.
    //!
    void DeleteVote(const ContractContext& ctx);
}; // PollRegistry

//!
//! \brief Get the global poll registry.
//!
//! \return Current global poll registry instance.
//!
PollRegistry& GetPollRegistry();
}
