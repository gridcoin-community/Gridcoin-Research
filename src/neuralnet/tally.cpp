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
            if (!m_snapshots.Store(superblock.m_height, m_researchers)) {
                return false;
            }

            TallySuperblockAccrual(superblock.m_timestamp);
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
            return m_snapshots.ApplyLatest(m_researchers)
                && m_snapshots.Drop(m_current_superblock.m_height);
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
        const CBlockIndex* const pindex,
        const SuperblockPtr superblock)
    {
        // Someone might run one of the snapshot accrual testing RPCs before
        // the chain is synchronized. We allow this after passing version 10
        // for testing, but it won't actually apply to accrual until version
        // 11 blocks arrive.
        //
        if (!pindex || !IsV10Enabled(pindex->nHeight)) {
            return true;
        }

        m_current_superblock = superblock;

        // If normal initialization is not successful, reinitialize, which
        // deletes everything in the accrual directory. If the
        // reinitialization fails, then something is seriously wrong, return false.
        // If the reinitialization succeeds, this will end up doing a rebuild
        // below.
        if (!m_snapshots.Initialize()) {
            if(!m_snapshots.Reinitialize()) return false;
        }

        // If the node initialized the snapshot accrual system before, we
        // should already have the latest snapshot. If the snapshot ApplyLatest
        // passes the integrity check, we are good, if not, force a rebuild.
        if (m_snapshots.HasBaseline()) {
            bool result = m_snapshots.ApplyLatest(m_researchers);

            if (!result) {
                LogPrint(LogFlags::TALLY, "INFO: TALLY: ApplyLatest snapshot failed. Rebuilding baseline.");
            } else {
                return (result);
            }
        } else {
            // If we are here, there has never been a baseline done. So do the same as a rebuild...

            LogPrint(LogFlags::TALLY, "INFO: ResearcherTally::ActivateSnapshotAccrual: There is no snapshot baseline.");
        }

        SnapshotBaselineBuilder builder(m_researchers);

        if (!builder.Run(pindex, superblock)) {
            return false;
        }

        return m_snapshots.StoreBaseline(superblock.m_height, m_researchers);
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
    //! \brief A link to the current active superblock used to lazily update
    //! researcher magnitudes when supplying an account.
    //!
    SuperblockPtr m_current_superblock = SuperblockPtr::Empty();

    //!
    //! \brief Manages snapshots for delta accrual calculations (version 2+
    //! superblocks).
    //!
    AccrualSnapshotRepository m_snapshots;

    //!
    //! \brief Tally research rewards accrued since the current superblock
    //! arrived.
    //!
    //! \param payment_time Time of payment to calculate rewards at.
    //!
    void TallySuperblockAccrual(const int64_t payment_time)
    {
        const SnapshotCalculator calc(payment_time, m_current_superblock);

        for (const auto& cpid_pair : m_current_superblock->m_cpids) {
            ResearchAccount& account = m_researchers[cpid_pair.Cpid()];

            if (account.LastRewardHeight() >= m_current_superblock.m_height) {
                account.m_accrual = 0;
            }

            account.m_accrual += calc.AccrualDelta(cpid_pair.Cpid(), account);
        }
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

    LogPrintf("Initializing research reward tally...");

    const int64_t start_time = GetTimeMillis();

    for (; pindex; pindex = pindex->pnext) {
        if (pindex->nHeight + 1 == GetV11Threshold()) {
            ActivateSnapshotAccrual(pindex);
        }

        if (pindex->nResearchSubsidy <= 0) {
            continue;
        }

        if (const CpidOption cpid = pindex->GetMiningId().TryCpid()) {
            if (cpid->IsZero()) {
                RepairZeroCpidIndex(pindex);
            }

            g_researcher_tally.RecordRewardBlock(*cpid, pindex);
        }
    }

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
