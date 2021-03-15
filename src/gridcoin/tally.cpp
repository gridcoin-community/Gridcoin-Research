// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "gridcoin/accrual/newbie.h"
#include "gridcoin/accrual/null.h"
#include "gridcoin/accrual/research_age.h"
#include "gridcoin/accrual/snapshot.h"
#include "gridcoin/claim.h"
#include "gridcoin/cpid.h"
#include "gridcoin/quorum.h"
#include "gridcoin/superblock.h"
#include "gridcoin/tally.h"
#include "util.h"

#include <unordered_map>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

extern int64_t g_v11_timestamp;

namespace {
/*
//!
//! \brief Determines whether the snapshot accrual system should enable the fix
//! for an issue that prevents new CPIDs from accruing research rewards.
//!
//! The snapshot accrual system released with mandatory v5.0.0 contained a bug
//! that prevented a CPID from accruing research rewards earlier than the last
//! superblock if that CPID never staked a block before. The fix causes a hard
//! fork so this flag controls the activation based on block height.
//!
//! This fix is temporary and can be removed after the next mandatory release.
//!
bool g_newbie_snapshot_fix_enabled;
*/

//!
//! \brief Set the correct CPID from the block claim when the block index
//! contains a zero CPID.
//!
//! There were reports of 0000 cpid in index where INVESTOR should have been.
//!
//! \param pindex Index of the block to repair.
//!
void RepairZeroCpidIndex(CBlockIndex* const pindex)
{
    const ClaimOption claim = GetClaimByIndex(pindex);

    if (!claim) {
        return;
    }

    if (claim->m_mining_id != pindex->GetMiningId())
    {
        LogPrint(LogFlags::TALLY,
            "WARNING: BlockIndex CPID %s did not match %s in block {%s %d}",
            pindex->GetMiningId().ToString(),
            claim->m_mining_id.ToString(),
            pindex->GetBlockHash().GetHex(),
            pindex->nHeight);

        /* Repair the cpid field */
        pindex->SetResearcherContext(
            claim->m_mining_id,
            pindex->ResearchSubsidy(),
            pindex->Magnitude());

#if 0
        if(!WriteBlockIndex(CDiskBlockIndex(pindex)))
            error("LoadBlockIndex: writing CDiskBlockIndex failed");
#endif
    }
}

//!
//! \brief Contains the two-week network average tally used to produce the
//! magnitude unit for legacy research age reward calculations (version 10
//! blocks and below).
//!
//! Before block version 11, the network used a feedback filter to limit the
//! total research reward generated over a rolling two week window. To scale
//! rewards accordingly, the magnitude unit acts as a multiplier and changes
//! in response to the amount of research rewards minted over the two weeks.
//! This class holds state for the parameters that produce a magnitude units
//! at a point in time.
//!
class NetworkTally
{
public:
    //!
    //! \brief Number of days that the tally scans backward from to calculate
    //! the average network payment.
    //!
    static constexpr size_t TALLY_DAYS = 14;

    //!
    //! \brief Get the maximum network-wide research reward amount per day.
    //!
    //! \param payment_time Determines the max reward based on a schedule.
    //!
    //! \return Maximum daily emission in units of GRC (not COIN).
    //!
    static double MaxEmission(const int64_t payment_time)
    {
        return BLOCKS_PER_DAY * GetMaxResearchSubsidy(payment_time);
    }

    //!
    //! \brief Get the current network-wide magnitude unit.
    //!
    //! \param payment_time Determines the max reward based on a schedule.
    //!
    //! \return Magnitude unit based on the current network averages.
    //!
    double GetMagnitudeUnit(const int64_t payment_time) const
    {
        double max_emission = MaxEmission(payment_time);
        max_emission -= m_two_week_research_subsidy / TALLY_DAYS;

        if (max_emission < 1) max_emission = 1;

        const double magnitude_unit = (max_emission / m_total_magnitude) * 1.25;

        // Just in case we lose a superblock or something strange happens:
        if (magnitude_unit > 5) {
            return 5.0;
        }

        return SnapToGrid(magnitude_unit);
    }

    //!
    //! \brief Reset the two-week averages to the provided values.
    //!
    //! \param research_subsidy Sum of research payments over the last two weeks.
    //!
    void Reset(double research_subsidy)
    {
        m_two_week_research_subsidy = research_subsidy / COIN;
    }

    //!
    //! \brief Load the total network magnitude from the provided superblock.
    //!
    //! \param superblock Provides the total network magnitude for subsequent
    //! magnitude unit calculations.
    //!
    void ApplySuperblock(const SuperblockPtr superblock)
    {
        LogPrint(LogFlags::TALLY,
            "NetworkTally::ApplySuperblock(%" PRId64 ")", superblock.m_height);

        m_total_magnitude = superblock->m_cpids.TotalMagnitude();
    }

private:
    uint32_t m_total_magnitude = 0;         //!< Sum of the magnitude of all CPIDs.
    double m_two_week_research_subsidy = 0; //!< Sum of research subsidy payments.

    //!
    //! \brief Round a magnitude unit value to intervals of 0.025.
    //!
    static double SnapToGrid(double d)
    {
        double dDither = .04;
        double dOut = RoundFromString(RoundToString(d * dDither, 3), 3) / dDither;
        return dOut;
    }
}; // NetworkTally

//!
//! \brief Tracks research payments for each CPID in the network.
//!
class ResearcherTally
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
    //! \param current_superblock Used to bootstrap snapshot accrual.
    //!
    //! \return \c true if the tally initialized without an error.
    //!
    bool Initialize(CBlockIndex* pindex, SuperblockPtr current_superblock)
    {
        LogPrintf("Initializing research reward tally...");

        m_start_pindex = pindex;

        for (; pindex; pindex = pindex->pnext) {
            if (pindex->nHeight + 1 == Params().GetConsensus().BlockV11Height) {
                // Set the timestamp for the block version 11 threshold. This
                // is temporary. Remove this variable in a release that comes
                // after the hard fork. For now, this is the least cumbersome
                // place to set the value:
                //
                g_v11_timestamp = pindex->nTime;

                // This will finish loading the research accounting context
                // for snapshot accrual (block version 11+):
                return ActivateSnapshotAccrual(pindex, current_superblock);
            }

            if (pindex->ResearchSubsidy() <= 0) {
                continue;
            }

            if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
                if (cpid->IsZero()) {
                    RepairZeroCpidIndex(pindex);
                }

                RecordRewardBlock(*cpid, pindex);
            }
        }

        // If this function does not return from the loop above to activate
        // snapshot accrual, the local blockchain data contains no snapshot
        // accrual blocks, so we erase any accrual snapshots that may exist
        // from a prior sync. This avoids issues when starting over without
        // any version 11 blocks (like a sync from the genesis block):
        //
        LogPrintf("Resetting accrual directory.");
        return m_snapshots.EraseAll();
    }

    //!
    //! \brief Get a traversable object for the research accounts stored in
    //! the tally.
    //!
    //! \return Provides range-based iteration over every research account.
    //!
    ResearchAccountRange Accounts()
    {
        return ResearchAccountRange(m_researchers);
    }

    //!
    //! \brief Get the research account for the specified CPID.
    //!
    //! \param cpid The CPID of the account to fetch.
    //!
    //! \return An account that matches the CPID or a blank account if no
    //! research reward data exists for the CPID.
    //!
    const ResearchAccount& GetAccount(const Cpid cpid)
    {
        auto iter = m_researchers.find(cpid);

        if (iter == m_researchers.end()) {
            return m_new_account;
        }

        return iter->second;
    }

    //!
    //! \brief Record a block's research reward data in the tally.
    //!
    //! \param cpid   The CPID of the research account to record the block for.
    //! \param pindex Contains information about the block to record.
    //!
    void RecordRewardBlock(const Cpid cpid, const CBlockIndex* const pindex)
    {
        ResearchAccount& account = m_researchers[cpid];

        account.m_total_research_subsidy += pindex->ResearchSubsidy();

        if (pindex->Magnitude() > 0) {
            account.m_accuracy++;
            account.m_total_magnitude += pindex->Magnitude();
        }

        if (account.m_first_block_ptr == nullptr) {
            account.m_first_block_ptr = pindex;
            account.m_last_block_ptr = pindex;
        } else {
            assert(pindex->nHeight > account.m_last_block_ptr->nHeight);
            account.m_last_block_ptr = pindex;
        }
    }

    //!
    //! \brief Disassociate a block's research reward data from the tally.
    //!
    //! \param cpid   The CPID of the research account to drop the block for.
    //! \param pindex Contains information about the block to erase.
    //!
    void ForgetRewardBlock(const Cpid cpid, const CBlockIndex* pindex)
    {
        auto iter = m_researchers.find(cpid);

        assert(iter != m_researchers.end());

        ResearchAccount& account = iter->second;

        assert(account.m_first_block_ptr != nullptr);
        assert(pindex == account.m_last_block_ptr);

        // When disconnecting a CPID's first block, reset the account, but
        // retain the pending snapshot accrual amount:
        //
        if (pindex == account.m_first_block_ptr) {
            account = ResearchAccount(account.m_accrual);
            return;
        }

        account.m_total_research_subsidy -= pindex->ResearchSubsidy();

        if (pindex->Magnitude() > 0) {
            account.m_accuracy--;
            account.m_total_magnitude -= pindex->Magnitude();
        }

        pindex = pindex->pprev;

        while (pindex
            && (pindex->ResearchSubsidy() <= 0 || pindex->GetMiningId() != cpid))
        {
            pindex = pindex->pprev;
        }

        assert(pindex && pindex != pindexGenesisBlock);

        account.m_last_block_ptr = pindex;
    }

    //!
    //! \brief Update the account data with information from a new superblock.
    //!
    //! \param superblock Refers to the current active superblock.
    //!
    //! \return \c false if an IO error occurred while processing the superblock.
    //!
    bool ApplySuperblock(SuperblockPtr superblock)
    {
        // The network publishes version 2+ superblocks after the mandatory
        // switch to block version 11.
        //
        if (superblock->m_version >= 2) {
            TallySuperblockAccrual(superblock);

            if (!m_snapshots.Store(superblock.m_height, m_researchers)) {
                return false;
            }
        }

        m_current_superblock = std::move(superblock);

        return true;
    }

    //!
    //! \brief Reset the account data to a state before the current superblock.
    //!
    //! \param superblock Refers to the current active superblock (before the
    //! reverted superblock).
    //!
    //! \return \c false if an IO error occurred while processing the superblock.
    //!
    bool RevertSuperblock(SuperblockPtr superblock)
    {
        if (m_current_superblock->m_version >= 2) {
            try {
                if (!m_snapshots.Drop(m_current_superblock.m_height)
                    || !m_snapshots.ApplyLatest(m_researchers))
                {
                    return false;
                }
            } catch (const SnapshotStateError& e) {
                LogPrintf("%s: %s", __func__, e.what());

                return RebuildAccrualSnapshots();
            }
        }

        m_current_superblock = std::move(superblock);

        return true;
    }

    //!
    //! \brief Switch from legacy research age accrual calculations to the
    //! superblock snapshot accrual system.
    //!
    //! \param pindex     Index of the block to enable snapshot accrual for.
    //! \param superblock Refers to the current active superblock.
    //!
    //! \return \c false if the snapshot system failed to initialize because of
    //! an error.
    //!
    bool ActivateSnapshotAccrual(
        const CBlockIndex* const baseline_pindex,
        SuperblockPtr superblock)
    {
        if (!baseline_pindex || !IsV11Enabled(baseline_pindex->nHeight + 1)) {
            return false;
        }

        m_snapshot_baseline_pindex = baseline_pindex;
        m_current_superblock = std::move(superblock);

        try {
            if (!m_snapshots.Initialize()) {
                return false;
            }

            // If the node initialized the snapshot accrual system before, we
            // should already have the latest snapshot. Otherwise, create the
            // baseline snapshot and scan context for the remaining blocks:
            //
            if (!m_snapshots.HasBaseline()) {
                return BuildAccrualSnapshots();
            }

            // Check the integrity of the baseline superblock snapshot:
            //
            if (const CBlockIndex* pindex = FindBaselineSuperblockHeight()) {
                m_snapshots.AssertMatch(pindex->nHeight);
            }

            // Finish loading the research account context for the remaining
            // blocks after the snapshot accrual threshold. Verify snapshots
            // along the way:
            //
            for (const CBlockIndex* pindex = baseline_pindex;
                pindex;
                pindex = pindex->pnext)
            {
                if (pindex->IsSuperblock()) {
                    m_snapshots.AssertMatch(pindex->nHeight);
                }

                if (pindex->ResearchSubsidy() <= 0) {
                    continue;
                }

                if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
                    RecordRewardBlock(*cpid, pindex);
                }
            }

            m_snapshots.PruneSnapshotFiles();

            return m_snapshots.ApplyLatest(m_researchers);
        } catch (const SnapshotStateError& e) {
            LogPrintf("%s: %s", __func__, e.what());
        }

        return RebuildAccrualSnapshots();
    }

    //!
    //! \brief Erase the snapshot files and clear the registry.
    //!
    //! \return \c false if the snapshots and registery deletion failed because
    //! of an error.
    //!
    bool EraseSnapshots()
    {
        return m_snapshots.EraseAll();
    }

    //!
    //! \brief Return the CBlockIndex pointer for the tally baseline.
    //!
    const CBlockIndex* GetBaseline()
    {
        if (m_snapshots.HasBaseline())
        {
            return m_snapshot_baseline_pindex;
        }
        else
        {
            return nullptr;
        }
    }

private:
    //!
    //! \brief An empty account to return as a reference when requesting an
    //! account for a CPID with no historical record.
    //!
    const ResearchAccount m_new_account;

    //!
    //! \brief The set of all research accounts in the network.
    //!
    ResearchAccountMap m_researchers;

    //!
    //! \brief A link to the current active superblock for snapshot accrual
    //! calculations.
    //!
    SuperblockPtr m_current_superblock = SuperblockPtr::Empty();

    //!
    //! \brief Manages snapshots for delta accrual calculations (version 2+
    //! superblocks).
    //!
    AccrualSnapshotRepository m_snapshots;

    //!
    //! \brief Index of the first block that the tally tracks research rewards
    //! for.
    //!
    const CBlockIndex* m_start_pindex = nullptr;

    //!
    //! \brief Points to the index of the block when snapshot accrual activates
    //! (the block just before protocol enforces snapshot accrual).
    //!
    const CBlockIndex* m_snapshot_baseline_pindex = nullptr;

    //!
    //! \brief Get the block index entry of the block when research accounting
    //! begins.
    //!
    CBlockIndex* GetStartHeight()
    {
        // A node syncing from zero does not know the block index entry of the
        // starting height yet, so the tally will initialize without it.
        //
        if (m_start_pindex == nullptr && pindexGenesisBlock != nullptr) {
            const int32_t threshold = Params().GetConsensus().ResearchAgeHeight;
            const CBlockIndex* pindex = pindexGenesisBlock;

            for (; pindex && pindex->nHeight < threshold; pindex = pindex->pnext);

            m_start_pindex = pindex;
        }

        // Tally initialization will repair block index entries with zero CPID
        // values to workaround an old bug so we remove the const specifier on
        // this pointer. Besides this, the tally will never mutate block index
        // objects, so remove the const_cast after deprecating the repair:
        //
        return const_cast<CBlockIndex*>(m_start_pindex);
    }

    //!
    //! \brief Tally research rewards accrued since the current superblock
    //! arrived for the snapshot accrual system.
    //!
    //! \param superblock Incoming superblock to calculate rewards at.
    //!
    void TallySuperblockAccrual(const SuperblockPtr& superblock)
    {
        const SnapshotCalculator calc(superblock.m_timestamp, m_current_superblock);

        LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: superblock height = %u, m_researchers.size() = %u",
                 __func__, superblock.m_height, m_researchers.size());

        for (auto& account_pair : m_researchers) {
            const Cpid cpid = account_pair.first;
            ResearchAccount& account = account_pair.second;

            if (account.LastRewardHeight() >= m_current_superblock.m_height) {
                account.m_accrual = 0;
            }

            account.m_accrual += calc.AccrualDelta(cpid, account);
        }


        // This is the broken newbie fix originally at height 2104000 that didn't work. I
        // am leaving it here commented out for documentation purposes. It will be removed
        // in a future release.
        /*
        // Versions 5.0.x for the mandatory block version 11 protocol hard-fork
        // contain a bug that prevented new CPIDs from accruing rewards earlier
        // than the latest superblock because the loop above does not reconcile
        // the pending accrual for CPIDs without a research account yet.
        //
        if (!g_newbie_snapshot_fix_enabled) {
            return;
        }

        // Record snapshot accrual for any CPIDs with no accounting record as
        // of the last superblock:
        //
        for (const auto& iter : superblock->m_cpids) {
            if (m_researchers.find(iter.Cpid()) == m_researchers.end()) {
                ResearchAccount& account = m_researchers[iter.Cpid()];
                account.m_accrual = calc.AccrualDelta(iter.Cpid(), account);

                LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: accrual account not found "
                         "for CPID %s. Creating account with accrual %" PRId64" from "
                         "AccrualDelta() = %" PRId64 ". m_researchers.size() now %u",
                         __func__,
                         iter.Cpid().ToString(),
                         account.m_accrual,
                         calc.AccrualDelta(iter.Cpid(), account),
                         m_researchers.size());
            }
        }
        */

        // The accrual calculations for the newbie fix are a problem that resides on superblock
        // boundaries. It is sufficient to include a simple test here to determine whether the
        // incoming superblock is at the fix height or above to activate the fix.
        if (superblock.m_height >= GetNewbieSnapshotFixHeight())
        {
            // Record catch-up (fix) snapshot accrual for any CPIDs with no accounting record as
            // of the last superblock. This is in two pieces for each CPID in the incoming
            // superblock (i.e. active) that does not have an account: 1. The
            // GetNewbieSuperblockAccrualCorrection which is the "catch-up" accrual, and 2.
            // the normal AccrualDelta, which is the period from the current superblock to the
            // incoming one.
            for (const auto& iter : superblock->m_cpids) {
                if (m_researchers.find(iter.Cpid()) == m_researchers.end()) {
                    ResearchAccount& account = m_researchers[iter.Cpid()];

                    CAmount accrual_correction = Tally::GetNewbieSuperblockAccrualCorrection(iter.Cpid(),
                                                                                             m_current_superblock);
                    CAmount accrual_delta = calc.AccrualDelta(iter.Cpid(), account);

                    account.m_accrual = accrual_correction + accrual_delta;

                    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: accrual account not found "
                             "for CPID %s. Creating account with accrual %" PRId64" from "
                             "Accrual Correction = %" PRId64 ", Accrual Delta = %" PRId64 ", "
                             "superblock height = %" PRId64 ", m_researchers.size() = %u.",
                             __func__,
                             iter.Cpid().ToString(),
                             account.m_accrual,
                             accrual_correction,
                             accrual_delta,
                             superblock.m_height,
                             m_researchers.size());
                }
            }
        }
    }


    //!
    //! \brief Locate the block index entry of the superblock before the
    //! snapshot accrual threshold.
    //!
    //! \return The block index entry of the superblock used to build the
    //! baseline snapshot.
    //!
    const CBlockIndex* FindBaselineSuperblockHeight() const
    {
        assert(m_snapshot_baseline_pindex != nullptr);

        for (const CBlockIndex* pindex = m_snapshot_baseline_pindex;
            pindex;
            pindex = pindex->pprev)
        {
            if (!pindex->IsSuperblock()) {
                continue;
            }

            return pindex;
        }

        return nullptr;
    }

    //!
    //! \brief Locate the superblock before the snapshot accrual threshold.
    //!
    //! \return The superblock used to build the baseline snapshot.
    //!
    SuperblockPtr FindBaselineSuperblock() const
    {
        const CBlockIndex* pindex = FindBaselineSuperblockHeight();

        return SuperblockPtr::ReadFromDisk(pindex);
    }

    //!
    //! \brief Reset the tally to the snapshot accrual baseline and store the
    //! baseline snapshot to disk.
    //!
    //! \return \c false if an error occurred.
    //!
    bool BuildBaselineSnapshot()
    {
        assert(m_snapshot_baseline_pindex != nullptr);

        SuperblockPtr superblock = FindBaselineSuperblock();

        if (!superblock->WellFormed()) {
            return error("%s: unable to load baseline superblock", __func__);
        }

        m_current_superblock = superblock;
        SnapshotBaselineBuilder builder(m_researchers);

        if (!builder.Run(m_snapshot_baseline_pindex, std::move(superblock))) {
            return false;
        }

        if (!m_snapshots.StoreBaseline(superblock.m_height, m_researchers)) {
            return false;
        }

        return true;
    }

    //!
    //! \brief Create the baseline accrual snapshot and any remaining snapshots
    //! above the snapshot accrual threshold.
    //!
    //! Because snapshot accrual depends on the last reward blocks in research
    //! accounts, this method expects that the tally's research accounts state
    //! exists as it would at the snapshot accrual threshold (the block before
    //! version 11 blocks begin). This method finishes importing any remaining
    //! research account data from the block index entries above the threshold
    //! as it scans the chain to create accrual snapshots.
    //!
    //! \return \c false if an IO error occurred.
    //!
    bool BuildAccrualSnapshots()
    {
        if (!BuildBaselineSnapshot()) {
            return false;
        }

        CBlock block;

        // Scan forward to the chain tip and reapply snapshot accrual for each
        // account while writing snapshot files for every superblock along the
        // way. Finish rescanning the research accounts:
        //
        for (const CBlockIndex* pindex = m_snapshot_baseline_pindex->pnext;
            pindex;
            pindex = pindex->pnext)
        {
            if (pindex->IsSuperblock()) {
                if (!ApplySuperblock(SuperblockPtr::ReadFromDisk(pindex))) {
                    return false;
                }
            }

            if (pindex->ResearchSubsidy() > 0) {
                if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
                    RecordRewardBlock(*cpid, pindex);
                }
            }
        }

        return true;
    }

public:
    //!
    //! \brief Wipe out the entire snapshot accrual state and rebuild the
    //! snapshots and each account's accrual from the initial threshold.
    //!
    //! This provides the ability to repair the snapshot accrual system in
    //! case of corruption or user error.
    //!
    //! TODO: Rebuilding the entire snapshot history is not necessary. Each
    //! snapshot effectively contains the rolled-up state of snapshots that
    //! exist before it, so we can store just the snapshots near the tip of
    //! the chain.
    //!
    //! \return \c false if an error occurred.
    //!
    bool RebuildAccrualSnapshots()
    {
        assert(m_snapshot_baseline_pindex != nullptr);

        // Reset the research accounts and reinitialize the whole tally. We
        // need to make sure that an account contains the last reward block
        // below the baseline:
        //
        m_researchers.clear();

        LogPrintf("%s: rebuilding from %" PRId64 " to %" PRId64 "...",
            __func__,
            m_snapshot_baseline_pindex->nHeight,
            pindexBest->nHeight);

        if (!m_snapshots.EraseAll()) {
            return false;
        }

        return Initialize(GetStartHeight(), SuperblockPtr::Empty());
    }

    bool CloseRegistryFile()
    {
        return m_snapshots.CloseRegistryFile();
    }
}; // ResearcherTally

ResearcherTally g_researcher_tally; //!< Tracks lifetime research rewards.
NetworkTally g_network_tally;       //!< Tracks legacy two-week network averages.

} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: Tally
// -----------------------------------------------------------------------------

bool Tally::Initialize(CBlockIndex* pindex)
{
    if (!pindex || !IsResearchAgeEnabled(pindex->nHeight)) {
        LogPrintf("Tally initialization not needed.");

        // Also destroy any existing accrual snapshots, because if this is called from
        // init below the research age enabled height, the accrual directory must be stale
        // (i.e. this is a resync.)

        g_researcher_tally.EraseSnapshots();
        LogPrintf("Accrual directory reset.");

        return true;
    }

    /* This is part of the original newbie accrual fix that is now disabled.
    g_newbie_snapshot_fix_enabled = pindex->nHeight + 1 >= GetNewbieSnapshotFixHeight();

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: pindex->nHeight + 1 = %i, GetNewbieSnapshotFixHeight() = %i, "
                                       "g_newbie_snapshot_fix_enabled = %i",
             __func__,
             pindex->nHeight + 1,
             GetNewbieSnapshotFixHeight(),
             g_newbie_snapshot_fix_enabled);

    */

    const int64_t start_time = GetTimeMillis();

    g_researcher_tally.Initialize(pindex, Quorum::CurrentSuperblock());

    LogPrintf(
        "Tally initialization complete. Scan time %15" PRId64 "ms\n",
        GetTimeMillis() - start_time);

    return true;
}

bool Tally::ActivateSnapshotAccrual(const CBlockIndex* const pindex)
{
    LogPrint(LogFlags::TALLY, "Activating snapshot accrual...");

    // Activate any pending superblocks that may exist before the snapshot
    // system kicks-in. The legacy tally caches superblocks in the pending
    // state for 10 blocks before the recount trigger height.
    //
    Quorum::CommitSuperblock(pindex->nHeight);

    return g_researcher_tally.ActivateSnapshotAccrual(
        pindex,
        Quorum::CurrentSuperblock());
}

/*
bool Tally::FixNewbieSnapshotAccrual()
{
    g_newbie_snapshot_fix_enabled = true;

    return g_researcher_tally.RebuildAccrualSnapshots();
}
*/

bool Tally::IsLegacyTrigger(const uint64_t height)
{
    return height % TALLY_GRANULARITY == 0;
}

CBlockIndex* Tally::FindLegacyTrigger(CBlockIndex* pindex)
{
    // Scan backwards until we find one where accepting it would
    // trigger a tally.
    for (;
        pindex && pindex->pprev && !IsLegacyTrigger(pindex->nHeight);
        pindex = pindex->pprev);

    return pindex;
}

CAmount Tally::MaxEmission(const int64_t payment_time)
{
    return NetworkTally::MaxEmission(payment_time) * COIN;
}

double Tally::GetMagnitudeUnit(const CBlockIndex* const pindex)
{
    if (pindex->nVersion >= 11) {
        return SnapshotCalculator::MagnitudeUnit();
    }

    return g_network_tally.GetMagnitudeUnit(pindex->nTime);
}

ResearchAccountRange Tally::Accounts()
{
    return g_researcher_tally.Accounts();
}

const ResearchAccount& Tally::GetAccount(const Cpid cpid)
{
    return g_researcher_tally.GetAccount(cpid);
}

CAmount Tally::GetAccrual(
    const Cpid cpid,
    const int64_t payment_time,
    const CBlockIndex* const last_block_ptr)
{
    return GetComputer(cpid, payment_time, last_block_ptr)->Accrual();
}

//!
//! \brief Compute "catch-up" accrual to correct for newbie accrual bug.
//!
//! \param cpid for which to calculate the accrual correction.
//! \param superblock that is the high point of the accrual correction
//!
CAmount Tally::GetNewbieSuperblockAccrualCorrection(const Cpid& cpid, const SuperblockPtr& current_superblock)
{
    // This function was moved from the anonymous namespace and private, to public and made static, because it has
    // to be called from ClaimValidator::CheckResearchReward() directly too. Why?

    // The broken original newbie fix was CONDITIONAL. The initialization in Tally::Initialize was broken and would
    // never activate, yet the alternate path through AcceptBlock AddToBlockIndex could actually set the original
    // global boolean if syncing and passing through the trigger block height (now memorialized in
    // GetOrigNewbieSnapshotFixHeight()). This led to the following fracture:

    // Nodes that synced from zero through the original newbie trigger block height would have the global boolean set to
    // activate the fix. On the other hand as time (height passes) after the trigger height, nodes would get restarted
    // and are already beyond that height, so their boolean would be set to zero. So if a node that hadn't been restarted
    // is a newbie and actually stakes with the fix enabled, it will have an accrual account on that node and report
    // the correct historical accrual to be paid, rather than the newbie truncated value prior to the fix. There are
    // actually some blocks that made it on the chain that way.

    // Rather than leave the original broken global boolean flag for the original newbie fix, which doesn't actually work
    // and would be confusing to code maintainers in the future, I decided to implement this correction function.

    // The function has two current uses:

    // 1. It is used in TallySuperblockAccrual() above to compute the "catch-up" correction on the acceptance of a staked
    //    superblock for any CPIDs that were subject to the newbie bug and have historical accruals that need to be
    //    included. This is activated at GetNewbieSnapshotFixHeight(), and will cause this function to be used to apply
    //    the corrections to any CPID's that do not have an accrual account at that height.
    //
    // 2. It is used in ConnectBlock (the ClaimValidator::CheckResearchReward()) to conditionally apply the accrual
    //    correction if the claimed value by the block fails the original CheckReward. If the original computed reward
    //    plus the correction equals the claimed reward, then the block is passed with a warning. This enables any
    //    node to conditionally validate a block that was staked with the accrual correction active, even if the
    //    receiving node does not have the NEW correction active.

    // This function computes the accrual that should have been recorded in the periods between the first superblock that
    // posted that validated the original verified beacon in a chain of renewals and the current superblock. This uses a
    // calculation very similar to the calculation in the auditsnapshotaccrual RPC function.

    // The reason I go through the trouble to limit the lookback scope to the chain of beacon renewals (including the
    // original advertisement that was validated) is to limit the lookback for this function, and it is proper to limit
    // the scope of the lookback accrual correction to be done only over the time-frame that is covered by an unbroken
    // beacon chain.

    // This function should be very light after the new newbie fix is crossed by the network (after the application of
    // the accrual corrections for all the CPID's in the first superblock past that height), since the AccrualDelta with
    // no additional corrections will be necessary (i.e. this function will generate no periods).

    CAmount accrual = 0;

    // This lambda is almost a straight lift from the auditsnapshotaccrual RPC function. It is simplifed,
    // because since the accrual account doesn't exist, there has been no staking for this CPID and no payout,
    // so only superblock to superblock periods need to be considered.
    const auto tally_accrual_period = [&](
        const int64_t low_time,
        const int64_t high_time,
        const GRC::Magnitude magnitude)
    {
        int64_t time_interval = high_time - low_time;

        int64_t abs_time_interval = time_interval;

        int sign = (time_interval >= 0) ? 1 : -1;

        if (sign < 0) {
            abs_time_interval = -time_interval;
        }

        // This is the same way that AccrualDelta calculates accruals in the snapshot calculator. Here
        // we use the absolute value of the time interval to ensure negative values are carried through
        // correctly in the bignumber calculations.
        const uint64_t base_accrual = abs_time_interval
            * magnitude.Scaled()
            * MAG_UNIT_NUMERATOR;

        int64_t period = 0;

        if (base_accrual > std::numeric_limits<uint64_t>::max() / COIN) {
            arith_uint256 accrual_bn(base_accrual);
            accrual_bn *= COIN;
            accrual_bn /= 86400;
            accrual_bn /= Magnitude::SCALE_FACTOR;
            accrual_bn /= MAG_UNIT_DENOMINATOR;

            period = accrual_bn.GetLow64() * (int64_t) sign;
        }
        else
        {
            period = base_accrual * (int64_t) sign
                    * COIN
                    / 86400
                    / Magnitude::SCALE_FACTOR
                    / MAG_UNIT_DENOMINATOR;
        }

        accrual += period;

        // TODO: Change this to refer to MaxReward() from the snapshot computer.
        int64_t max_reward = 16384 * COIN;

        if (accrual > max_reward)
        {
            int64_t overage = accrual - max_reward;
            // Cap accrual at max_reward;
            accrual = max_reward;
            // Remove overage from period, because you can't have a period accrual to over the max.
            period -= overage;
        }

        return period;
    };

    GRC::BeaconRegistry& beacons = GRC::GetBeaconRegistry();
    GRC::BeaconOption beacon = beacons.TryActive(cpid, current_superblock.m_timestamp);

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: beacon registry size = %u", __func__, beacons.Beacons().size());

    // Bail if there is no active beacon.
    if (!beacon)
    {
        LogPrint(BCLog::LogFlags::ACCRUAL, "ERROR: %s: No active beacon for cpid %s.",
                 __func__, cpid.ToString());

        return accrual;
    }

    Beacon_ptr beacon_ptr = beacon;

    // Walk back the entries in the historical beacon map linked by renewal prev tx hash until the first
    // beacon in the renewal chain is found (the original advertisement). The accrual starts no earlier
    // than here.
    while (beacon_ptr->Renewed())
    {
        beacon_ptr = std::make_shared<Beacon>(beacons.GetBeaconDB().find(beacon->m_prev_beacon_hash)->second);
    }

    const CBlockIndex* pindex_baseline = GRC::Tally::GetBaseline();

    // Start at the tip.
    const CBlockIndex* pindex_high = mapBlockIndex[hashBestChain];

    // Rewind pindex_high to the current superblock.
    while (pindex_high->nHeight > current_superblock.m_height)
    {
        pindex_high = pindex_high->pprev;
    }

    // Set pindex to the block before: (pindex_high->pprev).
    const CBlockIndex* pindex = pindex_high->pprev;

    SuperblockPtr superblock;
    unsigned int period_num = 0;

    while (pindex->nHeight >= pindex_baseline->nHeight)
    {
        if (pindex->IsSuperblock())
        {
            superblock = SuperblockPtr::ReadFromDisk(pindex);

            const GRC::Magnitude magnitude = superblock->m_cpids.MagnitudeOf(cpid);

            // Stop the accrual when we get to a superblock that is before the beacon advertisement.
            if (pindex->nTime < beacon_ptr->m_timestamp) break;

            CAmount period = tally_accrual_period(pindex->nTime, pindex_high->nTime, magnitude);

            LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: period_num = %u, "
                     "low height = %i, high height = %u, magnitude at low height SB = %f, "
                     "low time = %u, high time = %u, "
                     "accrual for period = %" PRId64 ", accrual = %" PRId64 ".",
                     __func__,
                     period_num,
                     pindex->nHeight,
                     pindex_high->nHeight,
                     magnitude.Floating(),
                     pindex->nTime,
                     pindex_high->nTime,
                     period,
                     accrual);

            // We are going backwards through the chain.
            pindex_high = pindex;
            ++period_num;
        }

        pindex = pindex->pprev;
    }

    return accrual;
}

AccrualComputer Tally::GetComputer(
    const Cpid cpid,
    const int64_t payment_time,
    const CBlockIndex* const last_block_ptr)
{
    if (!last_block_ptr) {
        return MakeUnique<NullAccrualComputer>();
    }

    if (last_block_ptr->nVersion >= 11) {
        return GetSnapshotComputer(cpid, payment_time, last_block_ptr);
    }

    return GetLegacyComputer(cpid, payment_time, last_block_ptr);
}

AccrualComputer Tally::GetSnapshotComputer(
    const Cpid cpid,
    const ResearchAccount& account,
    const int64_t payment_time,
    const CBlockIndex* const last_block_ptr,
    const SuperblockPtr superblock)
{
    return MakeUnique<SnapshotAccrualComputer>(
        cpid,
        account,
        payment_time,
        last_block_ptr->nHeight,
        std::move(superblock));
}

AccrualComputer Tally::GetSnapshotComputer(
    const Cpid cpid,
    const int64_t payment_time,
    const CBlockIndex* const last_block_ptr)
{
    return GetSnapshotComputer(
        cpid,
        GetAccount(cpid),
        payment_time,
        last_block_ptr,
        Quorum::CurrentSuperblock());
}

AccrualComputer Tally::GetLegacyComputer(
    const Cpid cpid,
    const int64_t payment_time,
    const CBlockIndex* const last_block_ptr)
{
    const ResearchAccount& account = GetAccount(cpid);

    if (!account.IsActive(last_block_ptr->nHeight)) {
        return MakeUnique<NewbieAccrualComputer>(
            cpid,
            account,
            payment_time,
            g_network_tally.GetMagnitudeUnit(payment_time),
            Quorum::CurrentSuperblock()->m_cpids.MagnitudeOf(cpid).Floating());
    }

    return MakeUnique<ResearchAgeComputer>(
        cpid,
        account,
        Quorum::CurrentSuperblock()->m_cpids.MagnitudeOf(cpid).Floating(),
        payment_time,
        g_network_tally.GetMagnitudeUnit(payment_time),
        last_block_ptr->nHeight);
}

void Tally::RecordRewardBlock(const CBlockIndex* const pindex)
{
    if (!pindex || pindex->ResearchSubsidy() <= 0) {
        return;
    }

    if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
        g_researcher_tally.RecordRewardBlock(*cpid, pindex);
    }
}

void Tally::ForgetRewardBlock(const CBlockIndex* const pindex)
{
    if (!pindex || pindex->ResearchSubsidy() <= 0) {
        return;
    }

    if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
        g_researcher_tally.ForgetRewardBlock(*cpid, pindex);
    }
}

bool Tally::ApplySuperblock(SuperblockPtr superblock)
{
    return g_researcher_tally.ApplySuperblock(std::move(superblock));
}

bool Tally::RevertSuperblock()
{
    return g_researcher_tally.RevertSuperblock(Quorum::CurrentSuperblock());
}

void Tally::LegacyRecount(const CBlockIndex* pindex)
{
    if (!pindex) {
        return;
    }

    LogPrint(LogFlags::TALLY, "Tally::LegacyRecount(%" PRId64 ")", pindex->nHeight);

    const int64_t consensus_depth = pindex->nHeight - CONSENSUS_LOOKBACK;
    const int64_t lookback_depth = BLOCKS_PER_DAY * NetworkTally::TALLY_DAYS;

    int64_t max_depth = consensus_depth - (consensus_depth % TALLY_GRANULARITY);
    int64_t min_depth = max_depth - lookback_depth;

    if (fTestNet && !IsV9Enabled_Tally(pindex->nHeight)) {
        LogPrint(LogFlags::TALLY, "Tally::LegacyRecount(): retired tally");
        max_depth = consensus_depth - (consensus_depth % BLOCK_GRANULARITY);
        min_depth -= (max_depth - lookback_depth) % TALLY_GRANULARITY;
    }

    // Seek to the head of the tally window:
    while (pindex->nHeight > max_depth) {
        if (!pindex->pprev) {
            return;
        }

        pindex = pindex->pprev;
    }

    if (Quorum::CommitSuperblock(max_depth)) {
        g_network_tally.ApplySuperblock(Quorum::CurrentSuperblock());
    }

    CAmount total_research_subsidy = 0;

    while (pindex->nHeight > min_depth) {
        if (!pindex->pprev) {
            return;
        }

        pindex = pindex->pprev;

        total_research_subsidy += pindex->ResearchSubsidy();
    }

    g_network_tally.Reset(total_research_subsidy);
}

const CBlockIndex* Tally::GetBaseline()
{
    return g_researcher_tally.GetBaseline();
}

void Tally::CloseRegistryFile()
{
    g_researcher_tally.CloseRegistryFile();
}
