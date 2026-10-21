// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_REGISTRY_H
#define GRIDCOIN_VOTING_REGISTRY_H

#include "gridcoin/contract/handler.h"
#include "gridcoin/voting/filter.h"
#include "gridcoin/voting/fwd.h"
#include "gridcoin/voting/legacy_store.h"
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
    //! \brief Get the poll's start.
    //!
    //! \return The time of the poll's block when that block is at or above the v15 height; otherwise, and for a
    //! reference with no block, the time of the poll transaction (#3470).
    //!
    int64_t StartTime() const;

    //!
    //! \brief Get the elapsed time since the poll's start (StartTime()).
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
    //! \brief Get the time when the poll expires: its start (StartTime()) plus its duration.
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
    //! \brief Get the starting block height for the poll.
    //!
    //! \return block number if begun or std::nullopt (if skeleton reference - this should never happen).
    //!
    std::optional<int> GetStartingHeight() const;

    //!
    //! \brief Get the magnitude weight factor at the poll start.
    //!
    //! Resolved on each call from the poll's block and start (StartTime(), the block's time from v15), against
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

private:
    uint256 m_txid;                             //!< Hash of the poll transaction.
    uint32_t m_payload_version;                 //!< Version of the poll (payload).
    PollType m_type;                            //!< Type of the poll.
    PollWeightType m_weight_type;               //!< Weight type of the poll.
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
    //! PollRegistry::Snapshot's cache of the forward search on the main chain for the poll's end: the poll block the
    //! search started from, the last block it examined, and the first block at or past the end and the first block
    //! past the end it found (nullptr until found). The search stops at the first block past the end; an open poll's
    //! resumes after the last block examined. The cache holds while the last block examined is on the main chain (its
    //! ancestors, and so everything found, are then unchanged), checked on every read, so a reorg that disconnects it
    //! invalidates the cache at the next read. Block index entries are never freed (GRC::BlockIndexPool), so the
    //! pointers stay valid after a block leaves the main chain. Guarded by cs_poll_registry. The poll's end needs no
    //! place in that check: it is fixed by the poll's block and the v15 height, and neither changes in a running node
    //! (a reorg that moves the poll re-creates the reference).
    mutable const CBlockIndex* m_end_search_from;
    mutable const CBlockIndex* m_end_searched;
    mutable const CBlockIndex* m_at_or_past_end;
    mutable const CBlockIndex* m_past_end;
    //! Set once PollRegistry::Snapshot has logged that the poll's block is off the main chain (a corrupt record), so it
    //! logs that once per reference. Guarded by cs_poll_registry.
    mutable bool m_off_main_chain_logged;
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
    int64_t m_start_time = 0;                               //!< The poll's start (PollReference::StartTime()).
    int64_t m_end_time = 0;                                 //!< The poll's end (PollReference::Expiration()).
    std::vector<uint256> m_votes;                           //!< Linked votes, in connection order.
    const CBlockIndex* m_pindex_tip = nullptr;              //!< The pinned tip the snapshot was taken on.
    //! The poll's block; nullptr for a skeleton reference, or for a poll whose block is off the main chain (a corrupt
    //! record, PollRegistry::Snapshot).
    const CBlockIndex* m_pindex_start = nullptr;
    //! First block after the poll's block, on the chain that ends at the pinned tip, whose time exceeds the end;
    //! nullptr while that chain has none. The block before it is the poll's closing block.
    const CBlockIndex* m_pindex_past_end = nullptr;
    //! Last block of the active vote weight range: the closing block for a poll with m_v15_end, the first block at or
    //! past the end for another finished poll, and nullptr while the poll is open (the range then ends at the pinned
    //! tip).
    const CBlockIndex* m_pindex_end = nullptr;
    //! The block that bounds the tally by blocks: the closing block (the block before m_pindex_past_end) for
    //! a poll with m_v15_end, the pinned tip for an open poll whose block is at or above the v15 height, and nullptr
    //! for every other poll, whose tally keeps the time rules of #3470.
    const CBlockIndex* m_pindex_bound = nullptr;
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
    //! Display map: every ACTIVE poll by its title key (GRC::PollTitleKey). More than one poll can share a key:
    //! below v15 a poll whose title key is taken is still registered by its txid, and a pre-V11 poll loaded on demand
    //! can share a later poll's key. The title index, not this map, says which poll owns a title.
    using PollMapByTitle = std::multimap<std::string, PollReference*>;
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
    //! It walks every poll. Whether a poll is open is judged by blocks on a tip (PollRegistry::OpenAt, Snapshot), not
    //! here.
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
            Iterator(BaseIterator iter, BaseIterator end) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

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
        }; // Iterator

        //!
        //! \brief Initialize a poll sequence.
        //!
        //! \param polls The set of poll references in the registry.
        //!
        Sequence(const PollMapByTitle& polls) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

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
    }; // Sequence

    //!
    //! \brief Get a traversable sequence of the polls in the registry.
    //!
    //! \return A traversable type that iterates over the poll references in
    //! the registry and reads poll objects from disk.
    //!
    const Sequence Polls() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get the newest poll still open on \p pindex_tip, judged by blocks, or nullptr.
    //!
    const PollReference* TryLatestActive(const CBlockIndex* pindex_tip) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

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
    //! \brief Whether the poll is open on \p pindex_tip, judged exactly as Snapshot judges m_finished (by blocks),
    //! without copying the vote list. Throws InvalidDuetoReorgFork as Snapshot does; a caller holding
    //! cs_main from reading the tip to its last call (all of them today) never sees it.
    //!
    bool OpenAt(const PollReference& ref, const CBlockIndex* pindex_tip) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Get the ACTIVE poll that owns a title key: the first poll registered with it since V11. A pre-V11
    //! poll owns no title, and a title whose owner was removed names no poll, even when a later poll with its key is
    //! active. This is the poll a master-key REMOVE by title names (below v15).
    //!
    //! \param title_key The title key (GRC::PollTitleKey).
    //!
    const PollReference* TryByTitle(const std::string& title_key) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get every ACTIVE poll with a title key, in block order (height, then txid): the polls registered since
    //! V11 and the pre-V11 polls loaded on demand.
    //!
    //! \param title_key The title key (GRC::PollTitleKey).
    //!
    std::vector<const PollReference*> FindByTitle(const std::string& title_key) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get a poll by txid: one the registry holds, or else a poll below V11 from the legacy store, filled
    //! from the chain on first demand. A legacy poll never enters the registry itself.
    //!
    //! \param legacy_lookup Set to the legacy store's answer when the store is asked (the poll is neither held by the
    //! registry nor already shown); NOT_LEGACY otherwise. BELOW_FLOOR and INCOMPLETE tell why no poll is returned.
    //!
    //! \return The poll, or \c nullptr.
    //!
    const PollReference* TryByTxidOrLegacy(const uint256& txid, LegacyPollStore::Lookup& legacy_lookup)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief The lowest height the legacy store serves.
    //!
    int LegacyFloor() const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry) { return m_legacy_store.Floor(); }

    //!
    //! \brief Get a listed poll by txid: one the registry holds, or one the legacy store holds. Nothing is
    //! filled. The display paths use it; voting and removal take only a poll the registry holds (TryByTxid).
    //!
    const PollReference* TryListedByTxid(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Open the legacy poll store for polls from \p floor to below \p ceiling, and show the polls it holds.
    //! Initialize calls it with the network's floor and V11 height.
    //!
    void InitializeLegacyStore(int floor, int ceiling) EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Erase the legacy poll store, on disk and in memory, and the polls it shows
    //! (-clearlegacypollhistory).
    //!
    void ClearLegacyStore();

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

    LegacyPollStore m_legacy_store GUARDED_BY(cs_poll_registry); //!< Polls below V11.
    //! The references of the polls the legacy store holds, shown in the display map only.
    std::map<uint256, PollReference> m_legacy_polls GUARDED_BY(cs_poll_registry);

    //!
    //! \brief Build, keep and show the reference of a poll the legacy store holds, once.
    //!
    //! \return The reference, or \c nullptr if the poll's block is not indexed (a corrupt store).
    //!
    const PollReference* ShowLegacyPoll(const LegacyPollEntry& entry) EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Advance \p ref's cached search for its end on the main chain to \p pindex_bound, starting it
    //! again from the poll's block when the cache no longer holds. The poll has a block, and \p pindex_bound is on the
    //! main chain.
    //!
    void AdvanceEndSearch(const PollReference& ref, const CBlockIndex* pindex_bound) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Whether a vote in \p pindex_vote is linked to \p poll_ref.
    //!
    //! From v15 a vote is linked only from a block after the poll's block. A poll whose first block past the end on the
    //! vote's chain is at or above the v15 height, at or before the vote's block, has closed by block, and links
    //! nothing more. Otherwise the #3470 rule stands: a vote is linked when its block's time does not exceed the poll's
    //! end. (A legacy vote never reaches the registry: none is mined from V11, and below V11 the registry applies
    //! nothing; the legacy store links them.) It reads no block after \p pindex_vote: while a block is connected its
    //! parent is the tip, and in a replay or a rebuild it is on the main chain.
    //!
    //! \param poll_ref    The poll the vote names.
    //! \param pindex_vote The block that carries the vote.
    //!
    bool VoteInWindow(const PollReference& poll_ref, const CBlockIndex* pindex_vote) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

    //!
    //! \brief Everything Snapshot takes except the vote list.
    //!
    PollSnapshot SnapshotWithoutVotes(const PollReference& ref, const CBlockIndex* pindex_tip) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry);

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

    //! A shown poll's place in its title key's block order: its block's height, then its txid.
    std::pair<int, uint256> ShownOrder(const PollReference& ref) const EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //! Put one poll in the display map, in block order among the polls with its title key; \p height is its block's.
    void Show(const std::string& title_key, PollReference& ref, int height) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //! Take one poll out of the display map, leaving every other poll with its title key.
    void EraseShown(const std::string& title_key, const PollReference* poll_ref)
        EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get the ACTIVE poll that owns a title key (TryByTitle), for a caller that changes it.
    //!
    //! \param title The title key (GRC::PollTitleKey).
    //!
    //! \return Points to a poll object or \c nullptr when no ACTIVE poll owns the title key.
    //!
    PollReference* TryBy(const std::string& title) EXCLUSIVE_LOCKS_REQUIRED(cs_poll_registry);

    //!
    //! \brief Get the poll registered with a transaction hash.
    //!
    //! \param txid The hash of the poll's transaction.
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
