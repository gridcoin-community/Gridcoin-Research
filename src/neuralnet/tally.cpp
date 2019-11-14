#include "beacon.h"
#include "main.h"
#include "neuralnet/cpid.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/tally.h"
#include "util.h"

#include <deque>
#include <unordered_map>

using namespace NN;

namespace {
//!
//! \brief Number of days that the tally scans backward from to calculate
//! the average network payment.
//!
constexpr size_t TALLY_DAYS = 14;

//!
//! \brief Used to convert elapsed accrual seconds into days, so we declare it
//! as a double type.
//!
constexpr double ONE_DAY_IN_SECONDS = 86400.0;

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
//! \brief Get the multiplier used to calculate the maximum research reward
//! amount.
//!
//! Survey Results: Start inflation rate: 9%, end=1%, 30 day steps, 9 steps,
//! mag multiplier start: 2, mag end .3, 9 steps
//!
//! \return A value in units of GRC (not COIN) that represents the maximum
//! research reward expected per block.
//!
int64_t GetMaxResearchSubsidy(const int64_t nTime)
{
    // Gridcoin Global Daily Maximum Researcher Subsidy Schedule
    int MaxSubsidy = 500;

    if (nTime > 1447977700)                              MaxSubsidy =  50; // from  11-20-2015 forever
    else if (nTime >= 1445309276 && nTime <= 1447977700) MaxSubsidy =  75; // between 10-20-2015 and 11-20-2015
    else if (nTime >= 1438310876 && nTime <= 1445309276) MaxSubsidy = 100; // between 08-01-2015 and 10-20-2015
    else if (nTime >= 1430352000 && nTime <= 1438310876) MaxSubsidy = 150; // between 05-01-2015 and 07-31-2015
    else if (nTime >= 1427673600 && nTime <= 1430352000) MaxSubsidy = 200; // between 03-30-2015 and 04-30-2015
    else if (nTime >= 1425254400 && nTime <= 1427673600) MaxSubsidy = 250; // between 02-28-2015 and 03-30-2015
    else if (nTime >= 1422576000 && nTime <= 1425254400) MaxSubsidy = 300; // between 01-30-2015 and 02-28-2015
    else if (nTime >= 1419897600 && nTime <= 1422576000) MaxSubsidy = 400; // between 12-30-2014 and 01-30-2015
    else if (nTime >= 1417305600 && nTime <= 1419897600) MaxSubsidy = 400; // between 11-30-2014 and 12-30-2014
    else if (nTime >= 1410393600 && nTime <= 1417305600) MaxSubsidy = 500; // between inception  and 11-30-2014

    // The .5 allows for fractional amounts after the 4th decimal place (used to store the POR indicator)
    return MaxSubsidy+.5;
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
    //! \brief Get the average network-wide daily research payment per day.
    //!
    //! \return Average research payment per day over the last two weeks.
    //!
    double DailyResearchSubsidyAverage() const
    {
        return m_two_week_research_subsidy / TALLY_DAYS;
    }

    //!
    //! \brief Get the average network-wide daily block subsidy payment per day.
    //!
    //! \return Average block subsidy  payment per day over the last two weeks.
    //!
    double DailyBlockSubsidyAverage() const
    {
        return m_two_week_block_subsidy / TALLY_DAYS;
    }

    //!
    //! \brief Get a report that contains an overview of the network payment
    //! statistics stored in the tally.
    //!
    //! \param time Determines the max reward based on a schedule.
    //!
    //! \return A partial report of network statistics available in the object.
    //!
    NetworkStats GetStats(const int64_t time) const
    {
        NetworkStats stats;

        stats.m_magnitude_unit = GetMagnitudeUnit(time);

        stats.m_average_daily_research_subsidy = DailyResearchSubsidyAverage();
        stats.m_max_daily_research_subsidy = MaxEmission(time);

        stats.m_two_week_block_subsidy = m_two_week_block_subsidy;
        stats.m_average_daily_block_subsidy = DailyBlockSubsidyAverage();

        return stats;
    }

    //!
    //! \brief Reset the two-week averages to the provided values.
    //!
    //! \param block_subsidy    Sum of block payments over the last two weeks.
    //! \param research_subsidy Sum of research payments over the last two weeks.
    //!
    void Reset(double block_subsidy, double research_subsidy)
    {
        m_two_week_block_subsidy = block_subsidy;
        m_two_week_research_subsidy = research_subsidy;
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
    double m_two_week_block_subsidy = 0;    //!< Sum of block subsidy payments.
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

        account.m_total_block_subsidy += pindex->nInterestSubsidy;
        account.m_total_research_subsidy += pindex->nResearchSubsidy;

        if (pindex->nMagnitude > 0) {
            account.m_accuracy++;
            account.m_total_magnitude += pindex->nMagnitude;
        }

        if (account.m_last_block_ptr == nullptr
            || pindex->nHeight > account.m_last_block_ptr->nHeight)
        {
            account.m_last_block_ptr = pindex;
        }

        if (pindex->nTime < account.m_first_payment_time) {
            account.m_first_payment_time = pindex->nTime;
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

        if (iter->second.m_accuracy == 1) {
            m_researchers.erase(iter);
            return;
        }

        ResearchAccount& account = iter->second;

        assert(pindex == account.m_last_block_ptr);

        account.m_total_block_subsidy -= pindex->nInterestSubsidy;
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

//!
//! \brief An implementation of IAccrualComputer that always returns zeros.
//!
//! Useful for situations where we need to satisfy the interface but cannot
//! possibly calculate an accrual.
//!
class NullAccrualComputer : public IAccrualComputer
{
    // See IAccrualComputer for inherited API documentation.

public:
    double MagnitudeUnit() const override
    {
        return 0.0;
    }

    int64_t AccrualAge(const ResearchAccount& account) const override
    {
        return 0;
    }

    double AccrualDays(const ResearchAccount& account) const override
    {
        return 0.0;
    }

    int64_t AccrualBlockSpan(const ResearchAccount& account) const override
    {
        return 0;
    }

    double AverageMagnitude(const ResearchAccount& account) const override
    {
        return 0.0;
    }

    double PaymentPerDay(const ResearchAccount& account) const override
    {
        return 0.0;
    }

    double PaymentPerDayLimit(const ResearchAccount& account) const override
    {
        return 0.0;
    }

    bool ExceededRecentPayments(const ResearchAccount& account) const override
    {
        return false;
    }

    double ExpectedDaily(const ResearchAccount& account) const override
    {
        return 0.0;
    }

    int64_t RawAccrual(const ResearchAccount& account) const override
    {
        return 0.0;
    }

    int64_t Accrual(const ResearchAccount& account) const override
    {
        return 0.0;
    }
}; // NullAccrualComputer

//!
//! \brief An accrual calculator for a CPID that never earned a research reward
//! before.
//!
class NewbieAccrualComputer : public NullAccrualComputer
{
    // See IAccrualComputer for inherited API documentation.

public:
    //!
    //! \brief Initialze an accrual calculator for a CPID that never earned
    //! a research reward before.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param magnitude_unit Current network magnitude unit to factor in.
    //!
    NewbieAccrualComputer(
        const Cpid cpid,
        const int64_t payment_time,
        const double magnitude_unit)
        : m_cpid(cpid)
        , m_payment_time(payment_time)
        , m_magnitude_unit(magnitude_unit)
    {
    }

    double MagnitudeUnit() const override
    {
        return m_magnitude_unit;
    }

    int64_t AccrualAge(const ResearchAccount& account) const override
    {
        const int64_t beacon_time = BeaconTimeStamp(m_cpid.ToString());

        if (beacon_time == 0) {
            return 0;
        }

        return m_payment_time - beacon_time;
    }

    double AccrualDays(const ResearchAccount& account) const override
    {
        return AccrualAge(account) / ONE_DAY_IN_SECONDS;
    }

    double AverageMagnitude(const ResearchAccount& account) const override
    {
        return account.m_magnitude;
    }

    double PaymentPerDayLimit(const ResearchAccount& account) const override
    {
        return 500.00;
    }

    bool ExceededRecentPayments(const ResearchAccount& account) const override
    {
        return RawAccrual(account) > PaymentPerDayLimit(account) * COIN;
    }

    double ExpectedDaily(const ResearchAccount& account) const override
    {
        return account.m_magnitude * m_magnitude_unit;
    }

    int64_t RawAccrual(const ResearchAccount& account) const override
    {
        return AccrualDays(account) * account.m_magnitude * m_magnitude_unit * COIN;
    }

    int64_t Accrual(const ResearchAccount& account) const override
    {
        constexpr int64_t six_months = ONE_DAY_IN_SECONDS * 30 * 6;

        if (AccrualAge(account) >= six_months) {
            if (fDebug) {
                LogPrintf(
                    "Accrual: %s Invalid Beacon, Using 0.01 age bootstrap",
                    m_cpid.ToString());
            }

            return ((double)account.m_magnitude / 100) * COIN + (1 * COIN);
        }

        const int64_t accrual = RawAccrual(account);

        if (accrual > 500 * COIN) {
            if (fDebug) {
                LogPrintf(
                    "Accrual: %s Newbie special stake capped to 500 GRC.",
                    m_cpid.ToString());
            }

            return 500 * COIN;
        }

        return (accrual * COIN) + (1 * COIN);
    }

private:
    const Cpid m_cpid;             //!< CPID to calculate research accrual for.
    const int64_t m_payment_time;  //!< Time of payment to calculate rewards at.
    const double m_magnitude_unit; //!< Network magnitude unit to factor in.
}; // NewbieAccrualComputer

ResearcherTally g_researcher_tally; //!< Tracks lifetime research rewards.
NetworkTally g_network_tally;       //!< Tracks two-week network averages.

} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: ResearchAccount
// -----------------------------------------------------------------------------

ResearchAccount::ResearchAccount()
    : m_total_block_subsidy(0)
    , m_total_research_subsidy(0)
    , m_magnitude(0)
    , m_total_magnitude(0)
    , m_accuracy(0)
    , m_first_payment_time(std::numeric_limits<uint64_t>::max())
    , m_last_block_ptr(nullptr)
{
}

double ResearchAccount::IsNew() const
{
    return m_last_block_ptr == nullptr;
}

BlockPtrOption ResearchAccount::LastRewardBlock() const
{
    if (m_last_block_ptr == nullptr) {
        return boost::none;
    }

    return BlockPtrOption(m_last_block_ptr);
}

uint256 ResearchAccount::LastRewardBlockHash() const
{
    if (const BlockPtrOption pindex = LastRewardBlock()) {
        return (*pindex)->GetBlockHash();
    }

    return uint256();
}

uint32_t ResearchAccount::LastRewardHeight() const
{
    if (const BlockPtrOption pindex = LastRewardBlock()) {
        return (*pindex)->nHeight;
    }

    return 0;
}

int64_t ResearchAccount::LastRewardTime() const
{
    if (const BlockPtrOption pindex = LastRewardBlock()) {
        return (*pindex)->nTime;
    }

    return 0;
}

uint16_t ResearchAccount::LastRewardMagnitude() const
{
    if (const BlockPtrOption pindex = LastRewardBlock()) {
        return (*pindex)->nMagnitude;
    }

    return 0;
}

double ResearchAccount::AverageLifetimeMagnitude() const
{
    if (m_accuracy <= 0) {
        return 0.0;
    }

    return (double)m_total_magnitude / m_accuracy;
}

// -----------------------------------------------------------------------------
// Class: ResearchAgeComputer
// -----------------------------------------------------------------------------

ResearchAgeComputer::ResearchAgeComputer(
    const Cpid cpid,
    const int64_t payment_time,
    const double magnitude_unit,
    const uint32_t last_height)
    : m_cpid(cpid)
    , m_payment_time(payment_time)
    , m_magnitude_unit(magnitude_unit)
    , m_last_height(last_height)
{
}

int64_t ResearchAgeComputer::MaxReward(const int64_t payment_time)
{
    return GetMaxResearchSubsidy(payment_time) * 255 * COIN;
}

double ResearchAgeComputer::MagnitudeUnit() const
{
    return m_magnitude_unit;
}

int64_t ResearchAgeComputer::AccrualAge(const ResearchAccount& account) const
{
    if (const BlockPtrOption pindex_option = account.LastRewardBlock()) {
        const CBlockIndex* const pindex = *pindex_option;

        if (m_payment_time > pindex->nTime) {
            return m_payment_time - pindex->nTime;
        }
    }

    return 0;
}

int64_t ResearchAgeComputer::AccrualBlockSpan(const ResearchAccount& account) const
{
    return m_last_height - account.LastRewardHeight();
}

double ResearchAgeComputer::AccrualDays(const ResearchAccount& account) const
{
    return AccrualAge(account) / ONE_DAY_IN_SECONDS;
}

double ResearchAgeComputer::AverageMagnitude(const ResearchAccount& account) const
{
    const uint16_t last_magnitude = account.LastRewardMagnitude();

    if (m_last_height - account.LastRewardHeight() <= BLOCKS_PER_DAY * 20) {
        return (double)(last_magnitude + account.m_magnitude) / 2;
    }

    // ResearchAge: If the accrual age is more than 20 days, add in the midpoint
    // lifetime average magnitude to ensure the overall avg magnitude accurate:
    //
    // TODO: use superblock windows to calculate more precise average
    //
    const double total_mag = last_magnitude
        + account.AverageLifetimeMagnitude()
        + account.m_magnitude;

    return total_mag / 3;
}

double ResearchAgeComputer::PaymentPerDay(const ResearchAccount& account) const
{
    if (account.IsNew()) {
        return 0;
    }

    const int64_t elapsed = m_payment_time - account.m_first_payment_time;
    const double lifetime_days = elapsed / ONE_DAY_IN_SECONDS;

    // The extra 0.01 days was used in old code as a lazy way to avoid division
    // by zero errors. The payment per day limit exception in historical blocks
    // depends on this extra value, so we keep it. This especially manifests on
    // testnet where CPIDs stake frequently enough to activate the rule:
    //
    return account.m_total_research_subsidy / (lifetime_days + 0.01);
}

double ResearchAgeComputer::PaymentPerDayLimit(const ResearchAccount& account) const
{
    return account.AverageLifetimeMagnitude() * m_magnitude_unit * 5;
}

bool ResearchAgeComputer::ExceededRecentPayments(const ResearchAccount& account) const
{
    return PaymentPerDay(account) > PaymentPerDayLimit(account);
}

double ResearchAgeComputer::ExpectedDaily(const ResearchAccount& account) const
{
    return account.m_magnitude * m_magnitude_unit;
}

int64_t ResearchAgeComputer::RawAccrual(const ResearchAccount& account) const
{
    double current_avg_magnitude = AverageMagnitude(account);

    // Legacy sanity check for unlikely magnitude values:
    if (current_avg_magnitude > 20000) {
        current_avg_magnitude = 20000;
    }

    return AccrualDays(account) * current_avg_magnitude * m_magnitude_unit * COIN;
}

int64_t ResearchAgeComputer::Accrual(const ResearchAccount& account) const
{
    // Note that if the RA block span < 10, we want to return 0 for the accrual
    // amount so the CPID can still receive an accurate accrual in the future:
    //
    if (AccrualBlockSpan(account) < 10) {
        if (fDebug) {
            LogPrintf(
                "Accrual: %s Block Span less than 10 (%d) -> Accrual 0 (would be %f)",
                m_cpid.ToString(),
                AccrualBlockSpan(account),
                FormatMoney(RawAccrual(account)));
        }

        return 0;
    }

    // Since this condition can occur when a user ramps up their computing
    // power, let's return 0 to avoid shortchanging a researcher. Instead,
    // owed GRC continues to accrue and will be paid later after payments-
    // per-day falls below 5:
    //
    if (ExceededRecentPayments(account)) {
        if (fDebug) {
            LogPrintf("Accrual: %s RA-PPD, "
                "PPD=%f, "
                "unit=%f, "
                "RAAvgMag=%f, "
                "RASubsidy=%f, "
                "RALowLockTime=%d "
                "-> Accrual 0 (would be %f)",
                m_cpid.ToString(),
                PaymentPerDay(account),
                m_magnitude_unit,
                account.AverageLifetimeMagnitude(),
                account.m_total_research_subsidy,
                account.m_first_payment_time,
                FormatMoney(RawAccrual(account)));
        }

        return 0;
    }

    const int64_t accrual = RawAccrual(account);
    const int64_t max_reward = MaxReward(m_payment_time);

    if (accrual > max_reward) {
        return max_reward;
    }

    return accrual;
}

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

NetworkStats Tally::GetNetworkStats(int64_t time)
{
    if (time == 0) {
        time = GetAdjustedTime();
    }

    const SuperblockPtr superblock = Quorum::CurrentSuperblock();

    NetworkStats stats = g_network_tally.GetStats(time);

    stats.m_total_cpids = superblock->m_cpids.size();
    stats.m_total_magnitude = superblock->m_cpids.TotalMagnitude();
    stats.m_average_magnitude = superblock->m_cpids.AverageMagnitude();

    return stats;
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

    double total_block_subsidy = 0;
    double total_research_subsidy = 0;

    while (pindex->nHeight > min_depth) {
        if (!pindex->pprev) {
            return;
        }

        pindex = pindex->pprev;

        // Note: block subsidy is not required for the tally system. We only
        // count these for informational statistics:
        //
        total_block_subsidy += pindex->nInterestSubsidy;
        total_research_subsidy += pindex->nResearchSubsidy;
    }

    g_network_tally.Reset(total_block_subsidy, total_research_subsidy);
}
