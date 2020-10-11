// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/accrual/computer.h"

namespace {
using namespace GRC;

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
    int64_t MaxReward() const override
    {
        return 0;
    }

    double MagnitudeUnit() const override
    {
        return 0.0;
    }

    int64_t AccrualAge() const override
    {
        return 0;
    }

    double AccrualDays() const override
    {
        return 0.0;
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
        return 0;
    }

    bool ExceededRecentPayments() const override
    {
        return false;
    }

    int64_t ExpectedDaily() const override
    {
        return 0;
    }

    int64_t RawAccrual() const override
    {
        return 0;
    }

    int64_t Accrual() const override
    {
        return 0;
    }
}; // NullAccrualComputer
} // anonymous namespace
