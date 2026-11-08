// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TALLY_H
#define GRIDCOIN_TALLY_H

#include "amount.h"
#include "gridcoin/account.h"
#include "gridcoin/accrual/computer.h"
#include "sync.h"

#include <optional>
#include <string>

class CBlockIndex;
extern CCriticalSection cs_main;

namespace GRC {

class Cpid;
class SuperblockPtr;

//!
//! \brief The core Gridcoin tally system that processes magnitudes and reward
//! data from the blockchain to calculate earned research reward amounts.
//!
//! This class is designed as a facade that provides an interface for the rest
//! of the application. It consumes information from newly-connected blocks to
//! build a database of research reward context for each CPID in the network.
//!
//! THREAD SAFETY: This tally system interacts closely with pointers to blocks
//! in the chain index. Always lock cs_main before calling its methods.
//!
class Tally
{
public:
    //!
    //! \brief Initialize the research reward context for each CPID in the
    //! network.
    //!
    //! This scans historical block metadata to create an in-memory database of
    //! the pending accrual owed to each CPID in the network.
    //!
    //! \param pindex Index for the first research age block.
    //!
    //! \return \c true if the tally initialized without an error.
    //!
    static bool Initialize(CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Switch from legacy research age accrual calculations to the
    //! superblock snapshot accrual system.
    //!
    //! \param pindex Index of the first block to enable snapshot accrual for.
    //!
    //! \return \c false if the snapshot system failed to initialize because of
    //! an error.
    //!
    static bool ActivateSnapshotAccrual(const CBlockIndex* const pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /*
    //!
    //! \brief Activate the fix for an issue that prevents new CPIDs from
    //! accruing research rewards earlier than the latest superblock.
    //!
    //! \return \c false if an error occurs while resetting the snapshot system.
    //!
    static bool FixNewbieSnapshotAccrual();
    */

    //!
    //! \brief Check whether the height of the specified block matches the
    //! tally granularity.
    //!
    //! \param height The block height to check for.
    //!
    //! \return \c true if the block height should trigger a recount.
    //!
    static bool IsLegacyTrigger(const uint64_t height);

    //!
    //! \brief Find the previous tally trigger below the specified block.
    //!
    //! \param pindex Index of the block to start from.
    //!
    //! \return Index of the first tally trigger block.
    //!
    static CBlockIndex* FindLegacyTrigger(CBlockIndex* pindex);

    //!
    //! \brief Get the maximum network-wide research reward amount per day.
    //!
    //! \param payment_time Determines the max reward based on a schedule.
    //!
    //! \return Maximum daily emission in units of 1/100000000 GRC.
    //!
    static CAmount MaxEmission(const int64_t payment_time);

    //!
    //! \brief Get the current network magnitude unit.
    //!
    //! \param pindex Block context to calculate the magnitude unit for.
    //!
    //! \return Current magnitude unit adjusted for the specified block.
    //!
    static double GetMagnitudeUnit(CBlockIndex * const pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Get a traversable object for the research accounts stored in
    //! the tally.
    //!
    //! \return Provides range-based iteration over every research account.
    //!
    static ResearchAccountRange Accounts();

    //!
    //! \brief Get the research account for the specified CPID.
    //!
    //! \param cpid The CPID of the account to fetch.
    //!
    //! \return An account that matches the CPID or a blank account if no
    //! research reward data exists for the CPID.
    //!
    static const ResearchAccount& GetAccount(const Cpid cpid);

    //!
    //! \brief Calculate the research reward accrual for the specified CPID.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param last_block_ptr Refers to the block for the reward.
    //!
    //! \return Research reward accrual in units of 1/100000000 GRC.
    //!
    static CAmount GetAccrual(
        const Cpid cpid,
        const int64_t payment_time,
        const CBlockIndex* const last_block_ptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief A value of the accrual that is near the MaxReward for the accrual computer in context based on
    //! the rate of accrual. This is defined in the implementation of the virtual method NearRewardLimit()
    //! in IAccrualComputer.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param last_block_ptr Refers to the block for the reward.
    //!
    //! \return CAmount of account accrual that is near the MaxReward.
    //!
    static CAmount AccrualNearLimit(
            const Cpid cpid,
            const int64_t payment_time,
            const CBlockIndex* const last_block_ptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Compute "catch-up" accrual to correct for newbie accrual bug.
    //!
    //! \param cpid for which to calculate the accrual correction.
    //! \param current_superblock that is the high point of the accrual correction.
    //! \param pindex_start Block to rewind from to the current superblock: a
    //! descendant of it on the same chain. Null starts at the chain tip.
    //!
    static CAmount GetNewbieSuperblockAccrualCorrection(
        const Cpid& cpid,
        const SuperblockPtr& current_superblock,
        const CBlockIndex* const pindex_start = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Get an initialized research reward accrual calculator.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param last_block_ptr Refers to the block for the reward.
    //!
    //! \return An accrual calculator initialized with the supplied parameters.
    //!
    static AccrualComputer GetComputer(
        const Cpid cpid,
        const int64_t payment_time,
        const CBlockIndex* const last_block_ptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! \brief Get an accrual computer instance that calculates accrual using
    //! delta snapshot rules.
    //!
    //! CONSENSUS: This method is exposed for RPC test commands used to analyze
    //! new accrual implementations. Do not use it to calculate accrual for the
    //! protocol directly.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param account        CPID's corresponding historical accrual context.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param last_block_ptr Refers to the block for the reward.
    //! \param superblock     Superblock at the beginning of a snapshot window.
    //!
    //! \return An accrual calculator initialized with the supplied parameters.
    //!
    static AccrualComputer GetSnapshotComputer(
        const Cpid cpid,
        const ResearchAccount& account,
        const int64_t payment_time,
        const CBlockIndex* const last_block_ptr,
        const SuperblockPtr superblock) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Get an accrual computer instance that calculates accrual using
    //! delta snapshot rules for the current superblock.
    //!
    //! CONSENSUS: This method is exposed for RPC test commands used to analyze
    //! new accrual implementations. Do not use it to calculate accrual for the
    //! protocol directly.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param last_block_ptr Refers to the block for the reward.
    //!
    //! \return An accrual calculator initialized with the supplied parameters.
    //!
    static AccrualComputer GetSnapshotComputer(
        const Cpid cpid,
        const int64_t payment_time,
        const CBlockIndex* const last_block_ptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Get an accrual computer instance that calculates accrual using
    //! legacy research age rules.
    //!
    //! CONSENSUS: This method is exposed for RPC test commands used to analyze
    //! new accrual implementations. Do not use it to calculate accrual for the
    //! protocol directly.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param last_block_ptr Refers to the block for the reward.
    //!
    //! \return An accrual calculator initialized with the supplied parameters.
    //!
    static AccrualComputer GetLegacyComputer(
        const Cpid cpid,
        const int64_t payment_time,
        const CBlockIndex* const last_block_ptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //!
    //! \brief Record a block's research reward data in the tally.
    //!
    //! \param pindex Contains information about the block to record.
    //!
    static void RecordRewardBlock(const CBlockIndex* const pindex);

    //!
    //! \brief Disassociate a blocks research reward data from the tally.
    //!
    //! \param pindex Contains information about the block to erase.
    //!
    static void ForgetRewardBlock(const CBlockIndex* const pindex);

    //!
    //! \brief Update the account data with information from a new superblock.
    //!
    //! \param superblock Refers to the current active superblock.
    //! \param pindex     Block that contains the superblock.
    //!
    //! \return \c false if an IO error occurred while processing the superblock.
    //!
    static bool ApplySuperblock(SuperblockPtr superblock, const CBlockIndex* const pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Reset the account data to a state before the provided superblock.
    //!
    //! \return \c false if an IO error occurred while processing the superblock.
    //!
    static bool RevertSuperblock() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Recount the two-week network averages.
    //!
    //! This method scans backwards from the specified height to rebuild a two-
    //! week average of the research rewards earned by network participants. It
    //! sets this average as the basis for calculating upcoming rewards through
    //! a value represented as the network magnitude unit.
    //!
    //! \param pindex Index of the block to start recounting backward from.
    //!
    static void LegacyRecount(const CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Return the baseline snapshot height for the tally.
    //!
    const static CBlockIndex* GetBaseline();

    //!
    //! \brief This closes the underlying register file of the researcher repository. It
    //! is ONLY used in Shutdown() to release the lock on the registry.dat file
    //! so that a snapshot download process cleanup will succeed, since the accrual directory
    //! needs to be removed.
    //!
    static void CloseRegistryFile();

    //!
    //! \brief Creates an account.
    //!
    //! \param cpid Cpid of the account.
    //!
    //! \return The account being created or an existing account with the same cpid.
    //!
    static ResearchAccount& CreateAccount(const Cpid& cpid);

    //!
    //! \brief Removes an account.
    //!
    //! \param cpid Cpid of the account.
    //!
    //! \return \c false if the cpid does not have an account.
    //!
    static bool RemoveAccount(const Cpid& cpid);

    //!
    //! \brief The reward blocks that a VerifyRewardBlocks() check that does not
    //! pass found.
    //!
    struct RewardBlocks
    {
        const CBlockIndex* m_recorded; //!< The account's last reward block, or null.
        const CBlockIndex* m_expected; //!< The last reward block the chain implies, or null.
        const CBlockIndex* m_paid_in;  //!< The block that pays that reward, or null.
        CAmount m_amount;              //!< The research reward m_paid_in pays the CPID.
        bool m_mrc;                    //!< Whether m_paid_in pays it through an MRC.

        //!
        //! \brief Terse key=value form for the log.
        //!
        std::string ToString() const;
    };

    //!
    //! \brief Check a CPID's last reward block in the tally against its reward
    //! blocks on the chain that ends at the tip.
    //!
    //! The tally records a research reward at the block that pays it, and an
    //! MRC at the parent of the block that pays it, the MRC's accrual point,
    //! after that block's own reward. The chain therefore implies a last reward
    //! block: the highest of these. This walks back from the tip through the
    //! block index, down to the account's last reward block, or to the current
    //! superblock for an account that holds none.
    //!
    //! \param cpid The CPID to check.
    //! \param tip  The chain tip, whose state the tally holds.
    //!
    //! \return Nothing when the check passes, otherwise the reward blocks found.
    //!
    static std::optional<RewardBlocks> VerifyRewardBlocks(const Cpid& cpid, const CBlockIndex* const tip)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Ask for a rebuild of the accrual snapshots, which the scheduled
    //! job runs through RunRequestedAccrualRebuild().
    //!
    //! \param details What the check found, for the log line of the rebuild.
    //!
    static void RequestAccrualRebuild(const std::string& details) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //!
    //! \brief Whether a rebuild of the accrual snapshots is waiting to run.
    //!
    static bool AccrualRebuildRequested();

    //!
    //! \brief Rebuild the accrual snapshots if a rebuild was asked for, then
    //! run the check again for the CPID.
    //!
    //! If that check does not pass, research reward claims stay paused until
    //! the process restarts: a request is honored once per run.
    //!
    //! \param cpid The wallet's CPID, if any, to check after the rebuild.
    //!
    //! \return \c false if the rebuild failed.
    //!
    static bool RunRequestedAccrualRebuild(const CpidOption& cpid) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
};
}

#endif // GRIDCOIN_TALLY_H
