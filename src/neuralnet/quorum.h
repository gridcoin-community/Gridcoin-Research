#pragma once

#include <memory>
#include <string>

class CBlockIndex;

namespace NN {

class Claim;
class QuorumHash;
class Superblock;

//!
//! \brief A smart pointer that wraps a superblock object for shared ownership
//! with context of its containing block.
//!
//! In general, this class represents a superblock published and received in a
//! block.
//!
class SuperblockPtr
{
public:
    int64_t m_height;    //!< Height of the block that contains the contract.
    int64_t m_timestamp; //!< Timestamp of the block that contains the contract.

    //!
    //! \brief Wrap the provided superblock and store context of its containing
    //! block.
    //!
    //! \param superblock The superblock object to wrap.
    //! \param pindex     Index of the block that contains the superblock.
    //!
    //! \return A smart pointer that wraps the provided superblock.
    //!
    static SuperblockPtr BindShared(
        Superblock&& superblock,
        const CBlockIndex* const pindex)
    {
        return SuperblockPtr(
            std::make_shared<const Superblock>(std::move(superblock)),
            pindex);
    }

    //!
    //! \brief Create a representation of an empty, invalid superblock.
    //!
    //! \return A smart pointer to an empty superblock.
    //!
    static SuperblockPtr Empty()
    {
        return SuperblockPtr(std::make_shared<const Superblock>(), 0, 0);
    }

    const Superblock& operator*() const noexcept { return *m_superblock; }
    const Superblock* operator->() const noexcept { return m_superblock.get(); }

    //!
    //! \brief Get the current age of the superblock.
    //!
    //! \return Superblock age in seconds.
    //!
    int64_t Age() const
    {
        return GetAdjustedTime() - m_timestamp;
    }

private:
    std::shared_ptr<const Superblock> m_superblock; //!< The wrapped superblock.

    //!
    //! \brief Initialize a new superblock smart pointer wrapper.
    //!
    //! \param superblock Smart pointer around a superblock object.
    //! \param height     Height of the block that contains the superblock.
    //! \param timestamp  Time of the block that contains the superblock.
    //!
    SuperblockPtr(
        std::shared_ptr<const Superblock> superblock,
        const int64_t height,
        const int64_t timestamp)
        : m_height(height)
        , m_timestamp(timestamp)
        , m_superblock(std::move(superblock))
    {
    }

    //!
    //! \brief Initialize a new superblock smart pointer.
    //!
    //! \param superblock Smart pointer around a superblock object.
    //! \param pindex     Provides context about the containing block.
    //!
    SuperblockPtr(
        std::shared_ptr<const Superblock> superblock,
        const CBlockIndex* const pindex)
        : SuperblockPtr(std::move(superblock), pindex->nHeight, pindex->nTime)
    {
    }
};

//!
//! \brief Produces, stores, and validates superblocks.
//!
//! The quorum system enables the Gridcoin network to arrive at a consensus on
//! the daily superblocks. The legacy strategy for historical blocks follows a
//! protocol wherein nodes vote for superblocks. After switching to version 11
//! blocks, nodes create and validate superblocks from scraper convergences.
//!
//! This class is designed as a facade that provides an interface for the rest
//! of the application. It consumes information from newly-connected blocks to
//! update the superblock data for research reward calculations.
//!
//! THREAD SAFETY: The quorum system interacts closely with pointers to blocks
//! in the chain index. Always lock cs_main before calling its methods.
//!
class Quorum
{
public:
    //!
    //! \brief Determine whether the quorum system is active.
    //!
    //! \return \c true if the scraper is disabled or if the quorum system is
    //! explicitly configured as active.
    //!
    static bool Active();

    //!
    //! \brief Determine whether the provided address participates in the
    //! quorum consensus at the specified time.
    //!
    //! \param address Default wallet address of a node in the network.
    //! \param time    Timestamp to check participation at.
    //!
    //! \return \c true if the address matches the subset of addrsses that
    //! particpate in the quorum on the day of the year of \p time.
    //!
    static bool Participating(const std::string& address, const int64_t time);

    //!
    //! \brief Get the hash of the pending superblock with the greatest vote
    //! weight.
    //!
    //! \param pindex Provides context about the chain tip used to calculate
    //! quorum vote weights.
    //!
    //! \return Quorum hash of the most popular superblock or an invalid hash
    //! when no nodes voted in the current superblock cycle.
    //!
    static QuorumHash FindPopularHash(const CBlockIndex* const pindex);

    //!
    //! \brief Store a node's vote for a pending superblock in the cache.
    //!
    //! \param quorum_hash Hash of the superblock the node voted for.
    //! \param grc_address Default wallet address of the node.
    //! \param pindex      The block that contains the vote to store.
    //!
    static void RecordVote(
        const QuorumHash quorum_hash,
        const std::string& grc_address,
        const CBlockIndex* const pindex);

    //!
    //! \brief Remove a node's vote for a pending superblock from the cache.
    //!
    //! \param pindex The block that contains the vote to remove.
    //!
    static void ForgetVote(const CBlockIndex* const pindex);

    //!
    //! \brief Validate a superblock published to the network for the day.
    //!
    //! \param claim      Contains the superblock data staked in a block.
    //! \param superblock The claim's superblock to validate.
    //! \param pindex     Provides context for the block containing the superblock.
    //!
    static bool ValidateSuperblockClaim(
        const Claim& claim,
        const SuperblockPtr& superblock,
        const CBlockIndex* const pindex);

    //!
    //! \brief Validate the supplied superblock by comparing it to the node's
    //! local manifest data.
    //!
    //! \param superblock The superblock to validate.
    //! \param use_cache  If \c false, skip validation with the scraper cache.
    //! \param hint_bits  For testing by-project fallback validation.
    //!
    //! \return \c true if local manifest data produces a matching superblock.
    //!
    static bool ValidateSuperblock(
        const SuperblockPtr& superblock,
        const bool use_cache = true,
        const size_t hint_bits = 32);

    //!
    //! \brief Get a reference to the current active superblock.
    //!
    //! \return The most recent superblock applied by the tally.
    //!
    static SuperblockPtr CurrentSuperblock();

    //!
    //! \brief Get a reference to the upcoming superblock.
    //!
    //! After a node receives a new superblock, the tally must commit it before
    //! it becomes active.
    //!
    //! \return A superblock pending activation if one exists. Returns an empty
    //! superblock when the tally already activated the latest superblock.
    //!
    static SuperblockPtr PendingSuperblock();

    //!
    //! \brief Determine whether any superblocks are pending activation.
    //!
    //! \return \c true if the index contains a superblock loaded at a height
    //! above the last tally window.
    //!
    static bool HasPendingSuperblock();

    //!
    //! \brief Determine whether the network expects a new superblock.
    //!
    //! \return \c true if the age of the current superblock exceeds the
    //! protocol's superblock spacing parameter.
    //!
    static bool SuperblockNeeded();

    //!
    //! \brief Initialze the tally's superblock context.
    //!
    //! \param pindexLast The most recent block to begin loading backward from.
    //!
    static void LoadSuperblockIndex(const CBlockIndex* pindexLast);

    //!
    //! \brief Create a new superblock from scraper convergence data.
    //!
    //! \return A new superblock to publish to the network.
    //!
    static Superblock CreateSuperblock();

    //!
    //! \brief Push a new superblock into the tally.
    //!
    //! \param superblock Contains the superblock data to load.
    //!
    static void PushSuperblock(SuperblockPtr superblock);

    //!
    //! \brief Drop the last superblock loaded into the tally.
    //!
    //! \param pindex Represents the block that contains the superblock to drop.
    //!
    static void PopSuperblock(const CBlockIndex* const pindex);

    //!
    //! \brief Activate the superblock received at or below the specified
    //! height.
    //!
    //! \param height The maximum height to activate superblocks up to.
    //!
    //! \return \c true if a superblock at or below the specified height was
    //! activated.
    //!
    static bool CommitSuperblock(const uint32_t height);
};
}
