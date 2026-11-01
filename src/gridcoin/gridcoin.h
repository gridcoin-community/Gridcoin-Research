// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_GRIDCOIN_H
#define GRIDCOIN_GRIDCOIN_H

#include "fwd.h"
#include "sync.h"

class CScheduler;
extern CCriticalSection cs_main;

namespace GRC {
//!
//! \brief Initialize Gridcoin-specific components and services.
//!
//! \param threads    Used to start Gridcoin threads.
//! \param pindexBest Block index for the tip of the chain.
//!
//! \return \c false if a problem occurs during initialization.
//!
bool Initialize(ThreadHandlerPtr threads, CBlockIndex* pindexBest) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

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

//!
//! \brief Function to provide for poll expiration/warning notification.
//!
void NotifyPoll();

//!
//! \brief Rebuild the beacon registry in-line during a runtime multi-SB reorg.
//!
//! Called by DisconnectBlocksBatch when a runtime reorg crosses two or more
//! superblock boundaries -- the case where BeaconRegistry::Deactivate cannot
//! resurrect expired pending beacons from prior SBs (m_expired_pending only
//! carries the latest SB's set, per beacon.cpp:1265-1273). Without a full
//! rebuild the in-memory registry would be left missing prior-SB
//! expired-pending beacons, and any subsequent ConnectBlock that referenced
//! one of those beacons (e.g., a research-reward claim) would
//! deterministically diverge from healthy peers -- a real fork window.
//!
//! Rebuild steps:
//!   1. Non-blocking informational message (Qt popup or stderr line) so the
//!      operator sees what is happening.
//!   2. BeaconRegistry::Reset() -- wipes the in-memory and on-disk beacon
//!      state.
//!   3. InitializeContracts(pindexBest) -- re-initializes the registries
//!      and walks ReplayContracts to re-apply beacon contracts from V11
//!      forward (the other contract types' DB-backed Initialize is
//!      idempotent and ReplayContracts skips already-covered contracts).
//!
//! Runs under cs_main (caller's lock) so no other thread observes the
//! intermediate empty-registry state. On SSD the rebuild typically
//! completes in a few seconds; the wallet is briefly unresponsive but
//! consistent. Issue #2865 / PR #2941.
//!
void RebuildBeaconRegistry() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
} // namespace GRC

#endif // GRIDCOIN_GRIDCOIN_H
