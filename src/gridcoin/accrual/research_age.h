// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "gridcoin/accrual/computer.h"

namespace {
using namespace GRC;

//!
//! \brief Get the multiplier used to calculate the maximum research reward
//! amount.
//!
//! Survey Results: Start inflation rate: 9%, end=1%, 30 day steps, 9 steps,
//! mag multiplier start: 2, mag end .3, 9 steps
//!
//! \return A value in units of GRC that represents the maximum research reward
//! expected per block.
//!
CAmount GetMaxResearchSubsidy(const int64_t nTime)
{
    // Gridcoin Global Daily Maximum Researcher Subsidy Schedule
    CAmount MaxSubsidy = 500;

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
//! \brief A calculator that computes the accrued research rewards for a
//! research account using legacy research age rules in block version 10
//! and below.
//!
class ResearchAgeComputer : public IAccrualComputer
{
public:
    //!
    //! \brief Initialize a research age accrual calculator.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param account        CPID's historical accrual context.
    //! \param magnitude      CPID's magnitude in the last superblock.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param magnitude_unit Current network magnitude unit to factor in.
    //! \param last_height    Height of the block for the reward.
    //!
    ResearchAgeComputer(
        const Cpid cpid,
        const ResearchAccount& account,
        const double magnitude,
        const int64_t payment_time,
        const double magnitude_unit,
        const uint32_t last_height)
        : m_cpid(cpid)
        , m_account(account)
        , m_magnitude(magnitude)
        , m_payment_time(payment_time)
        , m_magnitude_unit(magnitude_unit)
        , m_last_height(last_height)
    {
    }

    //!
    //! \brief Get the maximum research reward payable in one block.
    //!
    //! \return Max reward allowed in units of 1/100000000 GRC.
    //!
    CAmount MaxReward() const override
    {
        return GetMaxResearchSubsidy(m_payment_time) * 255 * COIN;
    }

    //!
    //! \brief Get the magnitude unit factored into the reward calculation.
    //!
    //! \return Amount paid per unit of magnitude per day in units of GRC.
    //!
    double MagnitudeUnit() const override
    {
        return m_magnitude_unit;
    }

    //!
    //! \brief Get the time elapsed since the account's last research reward.
    //!
    //! \return Elapsed time in seconds.
    //!
    int64_t AccrualAge() const override
    {
        if (const BlockPtrOption pindex_option = m_account.LastRewardBlock()) {
            const CBlockIndex* const pindex = *pindex_option;

            if (m_payment_time > pindex->nTime) {
                return m_payment_time - pindex->nTime;
            }
        }

        return 0;
    }

    //!
    //! \brief Get the number of days since the account's last research reward.
    //!
    //! \return Elapsed time in days.
    //!
    double AccrualDays() const override
    {
        return AccrualAge() / 86400.0;
    }

    //!
    //! \brief Get the number of blocks since the account's last research reward.
    //!
    int64_t AccrualBlockSpan() const override
    {
        return m_last_height - m_account.LastRewardHeight();
    }

    //!
    //! \brief Get the average daily research payment over the lifetime of the
    //! account.
    //!
    //! \return Average research payment in units of 1/100000000 GRC.
    //!
    CAmount PaymentPerDay() const override
    {
        if (m_account.IsNew()) {
            return 0;
        }

        const int64_t elapsed = m_payment_time - m_account.FirstRewardTime();
        const double lifetime_days = elapsed / 86400.0;

        // The extra 0.01 days was used in old code as a lazy way to avoid
        // division by zero errors. The payment per day limit exception in
        // historical blocks depends on this extra value today, so we must
        // keep it. This especially manifests on testnet where CPIDs stake
        // frequently enough to activate the rule:
        //
        return m_account.m_total_research_subsidy / (lifetime_days + 0.01);
    }

    //!
    //! \brief Get the average daily research payment limit of the account.
    //!
    //! \return Payment per day limit in units of 1/100000000 GRC.
    //!
    CAmount PaymentPerDayLimit() const override
    {
        return m_account.AverageLifetimeMagnitude() * m_magnitude_unit * COIN * 5;
    }

    //!
    //! \brief Determine whether the account exceeded the daily payment limit.
    //!
    //! \return \c true if the average daily research payment amount exceeds
    //! the calculated daily payment limit of the account.
    //!
    bool ExceededRecentPayments() const override
    {
        return PaymentPerDay() > PaymentPerDayLimit();
    }

    //!
    //! \brief Get the expected daily research payment for the account based on
    //! the current network payment conditions.
    //!
    //! \return Expected daily payment in units of 1/100000000 GRC.
    //!
    CAmount ExpectedDaily() const override
    {
        return m_magnitude * m_magnitude_unit * COIN;
    }

    //!
    //! \brief Get the pending research reward for the account without applying
    //! any limit rules.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    CAmount RawAccrual() const override
    {
        double current_avg_magnitude = AverageMagnitude();

        // Legacy sanity check for unlikely magnitude values:
        if (current_avg_magnitude > 20000) {
            current_avg_magnitude = 20000;
        }

        return AccrualDays() * current_avg_magnitude * m_magnitude_unit * COIN;
    }

    //!
    //! \brief Get the pending research reward for the account as expected by
    //! the network.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    CAmount Accrual() const override
    {
        // Note that if the RA block span < 10, we want to return 0 for the
        // accrual amount so the CPID can still receive an accurate accrual
        // in the future:
        //
        if (AccrualBlockSpan() < 10) {
            LogPrint(BCLog::LogFlags::ACCRUAL,
                "Accrual: %s Block Span < 10 (%d) -> Accrual 0 (would be %s)",
                m_cpid.ToString(),
                AccrualBlockSpan(),
                FormatMoney(RawAccrual()));

            return 0;
        }

        // Since this condition can occur when a user ramps up their computing
        // power, let's return 0 to avoid shortchanging a researcher. Instead,
        // owed GRC continues to accrue and will be paid later after payments-
        // per-day falls below 5 days:
        //
        if (ExceededRecentPayments()) {
            LogPrint(BCLog::LogFlags::ACCRUAL,
                "Accrual: %s RA-PPD, "
                "PPD=%s, "
                "unit=%f, "
                "RAAvgMag=%f, "
                "RASubsidy=%s, "
                "RALowLockTime=%" PRIu64 " "
                "-> Accrual 0 (would be %s)",
                m_cpid.ToString(),
                FormatMoney(PaymentPerDay()),
                m_magnitude_unit,
                m_account.AverageLifetimeMagnitude(),
                FormatMoney(m_account.m_total_research_subsidy),
                m_account.FirstRewardTime(),
                FormatMoney(RawAccrual()));

            return 0;
        }

        const CAmount accrual = RawAccrual();
        const CAmount max_reward = MaxReward();

        if (accrual > max_reward) {
            return max_reward;
        }

        return accrual;
    }

private:
    const Cpid m_cpid;                //!< CPID to calculate research accrual for.
    const ResearchAccount& m_account; //!< CPID's historical accrual context.
    const double m_magnitude;         //!< CPID's magnitude last superblock.
    const int64_t m_payment_time;     //!< Time of payment to calculate rewards at.
    const double m_magnitude_unit;    //!< Network magnitude unit to factor in.
    const uint32_t m_last_height;     //!< Height of the block for the reward.

    //!
    //! \brief Get the average magnitude of the account over the accrual time
    //! span.
    //!
    //! \return Average of the CPID's current magnitude and the magnitude for
    //! the CPID's last generated block. When the CPID's last staked block is
    //! older than roughly 20 days, this average includes the CPID's lifetime
    //! average magnitude as well.
    //!
    double AverageMagnitude() const
    {
        uint16_t last_magnitude = m_account.LastRewardMagnitude();

        if (AccrualBlockSpan() <= BLOCKS_PER_DAY * 20) {
            return (last_magnitude + m_magnitude) / 2;
        }

        // If the accrual age is greater than than 20 days, add the midpoint
        // average magnitude to ensure that the overall average magnitude is
        // accurate:
        //
        // TODO: use superblock windows to calculate more precise average
        //
        const double total_mag = last_magnitude
            + m_account.AverageLifetimeMagnitude()
            + m_magnitude;

        return total_mag / 3;
    }
}; // ResearchAgeComputer
} // anonymous namespace
