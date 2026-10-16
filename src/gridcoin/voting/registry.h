// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_REGISTRY_H
#define GRIDCOIN_VOTING_REGISTRY_H

#include "gridcoin/contract/handler.h"
#include "gridcoin/voting/filter.h"
#include "gridcoin/voting/fwd.h"
#include "gridcoin/voting/poll_entry.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"
#include <atomic>
#include <map>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class CTxDB;

namespace GRC {

//!
//! \brief Thrown when a reorg/fork occurs during a poll registry traversal.
//!
class InvalidDuetoReorgFork : public std::exception
{
public:
    InvalidDuetoReorgFork()
    {
    }
};

class Contract;
class PollRegistry;
class SuperblockPtr;

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
    enum PollNotificationType
    {
        UNKNOWN,
        POLL_ADD,
        POLL_DELETE,
        POLL_EXPIRE_WARNING,
        POLL_NOTIFY_TEST
    };

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
    //! \brief Get the poll (payload) version
    //!
    uint32_t GetPollPayloadVersion() const;

    //!
    //! \brief Get the poll type
    //!
    PollType GetPollType() const;

    //!
    //! \brief Get the title of the associated poll.
    //!
    const std::string& Title() const;

    //!
    //! \brief Get the votes associated with the poll.
    //!
    //! \return A copy of the transaction hashes of the associated votes, in connection order. For a poll stored in
    //! the poll db the list is read from the vote link chain and cached until the chain changes. Takes
    //! PollRegistry::cs_poll_registry (recursive, so a caller may already hold it); PollRegistry is incomplete
    //! here, so the requirement cannot be annotated on the declaration.
    //!
    std::vector<uint256> Votes() const;

    //!
    //! \brief Get the timestamp of the poll transaction.
    //!
    //! \return Poll transaction timestamp in seconds.
    //!
    int64_t Time() const;

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
    //! \brief Get the block index pointer for the first block in the poll duration.
    //!
    //! This is the block that contains the poll contract, recorded when the poll
    //! was registered. It needs no lock: block index entries are never freed, and
    //! the registry deletes the poll if that block is disconnected.
    //!
    //! \return pointer to block index object.
    //!
    CBlockIndex* GetStartingBlockIndexPtr() const;

    //!
    //! \brief Get the block index pointer for the last block in the poll duration.
    //!
    //! \return pointer to block index object.
    //!
    CBlockIndex* GetEndingBlockIndexPtr(CBlockIndex* pindex_start = nullptr) const;

    //!
    //! \brief Get the starting block height for the poll.
    //!
    //! \return block number if begun or std::nullopt (if skeleton reference - this should never happen).
    //!
    std::optional<int> GetStartingHeight() const;

    //!
    //! \brief Get the ending block height for the poll.
    //!
    //! \return block number if ended or std::nullopt if still active.
    //!
    std::optional<int> GetEndingHeight() const;

    //!
    //! \brief Get the magnitude weight factor at the poll start.
    //!
    //! Resolved on each call from the poll's block and transaction time, against
    //! the current protocol registry, as a read of the poll from disk always did.
    //! A factor fixed at registration could differ between a node that saw the
    //! poll arrive and one that replayed it later, when an entry stamped before
    //! the poll was mined after it.
    //!
    //! \return Fraction representing magnitude weight factor.
    //!
    Fraction GetMagnitudeWeightFactor() const;

    //!
    //! \brief Provides string equivalent of PollNotificationType
    //! \param notify_type
    //! \return string equivalent of PollNotificationType
    //!
    std::string NotifyTypeToString(const PollNotificationType& notify_type) const;

    //!
    //! \brief Runs a free thread executing provided poll notification command. Also sets m_expiration_warn_notified
    //! to true if the notification type is EXPIRE_WARNING.
    //! \param notify_type
    //! \return std::string of command run
    //!
    std::string Notify(const PollNotificationType& notify_type) const;

    //!
    //! \brief Returns whether the expiration warning was sent.
    //! \return boolean indicating whether the expiration warning was sent.
    //!
    bool IsExpiringWarningNotified() const;

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
    uint256 m_txid;                             //!< Hash of the poll transaction.
    uint32_t m_payload_version;                 //!< Version of the poll (payload).
    PollType m_type;                            //!< Type of the poll.
    PollWeightType m_weight_type;               //!< Weight type of the poll.
    const std::string* m_ptitle;                //!< Title of the poll for indexing/mapping purposes.
    std::string m_title;                        //!< Original title of the poll for display purposes.
    int64_t m_timestamp;                        //!< Timestamp of the poll transaction.
    uint32_t m_duration_days;                   //!< Number of days the poll remains active.
    const CBlockIndex* m_pindex;                //!< Block that contains the poll contract.
    mutable std::vector<uint256> m_votes;       //!< Linked votes: the list (in-memory poll) or the cached chain (stored poll).
    mutable uint256 m_votes_head;               //!< The chain head m_votes was read at (stored poll).
    mutable bool m_expiration_warn_notified;    //!< Flag to indicate whether the expiration warning was sent.
    bool m_stored;                              //!< Backed by the poll db (a poll from V11 on).
    bool m_deleted;                             //!< Deleted by a master-key REMOVE; kept for its txid and title.
    const PollRegistry* m_registry;             //!< The registry that holds this reference.
    //! The first block past the poll's end found on the main chain, and the poll block it was found from: a cache for
    //! PollRegistry::Snapshot, checked against the main chain on every read, so a reorg that disconnects the block
    //! invalidates it at the next read. Block index entries are never freed (GRC::BlockIndexPool), so the pointer
    //! stays valid after the block leaves the main chain. Guarded by cs_poll_registry.
    mutable const CBlockIndex* m_past_end_from;
    mutable const CBlockIndex* m_past_end;
}; // PollReference

//!
//! \brief One consistent copy of what a tally reads from the poll registry and the chain index.
//!
//! PollRegistry::Snapshot takes it under cs_main and cs_poll_registry. The tally (PollResult::BuildFor), the result
//! cache and the RPCs then read only the snapshot: a change to the registry after the snapshot changes nothing the
//! tally reads. The block pointers need no lock to read: block index
//! entries are never freed, and their heights, times and pprev links never change.
//!
struct PollSnapshot
{
    uint256 m_txid;                                         //!< Hash of the poll transaction.
    uint32_t m_payload_version = 0;                         //!< Version of the poll payload.
    PollType m_type = PollType::UNKNOWN;                    //!< Type of the poll.
    PollWeightType m_weight_type = PollWeightType::UNKNOWN; //!< Weight type of the poll.
    std::string m_title;                                    //!< Title of the poll, for display and logs.
    int64_t m_timestamp = 0;                                //!< Time of the poll transaction.
    int64_t m_end_time = 0;                                 //!< The poll's end (PollReference::Expiration()).
    std::vector<uint256> m_votes;                           //!< Linked votes, in connection order.
    const CBlockIndex* m_pindex_tip = nullptr;              //!< The pinned tip the snapshot was taken on.
    const CBlockIndex* m_pindex_start = nullptr;            //!< The poll's block; nullptr for a skeleton reference.
    //! First block after the poll's block, on the chain that ends at the pinned tip, whose time exceeds the end;
    //! nullptr while that chain has none. The block before it is the poll's closing block.
    const CBlockIndex* m_pindex_past_end = nullptr;
    //! Last block of the active vote weight range, or nullptr while the poll is open (the range then ends at the
    //! pinned tip).
    const CBlockIndex* m_pindex_end = nullptr;
    bool m_v15 = false;                                     //!< The poll's block is at or above the v15 height.
    //! The poll's end follows the v15 rules: its first block past the end is at or above the v15 height. True for
    //! every v15 poll that has closed, and for a poll whose block is below the height but which closes at or above it.
    //! Only the end follows it: the start rules follow m_v15.
    bool m_v15_end = false;
    //! The poll has finished at the pinned tip: its first block past the end is on the tip's chain and at or above the
    //! v15 height (m_v15_end), or the tip's time exceeds the end (a poll that closes below that height).
    bool m_finished = false;
};

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
    //! Display map: title key (ToLower(title)) to the ACTIVE poll that registered it first.
    using PollMapByTitle = std::map<std::string, PollReference*>;
    //! Owning map: every poll the registry holds, ACTIVE or DELETED, by txid.
    using PollMapByTxid = std::map<uint256, PollReference>;

    //!
    //! \brief Initialize an empty poll registry with its poll and vote link databases.
    //!
    PollRegistry();

    //!
    //! \brief Lock for poll registry.
    //!
    //! STATIC, and that is load-bearing for the thread-safety annotations rather
    //! than a statement about instances (there is exactly one registry —
    //! GetPollRegistry() — and never has been another).
    //!
    //! Sequence and Iterator are nested classes that do not own the lock, so their
    //! EXCLUSIVE_LOCKS_REQUIRED clauses have to name it through the enclosing
    //! class. Clang's analyzer can only resolve that to a single, unambiguous
    //! object if the member is static; while it was non-static the annotation
    //! resolved to an unmatchable "some instance's cs_poll_registry", and a caller
    //! that correctly held GetPollRegistry().cs_poll_registry was reported as NOT
    //! holding it. That false positive is why the requirements used to live only
    //! on the out-of-line definitions in registry.cpp, where nothing outside the
    //! translation unit ever saw them — so no caller was ever checked, which is
    //! how an unlocked traversal reached production (VotingManagerImpl::
    //! latestActivePollTime).
    //!
    //! Not `mutable`: a static member is not part of any instance's constness.
    static CCriticalSection cs_poll_registry;

    //!
    //! \brief A traversable, immutable sequence of the polls in the registry.
    //!
    class Sequence
    {
    public:
        using FilterFlag = PollFilterFlag;

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
                const FilterFlag flags,
                const int64_t now) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

            //!
            //! \brief Initialize an iterator for the end of the sequence.
            //!
            Iterator(BaseIterator end) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

            //!
            //! \brief Get the poll reference at the current position.
            //!
            const PollReference& Ref() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

            //!
            //! \brief Load the poll at the current position from disk.
            //!
            PollOption TryPollFromDisk() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

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
            Iterator& operator++() EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

            //!
            //! \brief Advance the current position.
            //!
            Iterator operator++(int) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

            //!
            //! \brief Determine whether the item at the current position is
            //! equal to the specified position.
            //!
            bool operator==(const Iterator& other) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

            //!
            //! \brief Determine whether the item at the current position is
            //! not equal to the specified position.
            //!
            bool operator!=(const Iterator& other) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

        private:
            BaseIterator m_iter GUARDED_BY(cs_poll_registry); //!< The current position.
            BaseIterator m_end GUARDED_BY(cs_poll_registry);  //!< Element after the end of the sequence.
            FilterFlag m_flags GUARDED_BY(cs_poll_registry);  //!< Attributes to filter polls by.
            int64_t m_now GUARDED_BY(cs_poll_registry);       //!< Current time in seconds.

            //!
            //! \brief Advance the iterator to the next item that matches the
            //! configured filters.
            //!
            void SeekNextMatch() EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);
        }; // Iterator

        //!
        //! \brief Initialize a poll sequence.
        //!
        //! \param polls The set of poll references in the registry.
        //! \param flags Attributes to filter polls by.
        //!
        Sequence(const PollMapByTitle& polls, const FilterFlag flags = NO_FILTER) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

        //!
        //! \brief Set the attributes to filter polls by.
        //!
        //! \return A new sequence for the specified poll filters.
        //!
        Sequence Where(const FilterFlag flags) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

        //!
        //! \brief Set whether the sequence skips finished polls.
        //!
        //! \param active_only Whether the sequence skips finished polls.
        //!
        //! \return A new sequence for the specified poll status filters.
        //!
        Sequence OnlyActive(const bool active_only = true) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

        //!
        //! \brief Returns an iterator to the beginning.
        //!
        Iterator begin() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

        //!
        //! \brief Returns an iterator to the end.
        //!
        Iterator end() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    private:
        const PollMapByTitle& m_polls GUARDED_BY(cs_poll_registry) ; //!< Poll references in the registry.
        FilterFlag m_flags GUARDED_BY(cs_poll_registry) ;            //!< Attributes to filter polls by.
    }; // Sequence

    //!
    //! \brief Get a traversable sequence of the polls in the registry.
    //!
    //! \return A traversable type that iterates over the poll references in
    //! the registry and reads poll objects from disk.
    //!
    const Sequence Polls() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get the most recent poll submitted to the network.
    //!
    //! \return Points to a poll object or \c nullptr when no recent polls
    //! exist.
    //!
    const PollReference* TryLatestActive() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get the poll with the specified transaction ID.
    //!
    //! \param title Hash of the transaction that contains the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll exists for
    //! the supplied transaction hash.
    //!
    const PollReference* TryByTxid(const uint256 txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Copy what a tally of \p ref reads, as of the pinned tip \p pindex_tip.
    //!
    //! \param ref        A poll this registry holds.
    //! \param pindex_tip The tip the tally runs on, read under the same cs_main.
    //!
    PollSnapshot Snapshot(const PollReference& ref, const CBlockIndex* pindex_tip) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Get the poll with the specified title.
    //!
    //! \param title The title of the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll contains a
    //! matching title.
    //!
    const PollReference* TryByTitle(const std::string& title) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get an existing poll in the registry, or if not found, demand load a
    //! poll identified by the provided txid along with its votes. This is a temporary
    //! workaround to deal with polls beyond the lookback period for contract load
    //! during wallet init. This should be replaced when the poll caching state machine
    //! code is implemented.
    //!
    //! \return Points to a poll object or \c nullptr if no poll added that matches
    //! the supplied txid.
    const PollReference* TryByTxidWithAddHistoricalPollAndVotes(const uint256 txid)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Destroy the contract handler state to prepare for historical
    //! contract replay.
    //!
    void Reset() override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Load the poll and vote link databases and rebuild the in-memory indexes from their records, in
    //! record order. Idempotent until the next Reset() or ResetInMemoryOnly(): InitializeContracts calls it once for
    //! POLL and once for VOTE, which both map to this registry.
    //!
    //! \return The block height through which the databases hold records.
    //!
    int Initialize() override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief The higher of the two databases' heights. Every stored record advances its own database's height, so
    //! every POLL and VOTE contract below the higher height was processed.
    //!
    int GetDBHeight() override;

    //!
    //! \brief Set both databases' heights (a disconnect winding the bookmark back).
    //!
    void SetDBHeight(int& height) override;

    //!
    //! \brief Remove from memory the records that no current map references.
    //!
    uint64_t PassivateDB() override;

    //!
    //! \brief Clear the in-memory state only, keeping the databases (a restart, for tests).
    //!
    void ResetInMemoryOnly();

    //!
    //! \brief Whether a poll record or vote link with this hash is stored. Reads without loading a passivated record.
    //!
    bool IsStored(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief The hash of the poll's newest vote link, or null when it has none.
    //!
    uint256 VoteHead(const uint256& poll_txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief The txid of the poll that registered a title key first in chain order, ACTIVE or DELETED.
    //!
    std::optional<uint256> TitleOwner(const std::string& title_key) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief The poll's linked votes in connection order, read from its vote link chain. Older links are read with
    //! RegistryDB::peek(), so they stay passivated.
    //!
    std::vector<uint256> WalkVotes(const uint256& poll_txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Perform contextual validation for the provided contract.
    //!
    //! \param contract Contract to validate.
    //! \param tx       Transaction that contains the contract.
    //! \param DoS      Misbehavior score out.
    //!
    //! \return \c false If the contract fails validation.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Perform contextual validation for the provided contract including block context. This is used
    //! in ConnectBlock.
    //!
    //! \param ctx ContractContext to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return  \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Register a poll or vote from contract data.
    //!
    //! \param ctx References the poll or vote contract and associated context.
    //!
    void Add(const ContractContext& ctx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Deregister the poll or vote specified by contract data.
    //!
    //! \param ctx References the poll or vote contract and associated context.
    //!
    void Delete(const ContractContext& ctx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Revert a POLL or VOTE contract of a disconnected block. A disconnect pops a block's records newest first
    //! at every height (RevertContracts), so each revert erases the newest record of its chain: a poll's ADD record
    //! (with its title, if it owns it), a REMOVE's DELETED record (the ADD record becomes current again), or a vote's
    //! link (the previous link becomes current again). Below V11 nothing is reverted (the V11 floor).
    //!
    void Revert(const ContractContext& ctx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Detect reorganizations that would affect registry traversal.
    //!
    void DetectReorg();

    //!
    //! \brief RAII scope that marks a registry traversal in progress for its
    //! lifetime.
    //!
    //! registry_traversal_in_progress is a COUNT of active traversals, not a
    //! bool: table builds can overlap (e.g. a GUI PollResultCache::BuildPollTable
    //! and a concurrent RPC PollResultToJson/VoteDetailsToJson tally). A plain
    //! bool let the first traversal to finish clear the flag while another was
    //! still running, so DetectReorg could wrongly clear
    //! reorg_occurred_during_reg_traversal mid-traversal. Incrementing on entry
    //! and decrementing on exit (on every path, including exceptions) keeps the
    //! reorg detector armed until the LAST traversal finishes.
    //!
    class TraversalScope
    {
    public:
        explicit TraversalScope(PollRegistry& registry) : m_registry(registry)
        {
            ++m_registry.registry_traversal_in_progress;
        }

        ~TraversalScope() { --m_registry.registry_traversal_in_progress; }

        TraversalScope(const TraversalScope&) = delete;
        TraversalScope& operator=(const TraversalScope&) = delete;

    private:
        PollRegistry& m_registry;
    };

    std::atomic<int> registry_traversal_in_progress = 0;           //!< Count of registry traversals in progress.
    std::atomic<bool> reorg_occurred_during_reg_traversal = false; //!< Boolean to indicate whether a reorg occurred.
    std::atomic<bool> m_replay_in_progress = false;                //!< A replay or rebuild is applying contracts.

    //!
    //! \brief Marks a contract replay (the startup replay or a rebuild) for its scope, so the polls and votes it
    //! applies are not announced: only a block that connects live runs -pollnotify or signals the GUI.
    //!
    class ReplayScope
    {
    public:
        explicit ReplayScope(PollRegistry& registry)
            : m_registry(registry), m_saved(registry.m_replay_in_progress.exchange(true))
        {
        }
        ~ReplayScope() { m_registry.m_replay_in_progress = m_saved; }

        ReplayScope(const ReplayScope&) = delete;
        ReplayScope& operator=(const ReplayScope&) = delete;

    private:
        PollRegistry& m_registry;
        const bool m_saved;
    };

private:
    PollMapByTitle m_polls GUARDED_BY(cs_poll_registry);             //!< Display map: ACTIVE polls by title key.
    PollMapByTxid m_polls_by_txid GUARDED_BY(cs_poll_registry);      //!< Every poll held, by txid (owning).
    const PollReference* m_latest_poll GUARDED_BY(cs_poll_registry); //!< Cache for the most recent poll.

    PollEntryDB m_poll_db GUARDED_BY(cs_poll_registry);                       //!< Poll records, from V11.
    //! Vote links, from V11. Mutable because WalkVotes reads it with peek(), which changes no observable state.
    mutable VoteLinkDB m_vote_db GUARDED_BY(cs_poll_registry);
    PollEntryMap m_poll_entries GUARDED_BY(cs_poll_registry);                 //!< Poll txid to its newest record.
    VoteLinkMap m_vote_links GUARDED_BY(cs_poll_registry);                    //!< Poll txid to its newest link.
    std::map<std::string, uint256> m_title_index GUARDED_BY(cs_poll_registry); //!< Title key to first registration.
    bool m_initialized GUARDED_BY(cs_poll_registry);                           //!< Initialize() has loaded the dbs.

    //! Set while TryByTxidWithAddHistoricalPollAndVotes registers a pre-V11 poll in memory only, the one path that
    //! passes the V11 floor (until the legacy store replaces it).
    bool m_registering_legacy_poll GUARDED_BY(cs_poll_registry);

    //!
    //! \brief Erase a stored vote's link, which must be its poll's newest, and make the previous link current.
    //!
    //! \return false, changing nothing, when the link is not the poll's newest.
    //!
    bool EraseVoteLink(const uint256& vote_txid) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Mark the poll dbs for a rebuild at the next start: write version 0 under the poll db's version key, so
    //! the next Initialize() clears both dbs and the startup replay rebuilds them from V11.
    //!
    void FlagRebuild() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Clear every in-memory structure, keeping the dbs' contents.
    //!
    void ClearInMemory() EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Update the in-memory indexes from one poll record: the load observer calls it for every stored record
    //! in record order, and a live connect calls it for the record it just stored.
    //!
    void IndexPollRecord(const PollEntry& entry) EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Get the poll with the specified title.
    //!
    //! \param title The title of the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll contains a
    //! matching title.
    //!
    PollReference* TryBy(const std::string& title) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get the poll with the specified title.
    //!
    //! \param title The title of the poll to look up.
    //!
    //! \return Points to a poll object or \c nullptr when no poll exists for
    //! the supplied transaction hash.
    //!
    PollReference* TryBy(const uint256 txid) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

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

//!
//! \brief Find the last well-formed superblock at or before a block.
//!
//! The superblock flags in the block index are not set for superblocks before
//! height 1034768 on mainnet or 196562 on testnet, so below those heights the
//! claim of every block is read.
//!
//! \param pindex The block to search back from.
//!
//! \return The superblock, bound to its block, or nothing if none is found.
//!
std::optional<SuperblockPtr> LastWellFormedSuperblock(const CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//!
//! \brief Read the poll contract in the transaction \p txid from disk, with the transaction's time as the poll's
//! timestamp. The magnitude weight factor is not serialized; the caller resolves it.
//!
PollOption ReadPollFromDisk(CTxDB& txdb, const uint256& txid);

//!
//! \brief Compute the active vote weight of a tallied poll, which decides whether the poll is validated.
//!
//! \param snapshot The snapshot the tally ran on. The range runs from its poll block through m_pindex_end, or
//! through its pinned tip while the poll is open.
//! \param result   The tallied result. Its poll supplies the magnitude weight factor the votes were weighed with,
//! and its m_pools_voted the pools that stay in the magnitude correction.
//!
//! \return The active vote weight, or nothing for a skeleton reference, a weight type without one, or a poll with no
//! well-formed superblock at or before its block.
//!
std::optional<CAmount> ActiveVoteWeight(const PollSnapshot& snapshot, const PollResult& result);
}

#endif // GRIDCOIN_VOTING_REGISTRY_H
