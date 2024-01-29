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
    ContractType::SIDESTAKE
};

const std::vector<GRC::ContractType> RegistryBookmarks::CONTRACT_TYPES_SUPPORTING_REVERT = {
    ContractType::BEACON,
    ContractType::POLL,
    ContractType::PROJECT,
    ContractType::PROTOCOL,
    ContractType::SCRAPER,
    ContractType::VOTE,
    ContractType::SIDESTAKE
};

} // namespace GRC
