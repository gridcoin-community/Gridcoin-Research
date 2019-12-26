#pragma once

#include <memory>
#include <set>

#include <boost/optional.hpp>

class CBlockIndex;

namespace NN {

class Cpid;

//!
//! \brief An optional type that contains a pointer to a block index object or
//! does not.
//!
typedef boost::optional<const CBlockIndex*> BlockPtrOption;

//!
//! \brief Compares CBlockIndex objects by comparing block heights.
//!
struct BlockIndexHeightComparator
{
    //!
    //! \brief Determine whether \p a has a height less than \p b.
    //!
    //! \return \c true if the height of \p a is less than the height of \p b.
    //!
    bool operator()(const CBlockIndex* const a, const CBlockIndex* const b) const
    {
        return a->nHeight < b->nHeight;
    }
};

//!
//! \brief Stores the research reward context for a CPID used to calculate
//! accrued research rewards.
//!
class ResearchAccount
{
public:
    int64_t m_total_research_subsidy;    //!< Total lifetime research paid.

    uint16_t m_magnitude;                //!< Current magnitude in superblock.
    uint32_t m_total_magnitude;          //!< Total lifetime magnitude sum.
    uint32_t m_accuracy;                 //!< Non-zero magnitude payment count.

    const CBlockIndex* m_first_block_ptr; //!< First block with research reward.
    const CBlockIndex* m_last_block_ptr; //!< Last block with research reward.

    //!
    //! \brief Initialize an empty research account.
    //!
    ResearchAccount();

    //!
    //! \brief Determine whether this is a new research account.
    //!
    //! \return \c true if no block contains a reward for the account's CPID.
    //!
    bool IsNew() const;

    //!
    //! \brief Get a pointer to the first block with a research reward earned by
    //! the account.
    //!
    //! \return A pointer to the block index if the account earned a research
    //! reward before.
    //!
    BlockPtrOption FirstRewardBlock() const;

    //!
    //! \brief Get the hash of the first block with a research reward earned by
    //! the account.
    //!
    //! \return The SHA256 hash of the account's first reward block.
    //!
    uint256 FirstRewardBlockHash() const;

    //!
    //! \brief Get the height of the first block with a research reward earned
    //! by the account.
    //!
    //! \return A block height of zero if the account never earned a research
    //! reward before.
    //!
    uint32_t FirstRewardHeight() const;

    //!
    //! \brief Get the timestamp of the first block with a research reward
    //! earned by the account.
    //!
    //! \return A timestamp of zero if the account never earned a research
    //! reward before.
    //!
    int64_t FirstRewardTime() const;

    //!
    //! \brief Get a pointer to the last block with a research reward earned by
    //! the account.
    //!
    //! \return A pointer to the block index if the account earned a research
    //! reward before.
    //!
    BlockPtrOption LastRewardBlock() const;

    //!
    //! \brief Get the hash of the last block with a research reward earned by
    //! the account.
    //!
    //! \return The SHA256 hash of the account's last reward block.
    //!
    uint256 LastRewardBlockHash() const;

    //!
    //! \brief Get the height of the last block with a research reward earned
    //! by the account.
    //!
    //! \return A block height of zero if the account never earned a research
    //! reward before.
    //!
    uint32_t LastRewardHeight() const;

    //!
    //! \brief Get the timestamp of the last block with a research reward earned
    //! by the account.
    //!
    //! \return A timestamp of zero if the account never earned a research
    //! reward before.
    //!
    int64_t LastRewardTime() const;

    //!
    //! \brief Get the magnitude of the CPID in the last block with a research
    //! reward earned by the account.
    //!
    //! \return A magnitude of zero if the account never earned a research
    //! reward before.
    //!
    uint16_t LastRewardMagnitude() const;

    //!
    //! \brief Get the average magnitude of the CPID over every block with a
    //! research reward earned by the account.
    //!
    //! \return A magnitude of zero if the account never earned a research
    //! reward before.
    //!
    double AverageLifetimeMagnitude() const;
};

//!
//! \brief A traversable range of all the research accounts stored in the tally.
//!
//! This type provides \c const access to the research accounts in the tally
//! without exposing the internal storage or implementation.
//!
class ResearchAccountRange
{
    typedef std::unordered_map<Cpid, ResearchAccount> StorageType;

public:
    typedef StorageType::const_iterator const_iterator;
    typedef StorageType::size_type size_type;

    //!
    //! \brief Initialze the wrapper.
    //!
    //! \param accounts Points to the accounts stored in the tally.
    //!
    ResearchAccountRange(const StorageType& accounts) : m_accounts(accounts)
    {
    }

    //!
    //! \brief Returns an iterator to the beginning.
    //!
    const_iterator begin() const
    {
        return m_accounts.begin();
    }

    //!
    //! \brief Returns an iterator to the end.
    //!
    const_iterator end() const
    {
        return m_accounts.end();
    }

    //!
    //! \brief Get the number of accounts in the tally.
    //!
    size_type size() const
    {
        return m_accounts.size();
    }

private:
    const StorageType& m_accounts; //!< The accounts stored in the tally.
};

//!
//! \brief A calculator that computes the accrued research rewards for a
//! research account.
//!
class IAccrualComputer
{
public:
    //!
    //! \brief Destructor.
    //!
    virtual ~IAccrualComputer() { }

    //!
    //! \brief Get the magnitude unit factored into the reward calculation.
    //!
    virtual double MagnitudeUnit() const = 0;

    //!
    //! \brief Get the time elapsed since the account's last research reward.
    //!
    //! \return Elapsed time in seconds.
    //!
    virtual int64_t AccrualAge(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the number of days since the account's last research reward.
    //!
    virtual double AccrualDays(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the number of blocks since the account's last research reward.
    //!
    virtual int64_t AccrualBlockSpan(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the average magnitude of the account over the accrual time
    //! span.
    //!
    virtual double AverageMagnitude(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the average daily research payment over the lifetime of the
    //! account.
    //!
    //! \return Average research payment in units of 1/100000000 GRC.
    //!
    virtual int64_t PaymentPerDay(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the average daily research payment limit of the account.
    //!
    //! \return Payment per day limit in units of 1/100000000 GRC.
    //!
    virtual int64_t PaymentPerDayLimit(const ResearchAccount& account) const = 0;

    //!
    //! \brief Determine whether the account exceeded the daily payment limit.
    //!
    //! \return \c true if the average daily research payment amount exceeds
    //! the calculated daily payment limit of the account.
    //!
    virtual bool ExceededRecentPayments(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the expected daily research payment for the account based on
    //! the current network payment conditions.
    //!
    //! \return Expected daily payment in units of 1/100000000 GRC.
    //!
    virtual int64_t ExpectedDaily(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the pending research reward for the account without applying
    //! any limit rules.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    virtual int64_t RawAccrual(const ResearchAccount& account) const = 0;

    //!
    //! \brief Get the pending research reward for the account as expected by
    //! the network.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    virtual int64_t Accrual(const ResearchAccount& account) const = 0;
};

//!
//! \brief Wraps an object that implements the IAccrualComputer interface.
//!
typedef std::unique_ptr<IAccrualComputer> AccrualComputer;

//!
//! \brief A calculator that computes the accrued research rewards for a
//! research account using research age rules.
//!
class ResearchAgeComputer : public IAccrualComputer
{
public:
    //!
    //! \brief Initialze a research age accrual calculator.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param magnitude_unit Current network magnitude unit to factor in.
    //! \param last_height    Height of the block for the reward.
    //!
    ResearchAgeComputer(
        const Cpid cpid,
        const int64_t payment_time,
        const double magnitude_unit,
        const uint32_t last_height);

    //!
    //! \brief Get the maximum research reward payable in one block.
    //!
    //! \param payment_time Time of payment to calculate rewards for.
    //!
    //! \return Max reward allowed in units of 1/100000000 GRC.
    //!
    static int64_t MaxReward(const int64_t payment_time);

    //!
    //! \brief Get the magnitude unit factored into the reward calculation.
    //!
    double MagnitudeUnit() const override;

    //!
    //! \brief Get the time elapsed since the account's last research reward.
    //!
    //! \return Elapsed time in seconds.
    //!
    int64_t AccrualAge(const ResearchAccount& account) const override;

    //!
    //! \brief Get the number of days since the account's last research reward.
    //!
    double AccrualDays(const ResearchAccount& account) const override;

    //!
    //! \brief Get the number of blocks since the account's last research reward.
    //!
    int64_t AccrualBlockSpan(const ResearchAccount& account) const override;

    //!
    //! \brief Get the average magnitude of the account over the accrual time
    //! span.
    //!
    double AverageMagnitude(const ResearchAccount& account) const override;

    //!
    //! \brief Get the average daily research payment over the lifetime of the
    //! account.
    //!
    //! \return Average research payment in units of 1/100000000 GRC.
    //!
    int64_t PaymentPerDay(const ResearchAccount& account) const override;

    //!
    //! \brief Get the average daily research payment limit of the account.
    //!
    //! \return Payment per day limit in units of 1/100000000 GRC.
    //!
    int64_t PaymentPerDayLimit(const ResearchAccount& account) const override;

    //!
    //! \brief Determine whether the account exceeded the daily payment limit.
    //!
    //! \return \c true if the average daily research payment amount exceeds
    //! the calculated daily payment limit of the account.
    //!
    bool ExceededRecentPayments(const ResearchAccount& account) const override;

    //!
    //! \brief Get the expected daily research payment for the account based on
    //! the current network payment conditions.
    //!
    //! \return Expected daily payment in units of 1/100000000 GRC.
    //!
    int64_t ExpectedDaily(const ResearchAccount& account) const override;

    //!
    //! \brief Get the pending research reward for the account without applying
    //! any limit rules.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    int64_t RawAccrual(const ResearchAccount& account) const override;

    //!
    //! \brief Get the pending research reward for the account as expected by
    //! the network.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    int64_t Accrual(const ResearchAccount& account) const override;

private:
    const Cpid m_cpid;             //!< CPID to calculate research accrual for.
    const int64_t m_payment_time;  //!< Time of payment to calculate rewards at.
    const double m_magnitude_unit; //!< Network magnitude unit to factor in.
    const uint32_t m_last_height;  //!< Height of the block for the reward.
};

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
    //! \brief Check whether the height of the specified block matches the
    //! tally granularity.
    //!
    //! \param height The block height to check for.
    //!
    //! \return \c true if the block height should trigger a recount.
    //!
    static bool IsTrigger(const uint64_t height);

    //!
    //! \brief Find the previous tally trigger below the specified block.
    //!
    //! \param pindex Index of the block to start from.
    //!
    //! \return Index of the first tally trigger block.
    //!
    static CBlockIndex* FindTrigger(CBlockIndex* pindex);

    //!
    //! \brief Get the maximum network-wide research reward amount per day.
    //!
    //! \param payment_time Determines the max reward based on a schedule.
    //!
    //! \return Maximum daily emission in units of 1/100000000 GRC.
    //!
    static int64_t MaxEmission(const int64_t payment_time);

    //!
    //! \brief Get the current network magnitude unit.
    //!
    //! \param payment_time Timestamp to calculate the magnitude unit for.
    //!
    //! \return Current magnitude unit adjusted for the specified time.
    //!
    static double GetMagnitudeUnit(const int64_t payment_time);

    //!
    //! \brief Get the current magnitude of the CPID loaded by the wallet.
    //!
    //! \return The wallet user's magnitude or zero if the wallet started in
    //! investor mode.
    //!
    static uint16_t MyMagnitude();

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

    //!
    //! \brief Get the current magnitude for the specified CPID.
    //!
    //! \param cpid The CPID to fetch the magnitude for.
    //!
    //! \return Magnitude as of the last tallied superblock.
    //!
    static uint16_t GetMagnitude(const Cpid cpid);

    //!
    //! \brief Get the current magnitude for the specified mining ID.
    //!
    //! \param cpid May contain a CPID to fetch the magnitude for.
    //!
    //! \return Magnitude as of the last tallied superblock or zero if the
    //! mining ID represents an investor.
    //!
    static uint16_t GetMagnitude(const MiningId mining_id);

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
};
}
