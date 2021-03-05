// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "gridcoin/account.h"
#include "gridcoin/accrual/computer.h"

class CBlockIndex;

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
    static bool Initialize(CBlockIndex* pindex);

    //!
    //! \brief Switch from legacy research age accrual calculations to the
    //! superblock snapshot accrual system.
    //!
    //! \param pindex Index of the first block to enable snapshot accrual for.
    //!
    //! \return \c false if the snapshot system failed to initialize because of
    //! an error.
    //!
    static bool ActivateSnapshotAccrual(const CBlockIndex* const pindex);

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
    static double GetMagnitudeUnit(const CBlockIndex* const pindex);

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
        const CBlockIndex* const last_block_ptr);

    //!
    //! \brief Compute "catch-up" accrual to correct for newbie accrual bug.
    //!
    //! \param cpid for which to calculate the accrual correction.
    //!
    static CAmount GetNewbieSuperblockAccrualCorrection(const Cpid& cpid, const SuperblockPtr& current_superblock);

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
        const CBlockIndex* const last_block_ptr);

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
        const SuperblockPtr superblock);

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
        const CBlockIndex* const last_block_ptr);

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
        const CBlockIndex* const last_block_ptr);

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
    //!
    //! \return \c false if an IO error occurred while processing the superblock.
    //!
    static bool ApplySuperblock(SuperblockPtr superblock);

    //!
    //! \brief Reset the account data to a state before the provided superblock.
    //!
    //! \return \c false if an IO error occurred while processing the superblock.
    //!
    static bool RevertSuperblock();

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
    static void LegacyRecount(const CBlockIndex* pindex);

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

};
}
