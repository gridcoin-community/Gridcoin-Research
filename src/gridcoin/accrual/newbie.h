// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "gridcoin/accrual/computer.h"
#include "gridcoin/beacon.h"

namespace {
using namespace GRC;

//!
//! \brief An accrual calculator for a CPID that never earned a research reward
//! before. Used for legacy accrual calculations in block version 10 and below.
//!
class NewbieAccrualComputer : public IAccrualComputer
{
    // See IAccrualComputer for inherited API documentation.

public:
    //!
    //! \brief Initialize an accrual calculator for a CPID that never earned
    //! a research reward before.
    //!
    //! \param cpid           CPID to calculate research accrual for.
    //! \param account        CPID's historical accrual context.
    //! \param payment_time   Time of payment to calculate rewards at.
    //! \param magnitude_unit Current network magnitude unit to factor in.
    //! \param magnitude      CPID's magnitude in the last superblock.
    //!
    NewbieAccrualComputer(
        const Cpid cpid,
        const ResearchAccount& account,
        const int64_t payment_time,
        const double magnitude_unit,
        const double magnitude)
        : m_cpid(cpid)
        , m_account(account)
        , m_payment_time(payment_time)
        , m_magnitude_unit(magnitude_unit)
        , m_magnitude(magnitude)
    {
    }

    CAmount MaxReward() const override
    {
        return 500 * COIN;
    }

    double MagnitudeUnit() const override
    {
        return m_magnitude_unit;
    }

    int64_t AccrualAge() const override
    {
        if (const BeaconOption beacon = GetBeaconRegistry().Try(m_cpid)) {
            return m_payment_time - beacon->m_timestamp;
        }

        return 0;
    }

    double AccrualDays() const override
    {
        return AccrualAge() / 86400.0;
    }

    int64_t AccrualBlockSpan() const override
    {
        return 0;
    }

    CAmount PaymentPerDay() const override
    {
        return 0;
    }

    CAmount PaymentPerDayLimit() const override
    {
        return MaxReward();
    }

    bool ExceededRecentPayments() const override
    {
        return RawAccrual() > PaymentPerDayLimit();
    }

    CAmount ExpectedDaily() const override
    {
        return m_magnitude * m_magnitude_unit * COIN;
    }

    CAmount RawAccrual() const override
    {
        return AccrualDays() * ExpectedDaily();
    }

    CAmount Accrual() const override
    {
        if (m_magnitude <= 0) {
            return 0;
        }

        constexpr int64_t six_months = 86400 * 30 * 6; // seconds

        if (AccrualAge() >= six_months || m_account.m_total_research_subsidy > 0) {
            LogPrint(BCLog::LogFlags::ACCRUAL,
                "Accrual: %s Invalid Beacon, Using 0.01 age bootstrap",
                m_cpid.ToString());

            return (m_magnitude / 100) * COIN + (1 * COIN);
        }

        const int64_t accrual = RawAccrual();

        if (accrual > MaxReward()) {
            LogPrint(BCLog::LogFlags::ACCRUAL,
                "Accrual: %s Newbie special stake capped to 500 GRC.",
                m_cpid.ToString());

            return MaxReward();
        }

        return accrual + (1 * COIN);
    }

private:
    const Cpid m_cpid;             //!< CPID to calculate research accrual for.
    const ResearchAccount& m_account; //!< CPID's historical accrual context.
    const int64_t m_payment_time;  //!< Time of payment to calculate rewards at.
    const double m_magnitude_unit; //!< Network magnitude unit to factor in.
    const double m_magnitude;      //!< CPID's magnitude in the last superblock.
}; // NewbieAccrualComputer
} // anonymous namespace
