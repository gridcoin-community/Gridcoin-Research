#include "main.h"
#include "neuralnet/accrual/newbie.h"
#include "neuralnet/accrual/null.h"
#include "neuralnet/accrual/research_age.h"
#include "neuralnet/cpid.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/tally.h"
#include "util.h"

#include <unordered_map>

using namespace NN;

namespace {
//!
//! \brief Number of days that the tally scans backward from to calculate
//! the average network payment.
//!
constexpr size_t TALLY_DAYS = 14;

//!
//! \brief Round a value to intervals of 0.025.
//!
//! Used to calculate the network magnitude unit.
//!
double SnapToGrid(double d)
{
    double dDither = .04;
    double dOut = RoundFromString(RoundToString(d * dDither, 3), 3) / dDither;
    return dOut;
}

//!
//! \brief Contains the two-week network average tally used to produce the
//! magnitude unit for research age reward calculations.
//!
class NetworkTally
{
public:
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
        LogPrintf("NetworkTally::ApplySuperblock(%" PRId64 ")", superblock->m_height);
        m_total_magnitude = superblock->m_cpids.TotalMagnitude();
    }

private:
    uint32_t m_total_magnitude = 0;         //!< Sum of the magnitude of all CPIDs.
    double m_two_week_research_subsidy = 0; //!< Sum of research subsidy payments.
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
        // Since research account magnitudes are applied lazily from the last
        // superblock, we need to update the magnitude of each account before
        // providing the range.
        //
        // TODO: move the magnitude update into a transforming iterator so we
        // can avoid the loop outside the range. This API is not used in core
        // code at the moment so the performance hit is negligible.
        //
        for (auto& account_pair : m_researchers) {
            const Cpid& cpid = account_pair.first;
            ResearchAccount& account = account_pair.second;

            account.m_magnitude = m_current_superblock->m_cpids.MagnitudeOf(cpid);
        }

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
        const uint16_t magnitude = m_current_superblock->m_cpids.MagnitudeOf(cpid);
        auto iter = m_researchers.find(cpid);

        if (iter == m_researchers.end()) {
            if (magnitude > 0) {
                iter = m_researchers.emplace(cpid, ResearchAccount()).first;
            } else {
                return m_new_account;
            }
        }

        // We lazily apply the magnitude from the current active superblock
        // to avoid reloading the magnitude for every research account when
        // a new superblock arrives:
        //
        iter->second.m_magnitude = magnitude;

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
    //! \brief Disassociate a blocks research reward data from the tally.
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
    //! \param Update the current magnitude of each research account from the
    //! provided superblock.
    //!
    //! \param superblock Refers to the current active superblock.
    //!
    void ApplySuperblock(const SuperblockPtr superblock)
    {
        // We just link the current superblock here. GetAccount() will apply
        // the magnitude lazily when requested.
        //
        m_current_superblock = superblock;
    }

private:
    //!
    //! \brief An empty account to return as a reference when requesting an
    //! account for a CPID that with no historical record.
    //!
    const ResearchAccount m_new_account;

    //!
    //! \brief The set of all research accounts in the network.
    //!
    std::unordered_map<Cpid, ResearchAccount> m_researchers;

    //!
    //! \brief A link to the current active superblock used to lazily update
    //! researcher magnitudes when supplying an account.
    //!
    SuperblockPtr m_current_superblock = std::make_shared<Superblock>();
}; // ResearcherTally

ResearcherTally g_researcher_tally; //!< Tracks lifetime research rewards.
NetworkTally g_network_tally;       //!< Tracks two-week network averages.

} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: Tally
// -----------------------------------------------------------------------------

bool Tally::IsTrigger(const uint64_t height)
{
    return height % TALLY_GRANULARITY == 0;
}

CBlockIndex* Tally::FindTrigger(CBlockIndex* pindex)
{
    // Scan backwards until we find one where accepting it would
    // trigger a tally.
    for (;
        pindex && pindex->pprev && !IsTrigger(pindex->nHeight);
        pindex = pindex->pprev);

    return pindex;
}

int64_t Tally::MaxEmission(const int64_t payment_time)
{
    return NetworkTally::MaxEmission(payment_time) * COIN;
}

double Tally::GetMagnitudeUnit(const int64_t payment_time)
{
    return g_network_tally.GetMagnitudeUnit(payment_time);
}

uint16_t Tally::MyMagnitude()
{
    if (const auto cpid_option = NN::Researcher::Get()->Id().TryCpid()) {
        return Quorum::CurrentSuperblock()->m_cpids.MagnitudeOf(*cpid_option);
    }

    return 0;
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

    if (GetAccount(cpid).IsNew()) {
        return MakeUnique<NewbieAccrualComputer>(
            cpid,
            payment_time,
            GetMagnitudeUnit(payment_time));
    }

    return MakeUnique<ResearchAgeComputer>(
        cpid,
        payment_time,
        GetMagnitudeUnit(payment_time),
        last_block_ptr->nHeight);
}

uint16_t Tally::GetMagnitude(const Cpid cpid)
{
    return Quorum::CurrentSuperblock()->m_cpids.MagnitudeOf(cpid);
}

uint16_t Tally::GetMagnitude(const MiningId mining_id)
{
    if (const auto cpid_option = mining_id.TryCpid()) {
        return GetMagnitude(*cpid_option);
    }

    return 0;
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

void Tally::LegacyRecount(const CBlockIndex* pindex)
{
    if (!pindex) {
        return;
    }

    LogPrintf("Tally::LegacyRecount(%" PRId64 ")", pindex->nHeight);

    const int64_t consensus_depth = pindex->nHeight - CONSENSUS_LOOKBACK;
    const int64_t lookback_depth = BLOCKS_PER_DAY * TALLY_DAYS;

    int64_t max_depth = consensus_depth - (consensus_depth % TALLY_GRANULARITY);
    int64_t min_depth = max_depth - lookback_depth;

    if (fTestNet && !IsV9Enabled_Tally(pindex->nHeight)) {
        LogPrintf("Tally::LegacyRecount(): retired tally");
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
        const SuperblockPtr current = Quorum::CurrentSuperblock();

        g_network_tally.ApplySuperblock(current);
        g_researcher_tally.ApplySuperblock(current);
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
