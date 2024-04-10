// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_GRIDCOIN_H
#define GRIDCOIN_GRIDCOIN_H

#include "fwd.h"

class CScheduler;

namespace GRC {
//!
//! \brief Initialize Gridcoin-specific components and services.
//!
//! \param threads    Used to start Gridcoin threads.
//! \param pindexBest Block index for the tip of the chain.
//!
//! \return \c false if a problem occurs during initialization.
//!
bool Initialize(ThreadHandlerPtr threads, CBlockIndex* pindexBest);

//!
//! \brief This closes the underlying research file to support the snapshot update
//! process, which must remove the accrual directory as part of the blockchain cleanup.
//!
void CloseResearcherRegistryFile();

//!
//! \brief Set up Gridcoin-specific background jobs.
//!
//! \param scheduler Scheduler instance to register jobs with.
//!
void ScheduleBackgroundJobs(CScheduler& scheduler);

//!
//! \brief Cleans the config file of obsolete config keys. Might not make changes
//! if a specific key is not present.
//!
//! \return \c true if no errors occurred.
//!
bool CleanConfig();

//!
//! \brief Function to allow cycling through DB passivation for all contract types backed
//! with registry db.
//!
void RunDBPassivation();
} // namespace GRC

#endif // GRIDCOIN_GRIDCOIN_H
