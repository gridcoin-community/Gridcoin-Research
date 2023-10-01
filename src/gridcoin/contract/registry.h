// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CONTRACT_REGISTRY_H
#define GRIDCOIN_CONTRACT_REGISTRY_H

#include "gridcoin/contract/payload.h"
#include "gridcoin/beacon.h"
#include "gridcoin/project.h"
#include "gridcoin/protocol.h"
#include "gridcoin/sidestake.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/voting/registry.h"

namespace GRC {

class RegistryBookmarks_error : public std::runtime_error
{
public:
    explicit RegistryBookmarks_error(const std::string& str)
        : std::runtime_error("ERROR: " + str)
    {
        LogPrintf("ERROR: %s", str);
    }
};

//!
//! \brief Registry is simply a mnemonic alias for IContractHandler.
//!
typedef IContractHandler Registry;

typedef std::unordered_map<ContractType, int> RegistryBlockHeights;

class RegistryBookmarks
{
public:
    static const std::vector<GRC::ContractType> CONTRACT_TYPES_WITH_REG_DB;

    static const std::vector<GRC::ContractType> CONTRACT_TYPES_SUPPORTING_REVERT;

    RegistryBookmarks()
    {
        UpdateRegistryBookmarks();
    }

    static Registry& GetRegistryWithDB(const ContractType type)
    {
        switch (type) {
        case ContractType::BEACON:      return GetBeaconRegistry();
        case ContractType::PROJECT:     return GetWhitelist();
        case ContractType::PROTOCOL:    return GetProtocolRegistry();
        case ContractType::SCRAPER:     return GetScraperRegistry();
        case ContractType::SIDESTAKE:   return GetSideStakeRegistry();
        case ContractType::UNKNOWN:
            [[fallthrough]];
        case ContractType::CLAIM:
            [[fallthrough]];
        case ContractType::MESSAGE:
            [[fallthrough]];
        case ContractType::POLL:
            [[fallthrough]];
        case ContractType::VOTE:
            [[fallthrough]];
        case ContractType::MRC:
            [[fallthrough]];
        case ContractType::OUT_OF_BOUND:
            break;
        }

        throw RegistryBookmarks_error("Contract type has no registry db.");
    }

    static Registry& GetRegistryWithRevert(const ContractType type)
    {
        switch (type) {
        case ContractType::BEACON:      return GetBeaconRegistry();
        case ContractType::POLL:        return GetPollRegistry();
        case ContractType::PROJECT:     return GetWhitelist();
        case ContractType::PROTOCOL:    return GetProtocolRegistry();
        case ContractType::SCRAPER:     return GetScraperRegistry();
        case ContractType::VOTE:        return GetPollRegistry();
        case ContractType::SIDESTAKE:   return GetSideStakeRegistry();
            [[fallthrough]];
        case ContractType::UNKNOWN:
            [[fallthrough]];
        case ContractType::CLAIM:
            [[fallthrough]];
        case ContractType::MESSAGE:
            [[fallthrough]];
        case ContractType::MRC:
            [[fallthrough]];
        case ContractType::OUT_OF_BOUND:
            break;
        }

        throw RegistryBookmarks_error("Contract type has no contract handler reversion capability.");
    }


    static bool IsRegistryBackedByDB(const ContractType& type)
    {
        auto iter = std::find(CONTRACT_TYPES_WITH_REG_DB.begin(),
                              CONTRACT_TYPES_WITH_REG_DB.end(), type);

        return (iter != CONTRACT_TYPES_WITH_REG_DB.end());
    }

    static bool IsRegistryRevertCapable(const ContractType& type)
    {
        auto iter = std::find(CONTRACT_TYPES_SUPPORTING_REVERT.begin(),
                              CONTRACT_TYPES_SUPPORTING_REVERT.end(), type);

        return (iter != CONTRACT_TYPES_SUPPORTING_REVERT.end());
    }

    std::optional<int> GetRegistryBlockHeight(const ContractType type) const
    {
        auto db_height_entry = m_db_heights.find(type);

        if (db_height_entry == m_db_heights.end()) {
            return std::nullopt;
        }

        return db_height_entry->second;
    }

    void UpdateRegistryBookmarks()
    {
        // We use array notation here, because we want the latest to override, and if one doesn't exist it will
        // be created.
        for (const auto& registry_type : CONTRACT_TYPES_WITH_REG_DB) {
            m_db_heights[registry_type] = GetRegistryWithDB(registry_type).GetDBHeight();
        }
    }

    //!
    //! \brief This method is used in the cleanup after disconnecting blocks in DisconnectBlocksBatch to reset the db
    //! bookmark heights. It will reset the DB block height AND bookmark for a registry if the new (head of chain)
    //! height is less than the recorded bookmark for that contract type.
    //!
    //! \param block_height.
    //!
    void UpdateRegistryBlockHeights(int& block_height)
    {
        for (const auto& registry_type : CONTRACT_TYPES_WITH_REG_DB) {
            if (GetRegistryBlockHeight(registry_type) > block_height) {
                GetRegistryWithDB(registry_type).SetDBHeight(block_height);

                m_db_heights[registry_type] = block_height;
            }
        }
    }

    int GetLowestRegistryBlockHeight()
    {
        int lowest_height = std::numeric_limits<int>::max();

        for (const auto& iter : m_db_heights) {
            if (iter.second < lowest_height) {
                lowest_height = iter.second;
            }
        }

        return lowest_height;
    }

private:
    RegistryBlockHeights m_db_heights;
};

} // GRC namespace
#endif // GRIDCOIN_CONTRACT_REGISTRY_H
