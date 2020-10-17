// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "gridcoin/account.h"

#include <memory>

namespace GRC {
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
    //! \brief Get the maximum research reward payable in one block.
    //!
    //! \return Max reward allowed in units of 1/100000000 GRC.
    //!
    virtual CAmount MaxReward() const = 0;

    //!
    //! \brief Get the magnitude unit factored into the reward calculation.
    //!
    //! \return Amount paid per unit of magnitude per day in units of GRC.
    //!
    virtual double MagnitudeUnit() const = 0;

    //!
    //! \brief Get the time elapsed since the account's last research reward.
    //!
    //! \return Elapsed time in seconds.
    //!
    virtual int64_t AccrualAge() const = 0;

    //!
    //! \brief Get the number of days since the account's last research reward.
    //!
    //! \return Elapsed time in days.
    //!
    virtual double AccrualDays() const = 0;

    //!
    //! \brief Get the number of blocks since the account's last research reward.
    //!
    virtual int64_t AccrualBlockSpan() const = 0;

    //!
    //! \brief Get the average daily research payment over the lifetime of the
    //! account.
    //!
    //! \return Average research payment in units of 1/100000000 GRC.
    //!
    virtual CAmount PaymentPerDay() const = 0;

    //!
    //! \brief Get the average daily research payment limit of the account.
    //!
    //! \return Payment per day limit in units of 1/100000000 GRC.
    //!
    virtual CAmount PaymentPerDayLimit() const = 0;

    //!
    //! \brief Determine whether the account exceeded the daily payment limit.
    //!
    //! \return \c true if the average daily research payment amount exceeds
    //! the calculated daily payment limit of the account.
    //!
    virtual bool ExceededRecentPayments() const = 0;

    //!
    //! \brief Get the expected daily research payment for the account based on
    //! the current network payment conditions.
    //!
    //! \return Expected daily payment in units of 1/100000000 GRC.
    //!
    virtual CAmount ExpectedDaily() const = 0;

    //!
    //! \brief Get the pending research reward for the account without applying
    //! any limit rules.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    virtual CAmount RawAccrual() const = 0;

    //!
    //! \brief Get the pending research reward for the account as expected by
    //! the network.
    //!
    //! \return Pending payment in units of 1/100000000 GRC.
    //!
    virtual CAmount Accrual() const = 0;
};

//!
//! \brief Wraps an object that implements the IAccrualComputer interface.
//!
typedef std::unique_ptr<IAccrualComputer> AccrualComputer;
}
