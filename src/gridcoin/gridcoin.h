// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

class CBlockIndex;
class CScheduler;

namespace GRC {
//!
//! \brief Initialize Gridcoin-specific components and services.
//!
//! \param pindexBest Block index for the tip of the chain.
//!
//! \return \c false if a problem occurs during initialization.
//!
bool Initialize(CBlockIndex* pindexBest);

//!
//! \brief Set up Gridcoin-specific background jobs.
//!
//! \param scheduler Scheduler instance to register jobs with.
//!
void ScheduleBackgroundJobs(CScheduler& scheduler);
} // namespace GRC
