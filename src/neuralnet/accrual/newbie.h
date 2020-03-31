#pragma once

#include "beacon.h"
#include "neuralnet/accrual/computer.h"

namespace {
using namespace NN;

//!
//! \brief An accrual calculator for a CPID that never earned a research reward
//! before.
//!
class NewbieAccrualComputer : public IAccrualComputer
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
    //! \param magnitude      CPID's magnitude in the last superblock.
    //!
    NewbieAccrualComputer(
        const Cpid cpid,
        const int64_t payment_time,
        const double magnitude_unit,
        const double magnitude)
        : m_cpid(cpid)
        , m_payment_time(payment_time)
        , m_magnitude_unit(magnitude_unit)
        , m_magnitude(magnitude)
    {
    }

    int64_t MaxReward() const override
    {
        return 500 * COIN;
    }

    double MagnitudeUnit() const override
    {
        return m_magnitude_unit;
    }

    int64_t AccrualAge() const override
    {
        const int64_t beacon_time = BeaconTimeStamp(m_cpid.ToString());

        if (beacon_time == 0) {
            return 0;
        }

        return m_payment_time - beacon_time;
    }

    double AccrualDays() const override
    {
        return AccrualAge() / 86400.0;
    }

    int64_t AccrualBlockSpan() const override
    {
        return 0;
    }

    int64_t PaymentPerDay() const override
    {
        return 0;
    }

    int64_t PaymentPerDayLimit() const override
    {
        return MaxReward();
    }

    bool ExceededRecentPayments() const override
    {
        return RawAccrual() > PaymentPerDayLimit();
    }

    int64_t ExpectedDaily() const override
    {
        return m_magnitude * m_magnitude_unit * COIN;
    }

    int64_t RawAccrual() const override
    {
        return AccrualDays() * ExpectedDaily();
    }

    int64_t Accrual() const override
    {
        if (m_magnitude <= 0) {
            return 0;
        }

        constexpr int64_t six_months = 86400 * 30 * 6; // seconds

        if (AccrualAge() >= six_months) {
            if (fDebug) {
                LogPrintf(
                    "Accrual: %s Invalid Beacon, Using 0.01 age bootstrap",
                    m_cpid.ToString());
            }

            return (m_magnitude / 100) * COIN + (1 * COIN);
        }

        const int64_t accrual = RawAccrual();

        if (accrual > MaxReward()) {
            if (fDebug) {
                LogPrintf(
                    "Accrual: %s Newbie special stake capped to 500 GRC.",
                    m_cpid.ToString());
            }

            return MaxReward();
        }

        return accrual + (1 * COIN);
    }

private:
    const Cpid m_cpid;             //!< CPID to calculate research accrual for.
    const int64_t m_payment_time;  //!< Time of payment to calculate rewards at.
    const double m_magnitude_unit; //!< Network magnitude unit to factor in.
    const double m_magnitude;      //!< CPID's magnitude in the last superblock.
}; // NewbieAccrualComputer
} // anonymous namespace
