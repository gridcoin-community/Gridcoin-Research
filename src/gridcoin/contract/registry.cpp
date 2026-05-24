// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/contract/registry.h"

namespace GRC {

const std::vector<GRC::ContractType> RegistryBookmarks::CONTRACT_TYPES_WITH_REG_DB = {
    ContractType::BEACON,
    ContractType::PROJECT,
    ContractType::PROTOCOL,
    ContractType::SCRAPER,
    ContractType::SIDESTAKE,
    // POOL_REGISTER and POOL_APPROVE share a single PoolRegistry / RegistryDB
    // instance (see GetRegistryWithDB in registry.h). Listing both types here
    // is deliberate: it keeps the bookmark map symmetric with the
    // ContractType enum so consumers iterating either type-key see consistent
    // GetRegistryBlockHeight results. The minor wart is that
    // InitializeContracts in gridcoin.cpp calls PoolRegistry::Initialize()
    // twice per startup; the second call is idempotent — RegistryDB caches
    // m_database_init after the first successful load so LoadDBHeight
    // short-circuits (registry_db.h:114-121) — but the historical entries
    // are still walked into the in-memory maps on each call. Functionally
    // safe (the walk is deterministic and the maps are overwritten with
    // identical state), just not free.
    ContractType::POOL_REGISTER,
    ContractType::POOL_APPROVE
};

const std::vector<GRC::ContractType> RegistryBookmarks::CONTRACT_TYPES_SUPPORTING_REVERT = {
    ContractType::BEACON,
    ContractType::POLL,
    ContractType::PROJECT,
    ContractType::PROTOCOL,
    ContractType::SCRAPER,
    ContractType::VOTE,
    ContractType::SIDESTAKE,
    ContractType::POOL_REGISTER,
    ContractType::POOL_APPROVE
};

} // namespace GRC
