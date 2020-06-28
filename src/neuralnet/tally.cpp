#include "main.h"
#include "neuralnet/accrual/newbie.h"
#include "neuralnet/accrual/null.h"
#include "neuralnet/accrual/research_age.h"
#include "neuralnet/accrual/snapshot.h"
#include "neuralnet/claim.h"
#include "neuralnet/cpid.h"
#include "neuralnet/quorum.h"
#include "neuralnet/superblock.h"
#include "neuralnet/tally.h"
#include "util.h"

#include <unordered_map>

using namespace NN;
using LogFlags = BCLog::LogFlags;

namespace {
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
        pindex->SetMiningId(claim->m_mining_id);

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
            if (pindex->nHeight + 1 == GetV11Threshold()) {
                // This will finish loading the research accounting context
                // for snapshot accrual (block version 11+):
                return ActivateSnapshotAccrual(pindex, current_superblock);
            }

            if (pindex->nResearchSubsidy <= 0) {
                continue;
            }

            if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
                if (cpid->IsZero()) {
                    RepairZeroCpidIndex(pindex);
                }

                RecordRewardBlock(*cpid, pindex);
            }
        }

        return true;
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

        account.m_total_research_subsidy += pindex->nResearchSubsidy;

        if (pindex->nMagnitude > 0) {
            account.m_accuracy++;
            account.m_total_magnitude += pindex->nMagnitude;
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

        if (pindex == account.m_first_block_ptr) {
            m_researchers.erase(iter);
            return;
        }

        account.m_total_research_subsidy -= pindex->nResearchSubsidy;

        if (pindex->nMagnitude > 0) {
            account.m_accuracy--;
            account.m_total_magnitude -= pindex->nMagnitude;
        }

        pindex = pindex->pprev;

        while (pindex
            && (pindex->nResearchSubsidy <= 0 || pindex->GetMiningId() != cpid))
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
    //! \return \c false if an IO error occured while processing the superblock.
    //!
    bool ApplySuperblock(SuperblockPtr superblock)
    {
        // The network publishes version 2+ superblocks after the mandatory
        // switch to block version 11.
        //
        if (superblock->m_version >= 2) {
            TallySuperblockAccrual(superblock.m_timestamp);

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
    //! \return \c false if an IO error occured while processing the superblock.
    //!
    bool RevertSuperblock(SuperblockPtr superblock)
    {
        if (m_current_superblock->m_version >= 2) {
            try {
                return m_snapshots.Drop(m_current_superblock.m_height)
                    && m_snapshots.ApplyLatest(m_researchers);
            } catch (const SnapshotStateError& e) {
                LogPrintf("%s: %s", e.what());

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
                if (pindex->nIsSuperBlock == 1) {
                    m_snapshots.AssertMatch(pindex->nHeight);
                }

                if (pindex->nResearchSubsidy <= 0) {
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
            const int32_t threshold = GetResearchAgeThreshold();
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
    //! arrived.
    //!
    //! \param payment_time Time of payment to calculate rewards at.
    //!
    void TallySuperblockAccrual(const int64_t payment_time)
    {
        const SnapshotCalculator calc(payment_time, m_current_superblock);

        for (auto& account_pair : m_researchers) {
            const Cpid cpid = account_pair.first;
            ResearchAccount& account = account_pair.second;

            if (account.LastRewardHeight() >= m_current_superblock.m_height) {
                account.m_accrual = 0;
            }

            account.m_accrual += calc.AccrualDelta(cpid, account);
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
            if (pindex->nIsSuperBlock != 1) {
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
            if (pindex->nIsSuperBlock == 1) {
                if (!ApplySuperblock(SuperblockPtr::ReadFromDisk(pindex))) {
                    return false;
                }
            }

            if (pindex->nResearchSubsidy > 0) {
                if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
                    RecordRewardBlock(*cpid, pindex);
                }
            }
        }

        return true;
    }

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
        return true;
    }

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

int64_t Tally::MaxEmission(const int64_t payment_time)
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

int64_t Tally::GetAccrual(
    const Cpid cpid,
    const int64_t payment_time,
    const CBlockIndex* const last_block_ptr)
{
    return GetComputer(cpid, payment_time, last_block_ptr)->Accrual();
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
    if (!pindex || pindex->nResearchSubsidy <= 0) {
        return;
    }

    if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
        g_researcher_tally.RecordRewardBlock(*cpid, pindex);
    }
}

void Tally::ForgetRewardBlock(const CBlockIndex* const pindex)
{
    if (!pindex || pindex->nResearchSubsidy <= 0) {
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

    int64_t total_research_subsidy = 0;

    while (pindex->nHeight > min_depth) {
        if (!pindex->pprev) {
            return;
        }

        pindex = pindex->pprev;

        total_research_subsidy += pindex->nResearchSubsidy;
    }

    g_network_tally.Reset(total_research_subsidy);
}
